#include "tests_common.h"

#include <server_lib/logging_helper.h>
#include <ssl_helpers/utils.h>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <unistd.h>
#endif

#include <fstream>
#include <boost/filesystem.hpp>
#include <memory>

#define TEST_CONTEXT_MARKER "TEST -> "
#undef SRV_LOG_CONTEXT_
#define SRV_LOG_CONTEXT_ TEST_CONTEXT_MARKER

namespace server_lib {
namespace tests {

    class temporary_logger : public logger
    {
    public:
        temporary_logger() = default;

        ~temporary_logger()
        {
            close_log_file();
            if (!_temp_to_log.empty())
            {
                boost::filesystem::remove(_temp_to_log);
            }
            if (logger::check_instance())
            {
                restore_global_log();
            }
        }

        std::string create_temp_file(const std::string& test_name)
        {
            std::string file_name = boost::filesystem::unique_path().generic_string();
            file_name += '.';
            file_name += test_name;

            _temp_to_log = boost::filesystem::temp_directory_path() / file_name;
            return _temp_to_log.generic_string();
        }

        void create_log_file(const std::string& test_name)
        {
            auto tmp_file = create_temp_file(test_name);
            _tmp_file = std::unique_ptr<std::ofstream>(new std::ofstream(tmp_file.c_str()));
            std::cout.clear();
            std::cout.rdbuf(_tmp_file->rdbuf());
        }

        std::string close_log_file()
        {
            if (_tmp_file)
            {
                _tmp_file->close();
                _tmp_file.reset();
                std::cout.rdbuf(pcout_old_buf);
                return _temp_to_log.generic_string();
            }
            return {};
        }

    private:
        boost::filesystem::path _temp_to_log;
        std::unique_ptr<std::ofstream> _tmp_file;
        static std::streambuf* pcout_old_buf;
    };

    std::streambuf* temporary_logger::pcout_old_buf = std::cout.rdbuf();

#if defined(LOGGER_REFERENCE)
#undef LOGGER_REFERENCE
#define LOGGER_REFERENCE (*this)
#endif

    BOOST_FIXTURE_TEST_SUITE(logger_tests, temporary_logger)

    BOOST_AUTO_TEST_CASE(cli_logger_default_level_check)
    {
        print_current_test_name();

        init_cli_log();

        create_log_file(current_test_name());

        LOG_TRACE(current_test_name() << " message");
        LOG_DEBUG(current_test_name() << " message");
        LOG_INFO(current_test_name() << " message");
        LOG_WARN(current_test_name() << " message");
        LOG_ERROR(current_test_name() << " message");
        LOG_FATAL(current_test_name() << " message");

        std::ifstream input(close_log_file());

        size_t rows = 0;
        for (std::string line; std::getline(input, line); ++rows)
        {
            switch (rows)
            {
            case 0: {
                BOOST_REQUIRE(line.find("[trace]") != std::string::npos);
                break;
            }
            case 1: {
                BOOST_REQUIRE(line.find("[debug]") != std::string::npos);
                break;
            }
            default:;
            }
        }

        BOOST_REQUIRE_EQUAL(rows, 6);
    }

    BOOST_AUTO_TEST_CASE(cli_logger_short_format_check)
    {
        print_current_test_name();

        set_level(logger::level_info).set_details(logger::details_message_with_level).init_cli_log();

        create_log_file(current_test_name());

        static const std::string message = " message";
        LOG_TRACE(current_test_name() << message);
        LOG_DEBUG(current_test_name() << message);
        LOG_INFO(current_test_name() << message);
        LOG_WARN(current_test_name() << message);
        LOG_ERROR(current_test_name() << message);
        LOG_FATAL(current_test_name() << message);

        std::ifstream input(close_log_file());

        size_t rows = 0;
        for (std::string line; std::getline(input, line); ++rows)
        {
            switch (rows)
            {
            case 0: {
                BOOST_REQUIRE(line.find("[info]") != std::string::npos);
                break;
            }
            case 1: {
                BOOST_REQUIRE(line.find("[warning]") != std::string::npos);
                break;
            }
            case 2: {
                BOOST_REQUIRE(line.find("[error]") != std::string::npos);
                break;
            }
            case 3: {
                BOOST_REQUIRE(line.find("[fatal]") != std::string::npos);
                break;
            }
            default:;
            }
        }

        BOOST_REQUIRE_EQUAL(rows, 4);
    }

