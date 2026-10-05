// Provided infrastructure. You do not need to modify this file.

#pragma once

#include "frame.hpp"

#include <opencv2/core.hpp>

class ImageProcessor
{
public:
    cv::Mat process(const Frame &frame) const;
};
