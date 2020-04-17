#pragma once

#include "common/status.h"

namespace bt {
namespace utils {

// Path to the temporary directory.
constexpr char kTmpDir[] = "/tmp";

// Creates a temporary directory.
StatusOr<std::string> MakeTemporaryDirectory();

// Recursively deletes a directory and its content.
void DeleteDirectory(const std::string &path);

// Whether or not a directory exists.
bool DirExists(const std::string &path);

// Split a string with a delimiter.
std::vector<std::string> StringSplit(const std::string &str, char delim);

} // namespace utils
} // namespace bt
