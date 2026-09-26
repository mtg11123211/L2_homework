#include "camera.hpp"

// ============================================================================
// camera.cpp —— Camera 类的实现，对应 example.cpp 里的线性流程，改造成 RAII 类
//
// example.cpp 的原始流程（对比着看就懂封装映射关系）：
//   MV_CC_EnumDevices   -> 构造函数第 1 步：枚举 USB 相机
//   MV_CC_CreateHandle  -> 构造函数第 2 步：为相机创建操作句柄
//   MV_CC_OpenDevice    -> 构造函数第 3 步：打开设备
//   SetXXX 参数设置     -> 构造函数第 4 步：白平衡/曝光/增益/帧率
//   MV_CC_StartGrabbing -> 构造函数第 5 步：开始取流
//   MV_CC_GetImageBuffer-> read()：从 SDK 缓冲池取一帧
//   Bayer 解码          -> read()：原始马赛克数据 -> OpenCV BGR 图像
//   MV_CC_FreeImageBuffer-> read()：把缓冲区还给 SDK（必须配对！）
//   StopGrabbing/Close/Destroy -> 析构函数：按相反顺序释放
// ============================================================================

#include <iostream>
#include <stdexcept>  // std::runtime_error：构造失败时抛出，强制上层处理
#include <string>
#include <algorithm>  // std::max/std::min：白平衡目标值夹紧到相机允许范围
#include <unordered_map>  // Bayer 相位类型 -> OpenCV 转换码的映射表

// 海康机器视觉 SDK 主头文件。只在本 .cpp 中包含（头文件不暴露 SDK 细节）。
#include "MvCameraControl.h"

