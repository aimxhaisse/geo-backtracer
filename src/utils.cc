#include <boost/filesystem.hpp>
#include <glog/logging.h>

#include "utils.h"

namespace bt {
namespace utils {

StatusOr<std::string> MakeTemporaryDirectory() {
  boost::filesystem::path p;
  p += std::string(kTmpDir);
  p += "/";
  p += boost::filesystem::unique_path();

  if (!boost::filesystem::create_directory(p)) {
    RETURN_ERROR(INTERNAL_ERROR, "can't create temporary directory");
  }

  LOG(INFO) << "created temporary directory, path=" << p.generic_string();

  return p.generic_string();
}

void DeleteDirectory(const std::string &path) {
  boost::filesystem::remove_all(path);
  LOG(INFO) << "deleted directory, path=" << path;
}

bool DirExists(const std::string &path) {
  return boost::filesystem::exists(path);
}

} // namespace utils
} // namespace bt
