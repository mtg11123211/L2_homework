// Provided infrastructure. You do not need to modify this file.

#include "frame_source.hpp"
#include "pipeline.hpp"

#include <filesystem>
#include <iostream>
#include <memory>

int main()
{
    const std::filesystem::path output = "test-output/pipeline";
    std::filesystem::remove_all(output);

    auto source = std::make_unique<ImageSequenceSource>("assets", 0);
    Pipeline pipeline(std::move(source), {4, 5, output});
    pipeline.start();
    pipeline.wait();

    const auto stats = pipeline.statistics();
    int output_count = 0;
    if (std::filesystem::is_directory(output))
    {
        for (const auto &entry : std::filesystem::directory_iterator(output))
        {
            output_count += entry.is_regular_file() ? 1 : 0;
        }
    }

    if (stats.produced != 20 || stats.processed != 20 ||
        stats.saved != 20 || stats.corrupted != 0 || output_count != 20)
    {
        std::cerr << "FAIL: produced=" << stats.produced
                  << " processed=" << stats.processed
                  << " saved=" << stats.saved
                  << " corrupted=" << stats.corrupted
                  << " files=" << output_count << '\n';
        return 1;
    }
    std::cout << "PASS: all frames processed exactly once\n";
    return 0;
}
