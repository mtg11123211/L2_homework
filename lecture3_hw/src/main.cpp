#include "frame_source.hpp"
#include "pipeline.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <opencv2/core/utility.hpp>

namespace
{
    struct Options
    {
        std::filesystem::path input = "assets";
        std::filesystem::path output = "output";
        int workers = 3;
        int producer_delay_ms = 0;
        int worker_delay_ms = 8;
    };

    Options parseOptions(int argc, char **argv)
    {
        const cv::String keys =
            "{help h usage ?  |      | show this help message }"
            "{input i         |assets| input image directory }"
            "{output o        |output| output image directory }"
            "{workers w       |3     | number of worker threads }"
            "{producer-delay  |0     | producer delay in milliseconds }"
            "{worker-delay    |8     | worker delay in milliseconds }";

        cv::CommandLineParser parser(argc, argv, keys);
        parser.about("Homework 3 - Mini Vision Pipeline");

        if (parser.has("help"))
        {
            parser.printMessage();
            std::exit(0);
        }

        Options options;
        options.input = parser.get<std::string>("input");
        options.output = parser.get<std::string>("output");
        options.workers = parser.get<int>("workers");
        options.producer_delay_ms = parser.get<int>("producer-delay");
        options.worker_delay_ms = parser.get<int>("worker-delay");

        if (!parser.check())
        {
            parser.printErrors();
            throw std::invalid_argument("invalid command-line arguments");
        }
        return options;
    }
}

int main(int argc, char **argv)
{
    try
    {
        const Options options = parseOptions(argc, argv);
        std::cout << "Mini Vision Pipeline\n====================\n\nWorkers: "
                  << options.workers << "\n\n";

        auto source = std::make_unique<ImageSequenceSource>(
            options.input, options.producer_delay_ms);
        Pipeline pipeline(std::move(source),
                          {options.workers, options.worker_delay_ms, options.output});
        pipeline.start();
        pipeline.wait();

        const auto stats = pipeline.statistics();
        std::cout << "\n--------------------------\n"
                  << "Produced : " << stats.produced << '\n'
                  << "Processed: " << stats.processed << '\n'
                  << "Saved    : " << stats.saved << '\n'
                  << "Corrupted: " << stats.corrupted << '\n'
                  << "--------------------------\n\n";

        const bool passed = stats.produced > 0 &&
                            stats.produced == stats.processed &&
                            stats.processed == stats.saved &&
                            stats.corrupted == 0;
        std::cout << (passed ? "PASS: all checks passed\n"
                             : "FAIL: pipeline checks failed\n");
        return passed ? 0 : 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 2;
    }
}
