#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

#include <opencv2/opencv.hpp>

#include "tasks/apriltag_detector.hpp"
#include "tools/img_tools.hpp"

// ============================================================
// 无相机验证：附加作业 AprilTag 管线离线版
//
// 用法（必须在 homework/ 目录下运行）：
//   ./opencv_offline          默认：PDF 真实标志照片 assets/samples/tags_36h11.jpg
//                             （tag36h11 的 id 10/18/24，带透视/划痕/光照不均）
//   ./opencv_offline --synth  程序生成标志动画（无畸变基准对照）
//
// 验证判据：
//   真实照片模式：终端打印检测到的 id 与四角坐标，画面绿色闭合框+id，
//                 保存 offline_tag_result.jpg
// ============================================================

static const std::string kDefaultTags = "assets/samples/tags_36h11.jpg";
static const std::string kSavePath = "offline_tag_result.jpg";

// --synth 模式：生成 3 个目标标志(10/18/24) + 1 个非目标(1，应被过滤)
static cv::Mat makeSynthFrame(int idx)
{
  auto dict = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11);
  const std::vector<int> ids = {10, 18, 24, 1};
  const cv::Point base[4] = {{120, 150}, {760, 150}, {380, 620}, {1020, 620}};

  cv::Mat img(1024, 1280, CV_8UC3, cv::Scalar(235, 235, 235));
  int shift = static_cast<int>(30 * std::sin(idx / 25.0));
  for (size_t i = 0; i < ids.size(); ++i) {
    cv::Mat marker, marker_bgr;
    cv::aruco::drawMarker(dict, ids[i], 220, marker, 1);
    cv::cvtColor(marker, marker_bgr, cv::COLOR_GRAY2BGR);
    cv::Point pos = base[i] + cv::Point(shift, shift);
    marker_bgr.copyTo(img(cv::Rect(pos.x, pos.y, 220, 220)));
    cv::putText(
      img, "id " + std::to_string(ids[i]), pos + cv::Point(0, -10),
      cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 2);
  }
  return img;
}

int main(int argc, char ** argv)
{
  std::filesystem::create_directories("logs");
  const bool synth = (argc > 1 && std::string(argv[1]) == "--synth");

  // 与评分文件 opencv.cpp 完全相同的识别器
  auto_charge::AprilTagDetector detector("./configs/yolo.yaml");

  cv::Mat real;
  if (!synth) {
    real = cv::imread(kDefaultTags);
    if (real.empty()) {
      std::cerr << "默认素材缺失: " << kDefaultTags << std::endl;
      return -1;
    }
    std::cout << "帧源: " << kDefaultTags << " (" << real.cols << "x" << real.rows
              << ")" << std::endl;
  } else {
    std::cout << "帧源: 合成标志动画" << std::endl;
  }

  int frame_count = 0;
  while (true) {
    cv::Mat img = synth ? makeSynthFrame(frame_count) : real.clone();

    auto tags = detector.detect(img);

    for (const auto & tag : tags) {
      tools::draw_points(img, tag.corners, {0, 255, 0});  // 绿色闭合四边形
      tools::draw_text(
        img, "id " + std::to_string(tag.id), cv::Point(tag.center),
        {0, 0, 255}, 1.0, 2);
    }

    cv::putText(
      img, "target detected: " + std::to_string(tags.size()) + " (expect 10/18/24)",
      cv::Point(30, 60), cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 120, 0), 2);

    if (frame_count == 0) {
      std::cout << "---- 首帧检测结果 ----" << std::endl;
      for (const auto & tag : tags) {
        std::cout << "  id=" << tag.id << " center=(" << tag.center.x << ", "
                  << tag.center.y << ") corners=";
        for (const auto & c : tag.corners) {
          std::cout << "(" << c.x << "," << c.y << ") ";
        }
        std::cout << std::endl;
      }
      std::cout << "共检测到 " << tags.size() << " 个目标 (配置目标 id: 10/18/24)"
                << std::endl;
      cv::imwrite(kSavePath, img);
      std::cout << "结果图已保存: " << kSavePath << std::endl;
    }

    cv::imshow("offline apriltag", img);
    if (cv::waitKey(30) == 'q') break;
    ++frame_count;
  }

  return 0;
}
