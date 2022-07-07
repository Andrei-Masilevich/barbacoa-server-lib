#pragma once

// For backward compatibility
#include "change_current_dir.h"

namespace server_lib {

bool is_regular_file(const std::string& path, bool not_empty = false);

bool is_directory(const std::string& path);

} // namespace server_lib
