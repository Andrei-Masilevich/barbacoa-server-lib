#include <server_lib/platform_config.h>

#if defined(SERVER_LIB_PLATFORM_LINUX)

#include "tests_common.h"
#include "performance.h"

#include <server_lib/logging_helper.h>
#include <server_lib/spawn.h>
#include <server_lib/event_loop.h>

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

        const char* pchild_argv[] = { "pwd", NULL /*end sign*/ };

        spawn ls(pchild_argv, true /*with shell*/);
        std::string pwd;
        BOOST_REQUIRE(std::getline(ls.stdout, pwd));

        BOOST_CHECK_EQUAL(std::string(pwd_check), pwd);
    }

    BOOST_AUTO_TEST_CASE(spawn_env_check)
    {
        print_current_test_name();

        const char TEST_ENV[] = "SPAWN_TEST";
        const char TEST_STR[] = "spawn_env_check";

        const char* pchild_argv[] = { "env", NULL };

        std::string test_env(TEST_ENV);
        test_env += "=";
        test_env += TEST_STR;

        const char* pchild_env[] = { test_env.c_str(), NULL };

        spawn env(pchild_argv, pchild_env, true);
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
            const char* pchild_argv[] = { "echo", TEST_STR, NULL };

            spawn echo(pchild_argv, true);
            std::string echo_result;
            BOOST_REQUIRE(!std::getline(echo.stderr, echo_result));
            BOOST_REQUIRE(std::getline(echo.stdout, echo_result));

            BOOST_CHECK_EQUAL(std::string(TEST_STR), echo_result);
        }

        {
            std::string awk_cmd("BEGIN { print \"");
            awk_cmd += TEST_STR;
            awk_cmd += "\" > \"/dev/fd/2\" }";

            const char* pchild_argv[] = { "awk", awk_cmd.c_str(), NULL };

            spawn echo(pchild_argv, true);
            std::string echo_result;
            BOOST_REQUIRE(!std::getline(echo.stdout, echo_result));
            BOOST_REQUIRE(std::getline(echo.stderr, echo_result));

            BOOST_CHECK_EQUAL(std::string(TEST_STR), echo_result);
        }
    }

    BOOST_AUTO_TEST_CASE(spawn_wait_check)
    {
        print_current_test_name();

        const char* pchild_argv[] = { "sleep", "1", NULL };
        cpu_profiler wait_sec;
        spawn sleep(pchild_argv, true);

        sleep.wait();
        BOOST_CHECK_EQUAL(wait_sec.sec_elapsed(), 1);
    }

    BOOST_AUTO_TEST_CASE(spawn_async_wait_check)
    {
        print_current_test_name();

        const char* pchild_argv[] = { "sleep", "1", NULL };
        spawn_ptr sleep = std::make_shared<spawn>(pchild_argv, true);

        spawn_waiting_pool pool;
        pool.addend(sleep);

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        server_lib::event_loop external_checker;

        external_checker.post([&]() {
                            LOG_INFO("Checking has started");

                            while (!pool.empty())
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

        spawn_waiting_pool pool;
        for (size_t ci = 0; ci < 10; ++ci)
        {
            const char* pchild_argv[] = { "sleep", "1", NULL };
            spawn_ptr sleep = std::make_shared<spawn>(pchild_argv, true);

            LOG_INFO("Spawned #" << ci);
            pool.addend(sleep);
        }

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        server_lib::event_loop external_checker;

        external_checker.post([&]() {
                            LOG_INFO("Checking has started");

                            while (!pool.empty())
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

    BOOST_AUTO_TEST_SUITE_END()
} // namespace tests
} // namespace server_lib

#endif // SERVER_LIB_PLATFORM_LINUX
