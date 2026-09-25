#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

#include <opencv2/opencv.hpp>

#include "tasks/armor.hpp"
#include "tasks/yolo.hpp"
#include "tools/img_tools.hpp"

// ============================================================
// 无相机验证：作业二 YOLO 管线离线版（帧源由 PDF 课件素材驱动）
//
// 用法（必须在 homework/ 目录下运行）：
//   ./main_offline                  默认：真实装甲板素材 assets/samples/armor_blue4.png
//   ./main_offline 图片/视频路径     自定义图片(.jpg/.png)或视频(.mp4)
//   ./main_offline --synth          合成灯条动画（只验证链路不断、测 FPS）
//
// 验证判据：
//   终端打印每个装甲板的 color/name/type/confidence；
//   画面装甲板上出现绿色四点闭合框；首帧保存 offline_yolo_result.jpg
// ============================================================

static const std::string kDefaultArmor = "assets/samples/armor_blue4.png";
static const std::string kSavePath = "offline_yolo_result.jpg";

static bool isImage(const std::string & path)
{
  static const std::string exts[] = {".jpg", ".jpeg", ".png", ".bmp"};
  for (const auto & e : exts) {
    if (path.size() >= e.size() &&
        path.compare(path.size() - e.size(), e.size(), e) == 0) {
      return true;
    }
  }
  return false;
}

// 合成帧：深色背景 + 红蓝竖条（模拟灯条），真实神经网络不会把它识别为装甲板
static cv::Mat makeSynthFrame(int idx)
{
  cv::Mat img(799, 1003, CV_8UC3, cv::Scalar(25, 25, 25));
  int x = 120 + (idx * 8) % 650;
  cv::rectangle(img, cv::Rect(x, 280, 30, 180), cv::Scalar(0, 0, 255), -1);
  cv::rectangle(img, cv::Rect(x + 220, 280, 30, 180), cv::Scalar(255, 0, 0), -1);
  cv::putText(
    img, "OFFLINE SIMULATION (synthetic, armors expected 0)", cv::Point(30, 50),
    cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
  return img;
}

int main(int argc, char ** argv)
{
  std::filesystem::create_directories("logs");

  const std::string arg = (argc > 1) ? argv[1] : "";
  const bool synth = (arg == "--synth");

  cv::VideoCapture cap;
  cv::Mat still;
  enum Mode { REAL_IMAGE, VIDEO, SYNTH } mode = REAL_IMAGE;
  std::string source_desc = kDefaultArmor;

  if (synth) {
    mode = SYNTH;
    source_desc = "synthetic frames";
  } else if (!arg.empty()) {
    if (isImage(arg)) {
      still = cv::imread(arg);
      if (still.empty()) {
        std::cerr << "无法读取图片: " << arg << std::endl;
        return -1;
      }
      mode = REAL_IMAGE;
      source_desc = arg;
    } else {
      if (!cap.open(arg)) {
        std::cerr << "无法打开视频: " << arg << std::endl;
        return -1;
      }
      mode = VIDEO;
      source_desc = arg;
    }
  } else {
    still = cv::imread(kDefaultArmor);
    if (still.empty()) {
      std::cerr << "默认素材缺失: " << kDefaultArmor << std::endl;
      return -1;
    }
  }
  std::cout << "帧源: " << source_desc << std::endl;

  try {
    // 与评分文件 main.cpp 完全相同的识别器
    auto_aim::YOLO yolo("./configs/yolo.yaml");
    std::cout << "YOLO 模型加载成功，开始推理..." << std::endl;

    int frame_count = 0;
    auto t0 = std::chrono::steady_clock::now();

    while (true) {
      cv::Mat img;
      if (mode == SYNTH) {
        img = makeSynthFrame(frame_count);
      } else if (mode == REAL_IMAGE) {
        img = still.clone();  // 静态图反复推理，模拟连续取流
      } else {
        cap >> img;
        if (img.empty()) break;
      }

      // 与作业二相同的检测
      auto armors = yolo.detect(img, frame_count);

      // 绿色四点闭合框 + 类别文字（文字不改变作业要求的绿框）
      for (const auto & a : armors) {
        tools::draw_points(img, a.points, {0, 255, 0});
        std::string label =
          auto_aim::COLORS[a.color] + " " + auto_aim::ARMOR_NAMES[a.name] +
          " " + auto_aim::ARMOR_TYPES[a.type] +
          " " + std::to_string(a.confidence).substr(0, 4);
        tools::draw_text(
          img, label, cv::Point(a.points[0]), {0, 255, 0}, 0.6, 2);
      }

      cv::putText(
        img, "armors: " + std::to_string(armors.size()), cv::Point(30, 90),
        cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 255, 0), 2);

      if (frame_count == 0) {
        // 首帧：终端打印结构化结果 + 落盘（无显示器也能核对）
        std::cout << "---- 首帧检测结果 ----" << std::endl;
        for (const auto & a : armors) {
          std::cout << "  color=" << auto_aim::COLORS[a.color]
                    << " name=" << auto_aim::ARMOR_NAMES[a.name]
                    << " type=" << auto_aim::ARMOR_TYPES[a.type]
                    << " conf=" << a.confidence << std::endl;
        }
        std::cout << "共 " << armors.size() << " 个装甲板" << std::endl;
        cv::imwrite(kSavePath, img);
        std::cout << "结果图已保存: " << kSavePath << std::endl;
      }

      cv::imshow("offline yolo", img);
      if (cv::waitKey(1) == 'q') break;

      ++frame_count;
      if (frame_count % 50 == 0) {
        auto dt = std::chrono::steady_clock::now() - t0;
        std::cout << "已处理 " << frame_count << " 帧, "
                  << 50.0 / std::chrono::duration<double>(dt).count() << " FPS"
                  << std::endl;
        t0 = std::chrono::steady_clock::now();
      }
    }
  } catch (const std::exception & e) {
    std::cerr << e.what() << std::endl;
    return -1;
  }

  return 0;
}