    BOOST_AUTO_TEST_CASE(cli_logger_all_details_check)
    {
        print_current_test_name();

        static const char* time_format = "%Y%m%d";

        set_level(logger::level_trace).set_details(logger::details_all).set_time_format(time_format).init_cli_log(false, false);

        create_log_file(current_test_name());

        using std::chrono::system_clock;
        auto now = system_clock::now();

        auto now_date = ssl_helpers::to_iso_string(system_clock::to_time_t(now), time_format, false);
        std::string app_name;
#if defined(SERVER_LIB_PLATFORM_LINUX)
        std::ifstream("/proc/self/comm") >> app_name;
#endif

        static const std::string message = " message";
        LOG_TRACE(current_test_name() << message);
        LOG_DEBUG(current_test_name() << message);
        LOG_INFO(current_test_name() << message);
        LOG_WARN(current_test_name() << message);
        LOG_ERROR(current_test_name() << message);
        LOG_FATAL(current_test_name() << message);

        std::ifstream input(close_log_file());

        size_t rows = 0;
        for (std::string line; std::getline(input, line); ++rows)
        {
            if (!app_name.empty())
            {
                BOOST_REQUIRE(line.find(app_name) == 0);
            }
            BOOST_REQUIRE(line.find(message) != std::string::npos);

            switch (rows)
            {
            case 0: {
                BOOST_REQUIRE(line.find("[trace]") != std::string::npos);
                break;
            }
            case 1: {
                BOOST_REQUIRE(line.find("[debug]") != std::string::npos);
                break;
            }
            case 2: {
                BOOST_REQUIRE(line.find("[info]") != std::string::npos);
                break;
            }
            case 3: {
                BOOST_REQUIRE(line.find("[warning]") != std::string::npos);
                break;
            }
            case 4: {
                BOOST_REQUIRE(line.find("[error]") != std::string::npos);
                break;
            }
            case 5: {
                BOOST_REQUIRE(line.find("[fatal]") != std::string::npos);
                break;
            }
            default:;
            }

            BOOST_REQUIRE(line.find(now_date) != std::string::npos);
            BOOST_REQUIRE(line.find(logger::log_message::trim_file_path(sizeof(__FILE__), __FILE__)) != std::string::npos);
        }

        BOOST_REQUIRE_EQUAL(rows, 6);
    }

    BOOST_AUTO_TEST_CASE(file_logger_level_check)
    {
        print_current_test_name();

        auto tmp_log = create_temp_file(current_test_name());

        set_level(logger::level_debug).init_file_log(tmp_log.c_str(), 1);

        LOG_TRACE(current_test_name() << " message");
        LOG_DEBUG(current_test_name() << " message");
        LOG_INFO(current_test_name() << " message");
        LOG_WARN(current_test_name() << " message");
        LOG_ERROR(current_test_name() << " message");
        LOG_FATAL(current_test_name() << " message");

        flush();

        std::ifstream input(tmp_log);

        size_t rows = 0;
        for (std::string line; std::getline(input, line); ++rows)
        {
            switch (rows)
            {
            case 0: {
                BOOST_REQUIRE(line.find("[debug]") != std::string::npos);
                break;
            }
            case 1: {
                BOOST_REQUIRE(line.find("[info]") != std::string::npos);
                break;
            }
            case 2: {
                BOOST_REQUIRE(line.find("[warning]") != std::string::npos);
                break;
            }
            case 3: {
                BOOST_REQUIRE(line.find("[error]") != std::string::npos);
                break;
            }
            case 4: {
                BOOST_REQUIRE(line.find("[fatal]") != std::string::npos);
                break;
            }
            default:;
            }
        }

        BOOST_REQUIRE_EQUAL(rows, 5);
    }

