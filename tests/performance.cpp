#include "performance.h"

namespace server_lib {
namespace tests {

    cpu_profiler::cpu_profiler()
    {
        start();
    }

    cpu_profiler::time_type cpu_profiler::now()
    {
        return std::chrono::steady_clock::now();
    }

    void cpu_profiler::start()
    {
        _start = std::chrono::steady_clock::now();
    }

    size_t cpu_profiler::us_elapsed() const
    {
        return static_cast<size_t>(std::chrono::duration_cast<std::chrono::microseconds>(now() - _start).count());
    }

    size_t cpu_profiler::ms_elapsed() const
    {
        return static_cast<size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now() - _start).count());
    }

    size_t cpu_profiler::sec_elapsed() const
    {
        return static_cast<size_t>(std::chrono::duration_cast<std::chrono::seconds>(now() - _start).count());
    }

} // namespace tests
} // namespace server_lib
