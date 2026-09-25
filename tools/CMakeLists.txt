cmake_minimum_required(VERSION 3.16)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(OpenCV REQUIRED)

add_library(tools OBJECT 
    img_tools.cpp
    logger.cpp
)

# Ubuntu 22.04 的 spdlog/fmt 是编译库形式（非 header-only），
# 必须 PUBLIC 链接，让 main/opencv 等消费者自动继承
target_link_libraries(tools PUBLIC spdlog::spdlog fmt::fmt ${OpenCV_LIBS})