namespace
{
// ---------------------------------------------------------------------------
// 匿名命名空间：里面的函数只在本编译单元（camera.cpp）可见，不会与其他文件
// 的同名函数冲突，相当于 C 语言里加 static 的文件内私有函数。
// ---------------------------------------------------------------------------

// 海康 SDK 是 C 风格接口：每个函数返回 int 错误码，MV_OK(=0) 表示成功。
// 这个小工具统一做错误检查，失败时把"哪一步失败 + 错误码"组装成异常抛出，
// 避免在构造函数里重复写十几遍 if (ret != MV_OK)。
void check(int ret, const char * what)
{
  if (ret != MV_OK) {
    throw std::runtime_error(std::string("Camera: ") + what + " failed, error code = " +
                             std::to_string(ret));
  }
}

// 传感器原始像素格式 -> OpenCV 去马赛克（demosaic）颜色转换码。
// 彩色工业相机的 CMOS 表面覆盖 Bayer 滤色阵列，每个像素只感应 R/G/B 一种
// 颜色，出厂输出的是单通道"马赛克"图，必须插值还原成三通道彩色图。
// Bayer 有 GR/RG/GB/BG 四种起始相位，必须和相机实际输出一一对应，
// 用哈希表查表比写四层 if-else 清晰，时间复杂度 O(1)。
const std::unordered_map<MvGvspPixelType, cv::ColorConversionCodes> kBayerMap = {
  // 实测：本机（MV-CS016）固件枚举为 BayerRG8，但 CFA 实际排列对应 OpenCV 的
  // GR 约定——用 RG2BGR 白纸整体偏蓝、用 GR2BGR 呈中性灰（debug 四相位对照确认）。
  // 海康 Bayer 命名与 OpenCV 差一位，故 RG8 这里映射到 COLOR_BayerGR2BGR。
  {PixelType_Gvsp_BayerRG8, cv::COLOR_BayerGR2BGR},
  {PixelType_Gvsp_BayerGR8, cv::COLOR_BayerGR2BGR},
  {PixelType_Gvsp_BayerGB8, cv::COLOR_BayerGB2BGR},
  {PixelType_Gvsp_BayerBG8, cv::COLOR_BayerBG2BGR},
};

// 把一帧 SDK 原始数据解码成三通道 BGR 图像（对应 example.cpp 里的 transfer 函数）。
cv::Mat toBgr(const MV_FRAME_OUT & raw)
{
  // stFrameInfo 里带有这一帧的宽、高、像素格式（由相机端决定）。
  const int width = static_cast<int>(raw.stFrameInfo.nWidth);
  const int height = static_cast<int>(raw.stFrameInfo.nHeight);

  // 【零拷贝的关键】cv::Mat 构造函数的这个重载不分配新内存，它只创建一个
  // 图像"头部"，data 指针直接指向 SDK 内部缓冲区 pBufAddr：
  //   Mat(尺寸, 类型 CV_8UC1 单通道8位, 外部数据指针)
  // 好处：整幅图（几百万字节）一次都不复制。
  // 约束：mono 的生命期绝不能超过 SDK 缓冲区——所以下面马上做 cvtColor
  // 把结果复制到新分配的 bgr，函数返回后只使用 bgr。
  cv::Mat mono(cv::Size(width, height), CV_8UC1, raw.pBufAddr);
  cv::Mat bgr;

  // 在映射表里查这台相机的 Bayer 相位
  auto it = kBayerMap.find(raw.stFrameInfo.enPixelType);
  if (it != kBayerMap.end()) {
    // 彩色机：Bayer 单通道 -> BGR 三通道。
    // 用 2BGR 而不是 2RGB：OpenCV 的 imshow/imwrite 按 BGR 顺序解释内存，
    // 用 2RGB 会导致红蓝颜色互换（example.cpp 原图就有这个坑）。
    // cvtColor 输出尺寸/通道数与输入不同，OpenCV 内部为 bgr 重新分配内存，
    // 因此转换结束后 bgr 已经不再指向 SDK 缓冲区，可以安全带走。
    cv::cvtColor(mono, bgr, it->second);

    // ---- 软件压绿：硬件 Green 白平衡对本机输出无效，白纸实测 G 高约 10% ----
    // 对解码后的 BGR 用三通道查找表，只把 G 通道乘 kOutGreenScale（一次性建好）。
    constexpr float kOutGreenScale = 0.93f;  // 现场实测白纸 B/R≈82、G≈88 → 88×0.93≈82 配平
    static cv::Mat green_lut = [] {
      cv::Mat lut(1, 256, CV_8UC3);
      for (int i = 0; i < 256; ++i) {
        int g = static_cast<int>(i * kOutGreenScale);
        lut.at<cv::Vec3b>(0, i) = cv::Vec3b(cv::saturate_cast<uchar>(i),
                                            cv::saturate_cast<uchar>(g),
                                            cv::saturate_cast<uchar>(i));
      }
      return lut;
    }();
    cv::LUT(bgr, green_lut, bgr);
  } else if (raw.stFrameInfo.enPixelType == PixelType_Gvsp_Mono8) {
    // 黑白相机输出 Mono8：没有色彩信息，复制成三通道即可（后续 YOLO 接口
    // 统一要 3 通道图），三个通道值相同，显示为灰度。
    cv::cvtColor(mono, bgr, cv::COLOR_GRAY2BGR);
  } else if (raw.stFrameInfo.enPixelType == PixelType_Gvsp_BGR8_Packed) {
    // 兜底模式：相机固件直接输出已做 demosaic+颜色校正的 BGR8。
    // Mat 零拷贝包住后 clone 一份独立内存再返回（不能带走 SDK 缓冲区）。
    cv::Mat packed(cv::Size(width, height), CV_8UC3, raw.pBufAddr);
    bgr = packed.clone();
  } else if (raw.stFrameInfo.enPixelType == PixelType_Gvsp_RGB8_Packed) {
    // 相机直出 RGB8：转成 OpenCV 使用的 BGR 序。
    cv::Mat packed(cv::Size(width, height), CV_8UC3, raw.pBufAddr);
    cv::cvtColor(packed, bgr, cv::COLOR_RGB2BGR);
  } else {
    // 既不是四种 Bayer 也不是 Mono8（例如相机被设成了 YUV/RGB packed 格式）。
    // 直接 std::map::at 会抛难懂的 out_of_range，这里给出带像素类型值的明确
    // 错误信息，方便现场定位。
    throw std::runtime_error(
      std::string("Camera: unsupported pixel type ") +
      std::to_string(static_cast<unsigned int>(raw.stFrameInfo.enPixelType)));
  }
  return bgr;  // 返回值是独立内存，与 SDK 缓冲区彻底脱钩
}
}  // namespace

