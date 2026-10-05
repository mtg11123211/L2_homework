#pragma once

#include "blocking_queue.hpp"
#include "frame_source.hpp"
#include "image_processor.hpp"
#include "statistics.hpp"

#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

struct PipelineConfig
{
    int worker_count = 3;
    int worker_delay_ms = 8;
    std::filesystem::path output_directory = "output";
};

class Pipeline
{
public:
    Pipeline(std::unique_ptr<FrameSource> source, PipelineConfig config);
    ~Pipeline();

    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;

    void start();
    void wait();
    StatisticsSnapshot statistics() const;

private:
    void producerLoop();
    void workerLoop(int worker_id);

    std::unique_ptr<FrameSource> source_;
    PipelineConfig config_;
    BlockingQueue<Frame> queue_;
    ImageProcessor processor_;
    Statistics statistics_;
    std::thread producer_;
    std::vector<std::thread> workers_;
};
