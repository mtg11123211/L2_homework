cmake_minimum_required(VERSION 3.16)

# 无相机离线模拟工程：不链接海康 SDK、不依赖 libusb
# 直接复用 homework 里 tasks/ 和 tools/ 的源码
project(offline_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(OpenCV REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(Eigen3 REQUIRED)
set(OpenVINO_DIR "/opt/intel/openvino_2024.6.0/runtime/cmake/")
find_package(OpenVINO REQUIRED COMPONENTS Runtime)

set(HW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# 作业二离线版：YOLO（视频/图片/合成帧 -> 识别 -> 绿色闭合框 -> 显示）
add_executable(main_offline
    main_offline.cpp
    ${HW_DIR}/tasks/armor.cpp
    ${HW_DIR}/tasks/yolo.cpp
    ${HW_DIR}/tasks/yolos/yolov5.cpp
    ${HW_DIR}/tools/img_tools.cpp
    ${HW_DIR}/tools/logger.cpp
)
target_include_directories(main_offline PRIVATE
    ${HW_DIR}
    ${EIGEN3_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(main_offline PRIVATE
    openvino::runtime fmt::fmt spdlog::spdlog yaml-cpp ${OpenCV_LIBS} pthread
)

# 附加作业离线版：程序生成 tag36h11 标志 -> AprilTag 识别器 -> 绿框+id
add_executable(opencv_offline
    opencv_offline.cpp
    ${HW_DIR}/tasks/apriltag_detector.cpp
    ${HW_DIR}/tools/img_tools.cpp
    ${HW_DIR}/tools/logger.cpp
)
target_include_directories(opencv_offline PRIVATE
    ${HW_DIR}
    ${EIGEN3_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(opencv_offline PRIVATE
    fmt::fmt spdlog::spdlog yaml-cpp ${OpenCV_LIBS} pthread
)
