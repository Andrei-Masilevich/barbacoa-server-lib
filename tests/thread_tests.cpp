#include "tests_common.h"

#include <server_lib/thread_local_storage.h>
#include <server_lib/logging_helper.h>

#include <thread>
#include <sstream>
#include <cstring>
#include <mutex>

namespace server_lib {
namespace tests {

    BOOST_AUTO_TEST_SUITE(thread_tests)

    BOOST_AUTO_TEST_CASE(local_storage_separation_by_thead_check)
    {
        print_current_test_name();

        constexpr size_t STORAGE_SIZE = 1024;
        thread_local_storage storage(1);
        std::mutex test_checks_mutex;
        auto thread_payload = [&]() {
            for (size_t ci = 0; ci < 11; ++ci)
            {
                std::stringstream ss;

                ss << "Thread " << std::this_thread::get_id() << " - " << ci;

                auto data = ss.str();

                auto* pdata = storage.create(STORAGE_SIZE);

                std::strncpy(reinterpret_cast<char*>(pdata), data.c_str(), std::min(data.size() + 1, STORAGE_SIZE));

                LOG_TRACE(data);

                std::this_thread::yield();

                std::string check_data = reinterpret_cast<const char*>(storage.get());

                std::lock_guard<std::mutex> lg { test_checks_mutex };
                BOOST_REQUIRE_EQUAL(storage.size(), STORAGE_SIZE);
                BOOST_REQUIRE_EQUAL(data, check_data);
            }
        };
        std::thread th1(thread_payload);
        std::thread th2(thread_payload);
        std::thread th3(thread_payload);

        th1.join();
        th2.join();
        th3.join();
    }

    BOOST_AUTO_TEST_CASE(local_storage_increase_check)
    {
        print_current_test_name();

        constexpr size_t STORAGE_SIZE = 1024;
        thread_local_storage storage(1);
        std::mutex test_checks_mutex;
        auto thread_payload = [&]() {
            for (size_t ci = 0; ci < 11; ++ci)
            {
                std::stringstream ss;

                ss << "Thread " << std::this_thread::get_id() << " - " << ci;

                auto data = ss.str();

                auto* pdata = storage.create(STORAGE_SIZE);

                std::strncpy(reinterpret_cast<char*>(pdata), data.c_str(), std::min(data.size() + 1, storage.size()));

                LOG_TRACE(storage.size() << " -> " << data);

                std::this_thread::yield();

                std::string check_data = reinterpret_cast<const char*>(storage.get());

                std::lock_guard<std::mutex> lg { test_checks_mutex };
                BOOST_REQUIRE_EQUAL(data, check_data);

                storage.resize(STORAGE_SIZE / 4);
            }
        };
        std::thread th1(thread_payload);
        std::thread th2(thread_payload);
        std::thread th3(thread_payload);

        th1.join();
        th2.join();
        th3.join();
    }

    BOOST_AUTO_TEST_CASE(local_storage_cleanup_check)
    {
        print_current_test_name();

        BOOST_REQUIRE_THROW(thread_local_storage { 0 }, std::logic_error);

        constexpr size_t STORAGE_SIZE = 1024;
        thread_local_storage storage(1);
        std::mutex test_checks_mutex;
        auto thread_payload = [&]() {
            storage.create(STORAGE_SIZE);
            auto sub_payload = [&]() {
                for (size_t ci = 0; ci < 11 / 2; ++ci)
                {
                    std::stringstream ss;

                    ss << "Thread " << std::this_thread::get_id() << " - " << ci;

                    auto data = ss.str();

                    auto* pdata = storage.get();

                    std::strncpy(reinterpret_cast<char*>(pdata), data.c_str(), std::min(data.size() + 1, storage.size()));

                    LOG_TRACE(storage.size() << " -> " << data);

                    std::this_thread::yield();

                    std::string check_data = reinterpret_cast<const char*>(storage.get());

                    std::lock_guard<std::mutex> lg { test_checks_mutex };
                    BOOST_REQUIRE_EQUAL(data, check_data);

                    storage.resize(STORAGE_SIZE / 4);
                }
            };

            sub_payload();
            sub_payload();
        };
        std::thread th1(thread_payload);
        std::thread th2(thread_payload);
        std::thread th3(thread_payload);

        th1.join();
        th2.join();
        th3.join();
        storage.remove();
    }

    BOOST_AUTO_TEST_SUITE_END()
} // namespace tests
} // namespace server_lib
