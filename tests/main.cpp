#include <boost/test/included/unit_test.hpp>

#include "tests_common.h"

#ifndef NDEBUG
#include <boost/test/debug.hpp>
#endif

#include <memory>

boost::unit_test::test_suite* init_unit_test_suite(int, char*[])
{
    boost::unit_test::framework::master_test_suite().p_name.value = "Test BarbacoaServer library";

#ifndef NDEBUG
    boost::debug::detect_memory_leaks(true);
#endif
    server_lib::tests::init_global_log();

    return nullptr;
}