// ============================================================================
// 构造函数：一条完整的"开机流水线"。出错时必须把前面已申请的资源回滚，
// 这叫异常安全（exception safety），是本题封装相对 example.cpp 的核心提升。
// ============================================================================
Camera::Camera() : handle_(nullptr)  // 先置空，万一中途抛异常，析构函数能安全判断
{
  // ---- 第 1 步：枚举设备 ------------------------------------------------
  // MV_USB_DEVICE 表示只枚举 USB3 Vision 相机；千兆网相机用 MV_GIGE_DEVICE。
  // MV_CC_DEVICE_INFO_LIST 是出参，nDeviceNum 是找到的设备数量。
  MV_CC_DEVICE_INFO_LIST device_list;
  device_list.nDeviceNum = 0;  // SDK 要求调用前清零
  check(MV_CC_EnumDevices(MV_USB_DEVICE, &device_list), "EnumDevices");

  // 没插相机 / 没被虚拟机接管 / USB 权限不对，都会走到这里。
  if (device_list.nDeviceNum == 0) {
    throw std::runtime_error("Camera: no hikrobot USB device found");
  }

  // ---- 第 2 步：创建句柄 ------------------------------------------------
  // demo 简化处理：永远取列表里第 0 台。多相机场景应遍历 pDeviceInfo 按
  // 序列号(SN)匹配指定相机。句柄是后续所有 SDK 调用的"相机身份证"。
  check(MV_CC_CreateHandle(&handle_, device_list.pDeviceInfo[0]), "CreateHandle");

  // ---- 第 3 步：打开设备 ------------------------------------------------
  // 用 try/catch 是因为：如果 OpenDevice 失败，第 2 步创建的句柄仍然有效，
  // 必须立刻 DestroyHandle 回收，否则资源泄漏。成功则继续往下走。
  try {
    check(MV_CC_OpenDevice(handle_), "OpenDevice");
  } catch (...) {
    MV_CC_DestroyHandle(handle_);
    handle_ = nullptr;  // 保证析构函数看到的是空，不会重复释放
    throw;              // 异常继续向上抛给 main，让程序报错退出
  }

  // ---- 第 4 步：配置成像参数（GenICam 标准节点，名字可在 MVS 客户端查）----
  // 曝光/增益切手动，避免灯条一会儿过曝一会儿暗；白平衡在第 6 步取流后固化。

  // 曝光切手动，否则自动曝光会让灯条一会儿过曝一会儿暗，无法稳定识别。
  MV_CC_SetEnumValue(handle_, "ExposureAuto", MV_EXPOSURE_AUTO_MODE_OFF);
  // 增益也切手动，再给固定值，避免噪声随增益飘。
  MV_CC_SetEnumValue(handle_, "GainAuto", MV_GAIN_MODE_OFF);
  // 曝光时间（微秒）。约束：曝光(μs) × 帧率(fps) < 1,000,000。
  // 现场要亮：20000μs=20ms，配 45fps → 20ms×45=0.9s 合法，亮度是原 10ms 的两倍。
  // 运动模糊敏感时降到 5000μs、帧率提回 60、增益加到 25dB。改完需重编译。
  MV_CC_SetFloatValue(handle_, "ExposureTime", 20000.0f);
  MV_CC_SetFloatValue(handle_, "Gain", 20.0f);  // 模拟增益，单位 dB，越大噪点越多
  MV_CC_SetFrameRate(handle_, 45.0f);           // 采集帧率 45 fps（配合 20ms 长曝光）

  // ---- 第 5 步：开始取流 -----------------------------------------------
  // 同样做失败回滚：StartGrabbing 失败时，设备处于已打开状态，
  // 必须 CloseDevice + DestroyHandle 完整释放后再抛异常。
  try {
    check(MV_CC_StartGrabbing(handle_), "StartGrabbing");
  } catch (...) {
    MV_CC_CloseDevice(handle_);
    MV_CC_DestroyHandle(handle_);
    handle_ = nullptr;
    throw;
  }

  // ---- 第 6 步：固化白平衡（开物馆灯光，2026-09-26 对白纸标定）-----------
  // 开机直接下发固定值，不再做 ONCE 自动标定，启动无需找纸、颜色稳定可复现。
  // 重新标定（换灯光场地时）：把本块临时换成 BalanceWhiteAuto=ONCE + sleep 2s
  // 对白纸跑一次，抄打印的 R/B 值替换下面两个常量（git 历史里有 ONCE 版本）。
  // 注意：该机型硬件 Green 增益对最终输出无效（demosaic 会重建绿色通道），
  // 残余约 10% 偏绿在 toBgr() 里用软件 kOutGreenScale 压掉，此处不动 G。
  constexpr unsigned int kWbRed = 1887;   // 自动标定 1685 × 1.12（宁红）
  constexpr unsigned int kWbBlue = 1843;  // 自动标定 1941 × 0.95（压蓝）
  if (MV_CC_SetEnumValue(handle_, "BalanceWhiteAuto", MV_BALANCEWHITE_AUTO_OFF) != MV_OK) {
    std::cerr << "[Camera] warn: BalanceWhiteAuto OFF failed, ignored" << std::endl;
  }
  // 固定值夹紧到该通道允许范围后下发，换同型号相机也不会越界。
  auto safe_set = [this](unsigned int value,
                         int (*getter)(void *, MVCC_INTVALUE *),
                         int (*setter)(void *, unsigned int), const char * name) {
    MVCC_INTVALUE range{};
    unsigned int target = value;
    if (getter(handle_, &range) == MV_OK && range.nMax > 0) {
      target = std::max(range.nMin, std::min(range.nMax, target));
    }
    int rc = setter(handle_, target);
    std::cout << "[Camera] WB " << name << " = " << target << ", ret=" << rc << std::endl;
  };
  safe_set(kWbRed, MV_CC_GetBalanceRatioRed, MV_CC_SetBalanceRatioRed, "R");
  safe_set(kWbBlue, MV_CC_GetBalanceRatioBlue, MV_CC_SetBalanceRatioBlue, "B");

  // 走到这里：相机已在后台持续往 SDK 缓冲池送帧，构造完成。
}

