cmake_minimum_required(VERSION 3.16)

project(camera_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

find_package(OpenCV REQUIRED)

add_executable(camera_test
    main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/camera.cpp
)

# 头文件：io 目录（camera.hpp）+ 海康 SDK 头文件
target_include_directories(camera_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../io
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/hikrobot/include
    ${OpenCV_INCLUDE_DIRS}
)

# 按架构选择海康 SDK 动态库路径
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
  set(HIK_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../io/hikrobot/lib/amd64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
  set(HIK_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../io/hikrobot/lib/arm64)
else()
  message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# 把库路径写进 rpath，运行时免设 LD_LIBRARY_PATH
set(CMAKE_BUILD_RPATH "${HIK_LIB_DIR}")
set(CMAKE_INSTALL_RPATH "${HIK_LIB_DIR}")

target_link_libraries(camera_test
    ${HIK_LIB_DIR}/libMvCameraControl.so
    ${OpenCV_LIBS}
)
