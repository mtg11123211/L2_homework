// Provided infrastructure. You do not need to modify this file.

#pragma once

#include "frame.hpp"

#include <filesystem>
#include <string>
#include <vector>

class FrameSource
{
public:
    virtual ~FrameSource() = default;
    virtual bool next(Frame &frame) = 0;
};

class ImageSequenceSource final : public FrameSource
{
public:
    explicit ImageSequenceSource(std::filesystem::path directory,
                                 int producer_delay_ms = 0);
    bool next(Frame &frame) override;

private:
    std::vector<std::filesystem::path> paths_;
    cv::Mat buffer_;
    std::size_t next_index_ = 0;
    int producer_delay_ms_ = 0;
};
