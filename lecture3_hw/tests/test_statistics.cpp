// Provided infrastructure. You do not need to modify this file.

#include "statistics.hpp"

#include <iostream>
#include <thread>
#include <vector>

int main()
{
    Statistics statistics;
    constexpr int thread_count = 8;
    constexpr int increments_per_thread = 50;
    std::vector<std::thread> threads;

    for (int i = 0; i < thread_count; ++i)
    {
        threads.emplace_back([&statistics]
                             {
            for (int j = 0; j < increments_per_thread; ++j) {
                statistics.onProcessed();
            } });
    }
    for (auto &thread : threads)
    {
        thread.join();
    }

    const int expected = thread_count * increments_per_thread;
    const int actual = statistics.snapshot().processed;
    if (actual != expected)
    {
        std::cerr << "FAIL: expected " << expected << ", got " << actual << '\n';
        return 1;
    }
    std::cout << "PASS: shared statistics are correct\n";
    return 0;
}
