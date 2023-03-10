#include <server_lib/platform_config.h>

#if defined(SERVER_LIB_PLATFORM_LINUX)

#include "tests_common.h"
#include "performance.h"

#include <server_lib/logging_helper.h>
#include <server_lib/spawn.h>
#include <server_lib/event_loop.h>
#include <ssl_helpers/hash.h>
#include <ssl_helpers/encoding.h>

#include <string>
#include <chrono>
#include <unistd.h>

namespace server_lib {
namespace tests {

    using namespace std::chrono_literals;

    BOOST_AUTO_TEST_SUITE(spawn_tests)

    BOOST_AUTO_TEST_CASE(spawn_pwd_check)
    {
        print_current_test_name();

        char path_buff[PATH_MAX] = { 0 };

        char* pwd_check = getcwd(path_buff, PATH_MAX);
        BOOST_REQUIRE(pwd_check != NULL);

        spawn pwd({ "pwd" }, true /*with shell*/);
        std::string pwd_result;
        BOOST_REQUIRE(std::getline(pwd.stdout, pwd_result));

        BOOST_CHECK_EQUAL(std::string(pwd_check), pwd_result);
    }

    BOOST_AUTO_TEST_CASE(spawn_env_check)
    {
        print_current_test_name();

        const char TEST_ENV[] = "SPAWN_TEST";
        const char TEST_STR[] = "spawn_env_check";

        spawn env({ "env" }, { { TEST_ENV, TEST_STR } }, true);

        std::string test_env(TEST_ENV);
        test_env += "=";
        test_env += TEST_STR;

        std::string env_result;
        bool found = false;
        while (std::getline(env.stdout, env_result))
        {
            if (env_result == test_env)
            {
                found = true;
                break;
            }
        }

        BOOST_REQUIRE(found);
    }

    BOOST_AUTO_TEST_CASE(spawn_output_check)
    {
        print_current_test_name();

        const char TEST_STR[] = "spawn_output_check";

        {
            spawn echo({ "echo", TEST_STR }, true);
            std::string echo_result;
            BOOST_REQUIRE(!std::getline(echo.stderr, echo_result));
            BOOST_REQUIRE(std::getline(echo.stdout, echo_result));

            BOOST_CHECK_EQUAL(std::string(TEST_STR), echo_result);
        }

        {
            std::string awk_cmd("BEGIN { print \"");
            awk_cmd += TEST_STR;
            awk_cmd += "\" > \"/dev/fd/2\" }";

            spawn echo({ "awk", awk_cmd }, true);
            std::string echo_result;
            BOOST_REQUIRE(!std::getline(echo.stdout, echo_result));
            BOOST_REQUIRE(std::getline(echo.stderr, echo_result));

            BOOST_CHECK_EQUAL(std::string(TEST_STR), echo_result);
        }
    }

    BOOST_AUTO_TEST_CASE(spawn_wait_check)
    {
        print_current_test_name();

        cpu_profiler wait_sec;
        spawn sleep({ "sleep", "1" }, true);

        sleep.wait();
        BOOST_CHECK_EQUAL(wait_sec.sec_elapsed(), 1);
    }

