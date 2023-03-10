#pragma once

#include <chrono>

namespace server_lib {
namespace tests {
    class cpu_profiler
    {
    public:
        cpu_profiler();

        void start();

        size_t us_elapsed() const;
        size_t ms_elapsed() const;
        size_t sec_elapsed() const;

        using time_type = std::chrono::time_point<std::chrono::steady_clock>;

        static time_type now();

    private:
        time_type _start;
    };
} // namespace tests
} // namespace server_lib
