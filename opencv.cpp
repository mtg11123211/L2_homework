// ============================================================================
// opencv.cpp —— 附加作业：识别 PDF 中的 OpenCV(AprilTag) 标志
//
// AprilTag 是一种类似二维码的方形基准标志（本题用 tag36h11 家族），
// 课件已给出识别器 AprilTagDetector，本文件负责"取图 -> 检测 -> 可视化"。
// 配置（configs/yolo.yaml 的 auto_charge 段）指定只保留 id 10/18/24，
// 识别到其它 id 会在识别器内部被过滤掉。
//
// 数据流：
//   Camera::read() -> BGR 图像
//   -> AprilTagDetector::detect() -> std::vector<TagDetection>
//      每个 TagDetection = { int id; 4 个角点 corners; 中心点 center }
//   -> draw_points 画绿色闭合四边形 + draw_text 标注 id -> imshow
// ============================================================================

#include <iostream>
#include <string>

#include "io/camera.hpp"
#include "opencv2/opencv.hpp"
#include "tasks/apriltag_detector.hpp"  // auto_charge::AprilTagDetector
#include "tools/img_tools.hpp"

int main()
{
  try {
    // ---- 初始化 -----------------------------------------------------------
    // 与作业二同一个 Camera 类：构造即开始取流。
    Camera cam;

    // AprilTag 识别器复用同一个 yaml（读取其中 auto_charge 段）：
    // tag_family=tag36h11、tag_ids=[10,18,24]、自适应阈值、亚像素角点精化等。
    auto_charge::AprilTagDetector detector("./configs/yolo.yaml");

    // ---- 主循环：与作业二结构完全一致，只是把 YOLO 换成 Tag 识别器 --------
    while (true) {
      cv::Mat img = cam.read();
      if (img.empty()) {
        std::cerr << "read frame timeout, retry..." << std::endl;
        continue;
      }

      // 识别器内部：BGR->灰度 -> cv::aruco::detectMarkers（自适应阈值找四边形、
      // 亚像素精化角点、解码出 id）-> 只保留 yaml 白名单里的 10/18/24。
      auto tags = detector.detect(img);

      for (const auto & tag : tags) {
        // 与作业二相同的绿色闭合四边形（4 角点，draw_points 自动首尾相接）
        tools::draw_points(img, tag.corners, {0, 255, 0});

        // 在标志中心位置标注红色 "id xx"。
        // draw_text 的形参是 cv::Point（整数坐标），tag.center 是 Point2f，
        // 这里显式构造 cv::Point 做类型转换；{0,0,255} 为 BGR 纯红。
        tools::draw_text(
          img, "id " + std::to_string(tag.id), cv::Point(tag.center), {0, 0, 255});
      }

      cv::imshow("img", img);
      if (cv::waitKey(1) == 'q') {
        break;
      }
    }
  } catch (const std::exception & e) {
    std::cerr << e.what() << std::endl;
    return -1;
  }

  return 0;
}
