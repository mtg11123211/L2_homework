// Provided infrastructure. You do not need to modify this file.

#include "frame_source.hpp"

#include <iostream>

int main()
{
    ImageSequenceSource source("assets");
    Frame first;
    Frame second;
    if (!source.next(first) || !source.next(second))
    {
        std::cerr << "FAIL: expected at least two frames\n";
        return 1;
    }
    if (checksum(first.image) != first.expected_checksum)
    {
        std::cerr << "FAIL: an older frame changed when the source captured a new frame\n";
        return 1;
    }
    std::cout << "PASS: queued frame owns valid image data\n";
    return 0;
}