    BOOST_AUTO_TEST_CASE(spawn_async_wait_check)
    {
        print_current_test_name();

        spawn_ptr sleep = std::make_shared<spawn>(spawn::args_type { "sleep", "1" }, true);

        spawn_polling poll(sleep);

        BOOST_REQUIRE(!poll.empty());
        BOOST_REQUIRE_EQUAL(poll.count(), 1u);

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        server_lib::event_loop external_checker;

        external_checker.post([&]() {
                            LOG_INFO("Checking has started");

                            while (!poll.empty())
                            {
                                std::this_thread::sleep_for(1s);
                            }

                            // Finish test
                            std::unique_lock<std::mutex> lck(done_test_cond_guard);
                            done_test = true;
                            done_test_cond.notify_one();

                            LOG_INFO("Sleep has been finished");
                        })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(spawn_async_multy_wait_check)
    {
        print_current_test_name();

        const size_t spawn_count = 10;
        spawn_polling poll;
        std::vector<spawn_ptr> sleeps;
        sleeps.reserve(spawn_count);
        for (size_t ci = 0; ci < spawn_count; ++ci)
        {
            spawn_ptr sleep = std::make_shared<spawn>(spawn::args_type { "sleep", "1" }, true);

            LOG_INFO("Spawned #" << ci);
            poll.append(sleep);
            sleeps.push_back(sleep);
        }

        BOOST_REQUIRE(!poll.empty());
        BOOST_REQUIRE_EQUAL(poll.count(), spawn_count);

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        server_lib::event_loop external_checker;

        external_checker.post([&]() {
                            LOG_INFO("Checking has started");

                            while (!poll.empty())
                            {
                                std::this_thread::sleep_for(1s);
                            }

                            // Finish test
                            std::unique_lock<std::mutex> lck(done_test_cond_guard);
                            done_test = true;
                            done_test_cond.notify_one();

                            LOG_INFO("Sleep has been finished");
                        })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        for (const auto& sleep : sleeps)
        {
            BOOST_CHECK_EQUAL(sleep->returncode(), 0);
            BOOST_CHECK_EQUAL(sleep->sigcode(), 0);
        }
    }

    BOOST_AUTO_TEST_CASE(spawn_async_multy_aborted_check)
    {
        print_current_test_name();

        const size_t spawn_count = 10;
        auto poll_ptr = std::make_shared<spawn_polling>();
        std::vector<spawn_ptr> sleeps;
        sleeps.reserve(spawn_count);
        for (size_t ci = 0; ci < spawn_count; ++ci)
        {
            spawn_ptr sleep = std::make_shared<spawn>(spawn::args_type { "sleep", "1" }, true);

            LOG_INFO("Spawned #" << ci);
            poll_ptr->append(sleep);
            sleeps.push_back(sleep);
        }

        BOOST_REQUIRE(!poll_ptr->empty());
        BOOST_REQUIRE_EQUAL(poll_ptr->count(), spawn_count);

        poll_ptr.reset();

        for (const auto& sleep : sleeps)
        {
            BOOST_CHECK_EQUAL(sleep->returncode(), SIGKILL);
            BOOST_CHECK_EQUAL(sleep->sigcode(), SIGKILL);
        }
    }

    BOOST_AUTO_TEST_CASE(spawn_communication_check)
    {
        print_current_test_name();

        std::string bash_cmd("read -p \"Enter: \" TEST; echo $TEST");

        spawn bash({ "bash", "-c", bash_cmd }, true);

        const std::string input("spawn_communication_check");
        bash.communicate(input);

        std::string input_result;
        BOOST_REQUIRE(std::getline(bash.stdout, input_result));
        BOOST_REQUIRE_EQUAL(input_result, input);
    }

    BOOST_AUTO_TEST_CASE(spawn_stdin_check)
    {
        print_current_test_name();

        std::string bash_cmd("cat < /dev/stdin");

        spawn bash({ "bash", "-c", bash_cmd }, true);

        bash.stdin << "<html>" << '\n';
        bash.stdin << "<body>" << '\n';
        bash.stdin << "</body>" << '\n';
        bash.stdin << "</html>";
        bash.communicate();

        size_t lines = 0;
        std::string line;
        while (std::getline(bash.stdout, line))
        {
            ++lines;
        }

        BOOST_CHECK_EQUAL(lines, 4u);
    }

    BOOST_AUTO_TEST_CASE(spawn_bin_stdin_check)
    {
        print_current_test_name();

        std::string bash_cmd("cat < /dev/stdin");

        spawn bash({ "bash", "-c", bash_cmd }, true);

        auto bin_data = ssl_helpers::create_ripemd160("blablabla");

        bash.stdin << bin_data;
        bash.communicate();

        std::vector<char> buff;
        buff.resize(bin_data.size());

        BOOST_REQUIRE(!bash.stdout.eof());
        bash.stdout.read(buff.data(), buff.size());

        LOG_INFO(ssl_helpers::to_hex(std::string(buff.data(), buff.size())));

        BOOST_CHECK_EQUAL(ssl_helpers::to_hex(bin_data), ssl_helpers::to_hex(std::string(buff.data(), buff.size())));
    }

    BOOST_AUTO_TEST_CASE(spawn_suspended_check)
    {
        print_current_test_name();

        std::string bash_cmd("echo STEP1; sleep 1; echo STEP2; sleep 1; echo STEP3");

        spawn bash({ "bash", "-c", bash_cmd }, true);

        size_t lines = 0;
        std::string line;
        while (std::getline(bash.stdout, line))
        {
            LOG_INFO(lines << ": " << line);
            ++lines;
        }

        BOOST_CHECK_EQUAL(lines, 3u);
    }

    BOOST_AUTO_TEST_CASE(spawn_terminate_check)
    {
        print_current_test_name();

        std::string bash_cmd("echo STEP1; sleep 1; echo STEP2; sleep 1; echo STEP3");

        spawn bash({ "bash", "-c", bash_cmd }, true);

        std::string line;
        while (std::getline(bash.stdout, line))
        {
            bash.terminate();
        }

        LOG_INFO(bash.returncode());

        BOOST_CHECK_EQUAL(bash.sigcode(), SIGTERM);
        BOOST_CHECK_EQUAL(bash.sigcode(), bash.returncode());
    }

    BOOST_AUTO_TEST_CASE(spawn_kill_check)
    {
        print_current_test_name();

        std::string bash_cmd("echo STEP1; sleep 1; echo STEP2; sleep 1; echo STEP3");

        spawn bash({ "bash", "-c", bash_cmd }, true);

        std::string line;
        while (std::getline(bash.stdout, line))
        {
            bash.kill();
        }

        LOG_INFO(bash.returncode());

        BOOST_CHECK_EQUAL(bash.sigcode(), SIGKILL);
        BOOST_CHECK_EQUAL(bash.sigcode(), bash.returncode());
    }

    BOOST_AUTO_TEST_CASE(spawn_signal_check)
    {
        print_current_test_name();

        std::string bash_cmd("echo STEP1; sleep 1; echo STEP2; sleep 1; echo STEP3");

        spawn bash({ "bash", "-c", bash_cmd }, true);

        std::string line;
        while (std::getline(bash.stdout, line))
        {
            bash.send_signal(SIGINT);
        }

        LOG_INFO(bash.returncode());

        BOOST_CHECK_EQUAL(bash.sigcode(), SIGINT);
        BOOST_CHECK_EQUAL(bash.sigcode(), bash.returncode());
    }

    BOOST_AUTO_TEST_CASE(spawn_returncode_check)
    {
        print_current_test_name();

        std::string bash_cmd("exit 2");

        spawn bash({ "bash", "-c", bash_cmd }, true);

        BOOST_CHECK_EQUAL(bash.returncode(), -1);
        BOOST_CHECK_EQUAL(bash.sigcode(), -1);

        bash.wait();

        BOOST_CHECK_EQUAL(bash.returncode(), 2);
        BOOST_CHECK_EQUAL(bash.sigcode(), 0);
    }

    BOOST_AUTO_TEST_CASE(spawn_returncode_ok_check)
    {
        print_current_test_name();

        std::string bash_cmd("exit 0");

        spawn bash({ "bash", "-c", bash_cmd }, true);

        BOOST_CHECK_EQUAL(bash.returncode(), -1);
        BOOST_CHECK_EQUAL(bash.sigcode(), -1);

        bash.wait();

        BOOST_CHECK_EQUAL(bash.returncode(), 0);
        BOOST_CHECK_EQUAL(bash.sigcode(), 0);
    }

    BOOST_AUTO_TEST_CASE(spawn_multy_wait_check)
    {
        print_current_test_name();

        std::string bash_cmd("exit 0");

        spawn bash({ "bash", "-c", bash_cmd }, true);

        for (size_t ci = 0; ci < 10; ++ci)
        {
            BOOST_REQUIRE_NO_THROW(bash.poll());
            BOOST_REQUIRE_NO_THROW(bash.wait());
        }

        BOOST_CHECK_EQUAL(bash.returncode(), 0);
    }

    BOOST_AUTO_TEST_SUITE_END()
} // namespace tests
} // namespace server_lib

#endif // SERVER_LIB_PLATFORM_LINUX
