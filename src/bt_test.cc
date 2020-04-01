#include <boost/filesystem.hpp>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include "bt.h"

namespace bt {
namespace {

constexpr char kTmpDir[] = "/tmp";

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

struct BtTest : public testing::Test {
  void SetUp() { temp_ = MakeTemporaryDirectory().ValueOrDie(); }
  void TearDown() { DeleteDirectory(temp_); }

  std::string temp_;
};

TEST_F(BtTest, InitOk) {
  Backtracer tracer;
  EXPECT_EQ(tracer.Init(temp_), StatusCode::OK);
}

TEST_F(BtTest, InitKo) {
  Backtracer tracer;
  EXPECT_EQ(tracer.Init("/dev/null"), StatusCode::INTERNAL_ERROR);
}

} // namespace
} // namespace bt
