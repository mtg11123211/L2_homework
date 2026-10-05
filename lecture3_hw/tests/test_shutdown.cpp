// Provided infrastructure. You do not need to modify this file.

#include "frame_source.hpp"
#include "pipeline.hpp"

#include <filesystem>
#include <memory>

int main()
{
    const std::filesystem::path output = "test-output/shutdown";
    std::filesystem::remove_all(output);
    {
        auto source = std::make_unique<ImageSequenceSource>("assets", 1);
        Pipeline pipeline(std::move(source), {3, 2, output});
        pipeline.start();
        // Deliberately do not call wait(). Destruction must still be safe.
    }
    return 0;
}
