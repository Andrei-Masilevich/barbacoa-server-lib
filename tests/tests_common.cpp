#include "tests_common.h"

#include <boost/filesystem.hpp>

#include <fstream>
#include <sstream>

#include <cstring>
#include <set>

#include <server_lib/logging_helper.h>

namespace server_lib {
namespace tests {

    boost::filesystem::path create_binary_data_file(const size_t file_size)
    {
        BOOST_REQUIRE_GT(file_size, 0u);

        boost::filesystem::path temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
        std::ofstream output { temp.generic_string(), std::ofstream::binary };

        constexpr size_t BUFF_SZ = 1024;
        unsigned char buff[BUFF_SZ];
        size_t ci = 0;
        size_t total = 0;
        while (true)
        {
            std::memset(buff, 0, sizeof(buff));
            auto* pbuff_offset = buff;
            for (auto cj = 0; cj < BUFF_SZ / sizeof(float); ++cj)
            {
                float fl = 9999 + ci++;
                std::memcpy(pbuff_offset, &fl, sizeof(float));
                pbuff_offset += sizeof(float);
            }

            size_t to_write = std::min(sizeof(buff), file_size - total);
            output.write(reinterpret_cast<char*>(buff), to_write);
            total += to_write;
            if (total >= file_size)
                break;
        }

        return temp;
    }

    std::string current_test_name()
    {
        return boost::unit_test::framework::current_test_case().p_name;
    }

    void print_current_test_name()
    {
        static uint32_t test_counter = 0;

        std::stringstream ss;

        ss << "TEST (";
        ss << ++test_counter;
        ss << ") - [";
        ss << current_test_name();
        ss << "]";
        DUMP_STR(ss.str());
    }

    bool waiting_for_asynch_test(bool& done,
                                 std::condition_variable& done_cond,
                                 std::mutex& done_cond_guard,
                                 size_t sec_timeout)
    {
        std::unique_lock<std::mutex> lck(done_cond_guard);
        if (!done)
        {
            done_cond.wait_for(lck, std::chrono::seconds(sec_timeout), [&done]() {
                return done;
            });
        }
        return done;
    }

    using logger_type = server_lib::logger;

    // Prevent fouling for Boost memory leaks detector.
    std::unique_ptr<logger_type, void (*)(logger_type*)> _s_logger { nullptr, [](logger_type*) { logger_type::destroy(); } };

    void init_global_log()
    {
        _s_logger.reset(logger_type::get_instance());
        _s_logger->init_debug_log(false, true);
    }

    void restore_global_log()
    {
        // we should reset global objects and appenders
        _s_logger.reset();
        init_global_log();
    }
} // namespace tests
} // namespace server_lib
