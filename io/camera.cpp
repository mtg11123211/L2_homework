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
  {PixelType_Gvsp_BayerGR8, cv::COLOR_BayerGR2BGR},
  {PixelType_Gvsp_BayerRG8, cv::COLOR_BayerRG2BGR},
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
  } else if (raw.stFrameInfo.enPixelType == PixelType_Gvsp_Mono8) {
    // 黑白相机输出 Mono8：没有色彩信息，复制成三通道即可（后续 YOLO 接口
    // 统一要 3 通道图），三个通道值相同，显示为灰度。
    cv::cvtColor(mono, bgr, cv::COLOR_GRAY2BGR);
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
  // 白平衡：设为连续自动。注意正式打装甲板时为了颜色稳定通常会改成手动，
  // 这里与 example.cpp 保持一致，作业重点不在调参。
  MV_CC_SetEnumValue(handle_, "BalanceWhiteAuto", MV_BALANCEWHITE_AUTO_CONTINUOUS);
  // 曝光切手动，否则自动曝光会让灯条一会儿过曝一会儿暗，无法稳定识别。
  MV_CC_SetEnumValue(handle_, "ExposureAuto", MV_EXPOSURE_AUTO_MODE_OFF);
  // 增益也切手动，再给固定值，避免噪声随增益飘。
  MV_CC_SetEnumValue(handle_, "GainAuto", MV_GAIN_MODE_OFF);
  // 曝光时间 10000 微秒 = 10 ms。注意与帧率的约束：曝光时间 × 帧率 < 1 秒，
  // 10ms × 60fps = 0.6 < 1，合法。现场画面暗调大、拖影调小，改完需重编译。
  MV_CC_SetFloatValue(handle_, "ExposureTime", 10000.0f);
  MV_CC_SetFloatValue(handle_, "Gain", 20.0f);  // 模拟增益 20，单位 dB，越大噪点越多
  MV_CC_SetFrameRate(handle_, 60.0f);           // 采集帧率 60 fps

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
  // 60fps 下每 16.7ms 就有一帧，100ms 等不到说明取流异常（USB 掉线/被抢占）。
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
