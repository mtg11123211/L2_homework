// ============================================================================
// camera_test/main.cpp —— 作业一的独立验证程序（轻量工程，不依赖 OpenVINO）
//
// 作用：只验证 Camera 类本身（枚举/打开/配参/取流/Bayer 解码/析构），
// 把相机链路与 YOLO 解耦——这样在没装 OpenVINO、或者只想验证相机时，
// 可以单独快速构建运行。
// ============================================================================

#include <iostream>

#include "camera.hpp"  // 本工程 include 路径直接指向 io/ 目录（见 CMakeLists）

int main()
{
  try {
    // 一行完成：枚举 -> 创建句柄 -> 打开 -> 白平衡/曝光10ms/增益20/60fps
    //          -> 开始取流。若相机没插好会在此处抛出带 SDK 错误码的异常。
    Camera cam;
    std::cout << "Camera opened. Press 'q' to quit." << std::endl;

    // 连续取流循环（example.cpp 只取一帧，这里扩展为实时视频）
    while (true) {
      cv::Mat img = cam.read();  // 返回 BGR 三通道图；超时返回空 Mat
      if (img.empty()) {
        // 偶发超时：提示并重试，不退出
        std::cout << "read timeout, retry..." << std::endl;
        continue;
      }

      cv::imshow("camera", img);
      // waitKey(1) 驱动窗口刷新并检测按键；q 退出循环，
      // 退出 main 时 cam 离开作用域，析构函数自动 StopGrabbing/Close/Destroy。
      if (cv::waitKey(1) == 'q') break;
    }
  } catch (const std::exception & e) {
    // 初始化阶段任何一步失败（找不到相机/被占用/权限不足等）都会到这里
    std::cerr << e.what() << std::endl;
    return -1;
  }

  return 0;
}