    BOOST_AUTO_TEST_CASE(file_logger_rotation_check)
    {
        print_current_test_name();

        auto tmp_log = create_temp_file(current_test_name());

        set_level(logger::level_debug).init_file_log(tmp_log.c_str(), 1);

        for (size_t ci = 0; ci < 100; ++ci)
        {
            LOG_TRACE(current_test_name() << " message");
            LOG_DEBUG(current_test_name() << " message");
            LOG_INFO(current_test_name() << " message");
            LOG_WARN(current_test_name() << " message");
            LOG_ERROR(current_test_name() << " message");
            LOG_FATAL(current_test_name() << " message");
        }

        flush();

        size_t log_size = static_cast<size_t>(boost::filesystem::file_size(tmp_log));

        BOOST_REQUIRE_GE(log_size, 1);
        BOOST_REQUIRE_LE(log_size, 1024);

        std::ifstream input(tmp_log);

        size_t rows = 0;
        for (std::string line; std::getline(input, line); ++rows)
        {
        }

        BOOST_REQUIRE_GE(rows, 1);
    }

    BOOST_AUTO_TEST_CASE(cli_alternative_check)
    {
        print_current_test_name();

        init_cli_log();

        create_log_file(current_test_name());

        LOG(TRACE) << current_test_name() << " message";
        LOG(DEBUG) << current_test_name() << " message";
        LOG(INFO) << current_test_name() << " message";
        LOG(WARNING) << current_test_name() << " message";
        LOG(ERROR) << current_test_name() << " message";
        LOG(FATAL) << current_test_name() << " message";

        LOGC(TRACE) << current_test_name() << " message";
        LOGC(DEBUG) << current_test_name() << " message";
        LOGC(INFO) << current_test_name() << " message";
        LOGC(WARNING) << current_test_name() << " message";
        LOGC(ERROR) << current_test_name() << " message";
        LOGC(FATAL) << current_test_name() << " message";

        LOG(WARN) << current_test_name() << " message";
        LOG(ERR) << current_test_name() << " message";

        std::ifstream input(close_log_file());

        size_t rows = 0;
        for (std::string line; std::getline(input, line); ++rows)
        {
            switch (rows)
            {
            case 0: {
                BOOST_REQUIRE(line.find("[trace]") != std::string::npos);
                BOOST_REQUIRE(line.find(TEST_CONTEXT_MARKER) == std::string::npos);
                break;
            }
            case 1: {
                BOOST_REQUIRE(line.find("[debug]") != std::string::npos);
                BOOST_REQUIRE(line.find(TEST_CONTEXT_MARKER) == std::string::npos);
                break;
            }
            case 6: {
                BOOST_REQUIRE(line.find("[trace]") != std::string::npos);
                BOOST_REQUIRE(line.find(TEST_CONTEXT_MARKER) != std::string::npos);
                break;
            }
            case 7: {
                BOOST_REQUIRE(line.find("[debug]") != std::string::npos);
                BOOST_REQUIRE(line.find(TEST_CONTEXT_MARKER) != std::string::npos);
                break;
            }
            case 12: {
                BOOST_REQUIRE(line.find("[warning]") != std::string::npos);
                BOOST_REQUIRE(line.find(TEST_CONTEXT_MARKER) == std::string::npos);
                break;
            }
            case 13: {
                BOOST_REQUIRE(line.find("[error]") != std::string::npos);
                BOOST_REQUIRE(line.find(TEST_CONTEXT_MARKER) == std::string::npos);
                break;
            }
            default:;
            }
        }

        BOOST_REQUIRE_EQUAL(rows, 6 + 6 + 2);
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
