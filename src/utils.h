#pragma once

#include "status.h"

namespace bt {
namespace utils {

// Path to the temporary directory.
constexpr char kTmpDir[] = "/tmp";

// Creates a temporary directory.
StatusOr<std::string> MakeTemporaryDirectory();

// Recursively deletes a directory and its content.
void DeleteDirectory(const std::string &path);

} // namespace utils
} // namespace bt
