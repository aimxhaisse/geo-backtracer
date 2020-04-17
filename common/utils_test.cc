#include <gtest/gtest.h>

#include "common/utils.h"

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

TEST(StringSplitTest, NoDelim) {
  const std::vector<std::string> s =
      utils::StringSplit("there is no spoon", '.');

  EXPECT_EQ(s.size(), 1);
  EXPECT_EQ(s[0], "there is no spoon");
}

TEST(StringSplitTest, Empty) {
  const std::vector<std::string> s = utils::StringSplit("", '.');

  EXPECT_EQ(s.size(), 0);
}

TEST(StringSplitTest, Delim) {
  const std::vector<std::string> s =
      utils::StringSplit("there is no spoon", ' ');

  EXPECT_EQ(s.size(), 4);
  EXPECT_EQ(s[0], "there");
  EXPECT_EQ(s[1], "is");
  EXPECT_EQ(s[2], "no");
  EXPECT_EQ(s[3], "spoon");
}

TEST(StringSplitTest, MultipleConsecutiveDelims) {
  const std::vector<std::string> s =
      utils::StringSplit(" there  is      no spoon  ", ' ');

  EXPECT_EQ(s.size(), 4);
  EXPECT_EQ(s[0], "there");
  EXPECT_EQ(s[1], "is");
  EXPECT_EQ(s[2], "no");
  EXPECT_EQ(s[3], "spoon");
}

} // anonymous namespace
} // namespace bt
