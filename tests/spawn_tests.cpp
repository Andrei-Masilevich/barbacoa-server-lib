#include "tests_common.h"

#include <string>
#include <server_lib/spawn.h>

namespace server_lib {
namespace tests {

    BOOST_AUTO_TEST_SUITE(spawn_tests)

    BOOST_AUTO_TEST_CASE(spawn_ls_check)
    {
        print_current_test_name();

        char *pchild_argv[] = { "pwd", NULL /*end sign*/ };
        char *pchild_env[] = { NULL };

        spawn ls(pchild_argv, pchild_env, true);
        std::string s;
        size_t ci = 0;
        while (std::getline(ls.stdout, s))
        {
            std::cout << ci++ << ":\t" << s << '\n';
        }
    }

    BOOST_AUTO_TEST_SUITE_END()
}}