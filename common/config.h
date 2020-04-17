#pragma once

#include <memory>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

#include "common/status.h"

namespace bt {

class Config {
public:
  explicit Config(const YAML::Node &node);

  static StatusOr<std::unique_ptr<Config>>
  LoadFromPath(const std::string &path);

  static StatusOr<std::unique_ptr<Config>>
  LoadFromString(const std::string &content);

  template <typename T> T Get(const std::string &location, const T &def) const {
    return GetChildNode(location).as<T>(def);
  }

  template <typename T> T Get(const std::string &location) const {
    return Get(location, T());
  }

  std::unique_ptr<Config> GetConfig(const std::string &location) const;

  std::vector<std::unique_ptr<Config>>
  GetConfigs(const std::string &location) const;

private:
  YAML::Node GetChildNode(const std::string &location) const;

  YAML::Node node_;
};

} // namespace bt
