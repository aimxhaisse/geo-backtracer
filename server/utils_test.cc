#include <gtest/gtest.h>

#include "utils.h"

namespace bt {
namespace {

TEST(UtilsTest, TempDirectoryCycle) {
  StatusOr<std::string> status = utils::MakeTemporaryDirectory();
  EXPECT_TRUE(status.Ok());
  const std::string &path = status.ValueOrDie();
  EXPECT_TRUE(utils::DirExists(path));
  utils::DeleteDirectory(path);
  EXPECT_FALSE(utils::DirExists(path));
}

} // anonymous namespace
} // namespace bt