// ============================================================================
// 析构函数：对象离开作用域（main 结束 / 异常展开）时自动调用。
// 释放顺序与申请顺序严格相反：停流 -> 关设备 -> 销毁句柄。
// ============================================================================
Camera::~Camera()
{
  if (handle_ == nullptr) return;  // 构造中途失败已回滚过的对象，直接跳过

  MV_CC_StopGrabbing(handle_);  // 停止后台采集
  MV_CC_CloseDevice(handle_);   // 关闭设备，释放独占占用，别的程序才能再打开
  MV_CC_DestroyHandle(handle_);  // 销毁句柄本身
  handle_ = nullptr;
}

// ============================================================================
// read()：取一帧 -> 解码成 BGR -> 归还缓冲 -> 返回。
// 可以在主循环里反复调用，这是类相对 example.cpp"只读一帧"最大的实用提升。
// ============================================================================
cv::Mat Camera::read()
{
  MV_FRAME_OUT raw;          // 出参：指向一帧数据及其帧信息
  raw.pBufAddr = nullptr;

  // 从 SDK 内部缓冲池取一帧，第 3 个参数是超时时间 100ms：
  // 45fps 下约每 22ms 一帧，100ms 等不到说明取流异常（USB 掉线/被抢占）。
  // 这里不抛异常而返回空 Mat：偶发丢帧在实时系统里是正常的，上层重试即可。
  if (MV_CC_GetImageBuffer(handle_, &raw, 100) != MV_OK) {
    return cv::Mat();
  }

  cv::Mat bgr;
  try {
    bgr = toBgr(raw);  // Bayer/Mono 解码（理论上格式不支持才会抛）
  } catch (const std::exception & e) {
    std::cerr << e.what() << std::endl;  // 打印但不崩，本帧返回空、下帧继续
  }

  // 【极易遗漏的一步】GetImageBuffer 和 FreeImageBuffer 必须严格配对：
  // 取出的帧占用着 SDK 缓冲池的一个槽位，不归还，池子很快耗尽，
  // 之后 GetImageBuffer 永远超时——这是新手写相机程序最常见的"跑一会就卡"。
  // 放在解码之后、return 之前无条件调用，保证任何路径下缓冲区都还得回去。
  MV_CC_FreeImageBuffer(handle_, &raw);

  return bgr;  // bgr 是 cvtColor 新分配的内存，归还缓冲后依然有效
}
