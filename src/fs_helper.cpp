#include <server_lib/fs_helper.h>

#include <boost/filesystem.hpp>


namespace server_lib {

namespace bf = boost::filesystem;

bool is_regular_file(const std::string& path, bool not_empty)
{
    bf::path path_(path);

    bf::file_status s = bf::status(path_);

    if (bf::regular_file != s.type())
        return false;

    return !not_empty || bf::file_size(path_) > 0;
}

bool is_directory(const std::string& path)
{
    bf::file_status s = bf::status(path);

    return bf::directory_file == s.type();
}

} // namespace server_lib
