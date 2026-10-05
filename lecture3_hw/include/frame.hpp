// Provided infrastructure. You do not need to modify this file.

#pragma once

#include <cstdint>
#include <opencv2/core.hpp>

struct Frame
{
    int id = -1;
    cv::Mat image;
    std::uint64_t expected_checksum = 0;
};

// 64-bit FNV-1a checksum of the image's raw bytes
inline std::uint64_t checksum(const cv::Mat &image)
{
    if (image.empty() || !image.isContinuous())
    {
        return 0;
    }

    const auto bytes = image.total() * image.elemSize();
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::size_t i = 0; i < bytes; ++i)
    {
        hash ^= image.data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}
