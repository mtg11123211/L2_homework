// ============================================================================
// main.cpp —— 作业二：读取相机 -> YOLO 识别装甲板 -> 绿色闭合矩形可视化
//
// 对照 PDF 的三条要求：
//   要求1：用 Camera 类读相机图像，再用 yolo 类识别装甲板，最后显示结果
//   要求2：把装甲板四个关键点头尾相接连成闭合矩形，颜色必须是绿色
//   要求3：前往开物馆大厅北侧灰色集装箱完成实机测试
//
// 整体数据流：
//   海康相机 --Camera::read()--> cv::Mat(BGR)
//          --YOLO::detect()--> std::list<Armor>（每个 Armor 含 4 个关键点）
//          --tools::draw_points()--> 在原图上画绿色闭合四边形
//          --cv::imshow()--> 实时窗口
// ============================================================================

#include <iostream>

#include "io/camera.hpp"       // 作业一封装好的相机类
#include "opencv2/opencv.hpp"  // imshow / waitKey
#include "tasks/yolo.hpp"      // YOLO 识别器（内部按 yaml 选择 yolov5 实现）
#include "tools/img_tools.hpp"  // draw_points：把点集头尾相接连成闭合多边形

int main()
{
  // 整个流程用 try/catch 包住：相机打不开、模型文件缺失等都是"致命错误"，
  // 底层以异常形式上报，这里统一打印错误信息并以非 0 码退出。
  try {
    // ---- 初始化阶段（只做一次）------------------------------------------
    // Camera 构造即完成 枚举->打开->曝光/增益配置->开始取流（见 camera.cpp）
    Camera cam;

    // YOLO 构造参数是配置文件路径。yaml 里指定了：
    //   模型文件 assets/yolov5.xml（OpenVINO IR，同目录 .bin 自动加载）、
    //   运行设备 CPU、置信度阈值、ROI 等。
    // 路径写相对路径 "./configs/yolo.yaml"，所以运行程序时工作目录(cwd)
    // 必须在 homework/ 下，这也是所有演示命令都先 cd 到 homework 的原因。
    auto_aim::YOLO yolo("./configs/yolo.yaml");

    int frame_count = 0;  // 帧号，传给 detect 用于调试绘制与可能的时序逻辑

    // ---- 实时处理主循环 --------------------------------------------------
    while (true) {
      // 1) 取一帧 BGR 图像；失败（超时）返回空 Mat
      cv::Mat img = cam.read();
      if (img.empty()) {
        // 偶发丢帧/超时不是致命错误：打印提示后 continue 继续等下一帧，
        // 绝不能因为一帧超时就退出整个程序。
        std::cerr << "read frame timeout, retry..." << std::endl;
        continue;
      }

      // 2) YOLO 推理 + 后处理：
      //    预处理（letterbox 到 640x640）-> OpenVINO 推理 ->
      //    解析输出 [1,25200,22]（框、关键点、颜色、数字）->
      //    置信度/类型过滤，返回本帧所有装甲板，类型 std::list<Armor>。
      //    frame_count++ 用后置递增：本帧传旧值，之后再加一。
      auto armors = yolo.detect(img, frame_count++);

      // 3) 可视化（PDF 要求2 的核心）：
      //    Armor::points 是 4 个 cv::Point2f，顺序为左上/左下/右下/右上；
      //    draw_points 内部用 drawContours 把点按顺序连线并自动首尾闭合，
      //    颜色 {0, 255, 0} 是 OpenCV BGR 顺序下的纯绿色。
      //    用引用遍历避免拷贝 list 元素；不修改 armor 所以加 const。
      for (const auto & armor : armors) {
        tools::draw_points(img, armor.points, {0, 255, 0});
      }

      // 4) 显示。窗口名 "img" 与 PDF 效果图一致。
      cv::imshow("img", img);

      // waitKey(1) 必不可少：它既是 GUI 事件循环（没有它窗口会"未响应"），
      // 又承担"延时 1ms 并返回按键"的作用。按 q 跳出循环结束程序，
      // cam 随即析构，自动停流关相机。
      if (cv::waitKey(1) == 'q') {
        break;
      }
    }
  } catch (const std::exception & e) {
    // 捕获相机/模型初始化的致命异常，what() 里带有具体步骤和 SDK 错误码。
    std::cerr << e.what() << std::endl;
    return -1;  // 非零返回值：脚本/管理员可据此判断程序是异常退出
  }

  return 0;  // 正常退出
}
