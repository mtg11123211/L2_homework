cmake_minimum_required(VERSION 3.16)

project(mini_vision_pipeline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(Threads REQUIRED)

add_library(mini_vision_lib
  src/frame_source.cpp
  src/image_processor.cpp
  src/pipeline.cpp
  src/statistics.cpp
)
target_include_directories(mini_vision_lib PUBLIC include)
target_link_libraries(mini_vision_lib PUBLIC ${OpenCV_LIBS} Threads::Threads)
target_compile_options(mini_vision_lib PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

add_executable(mini_vision src/main.cpp)
target_link_libraries(mini_vision PRIVATE mini_vision_lib)

include(CTest)
if(BUILD_TESTING)
  function(homework_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE mini_vision_lib)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  endfunction()

  homework_test(frame_integrity_test tests/test_frame.cpp)
  homework_test(statistics_test tests/test_statistics.cpp)
  homework_test(pipeline_test tests/test_pipeline.cpp)
  homework_test(shutdown_test tests/test_shutdown.cpp)
endif()

