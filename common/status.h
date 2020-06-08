#pragma once

#include <cassert>
#include <iosfwd>
#include <sstream>
#include <string>

namespace bt {

enum StatusCode {
  OK = 0,
  INTERNAL_ERROR,
  NOT_YET_IMPLEMENTED,
  INVALID_CONFIG,
  INVALID_ARGUMENT,
};

class Status {
public:
  Status();
  Status(StatusCode code, const std::string &message);
  Status(StatusCode code);
  Status(const Status &status);

  Status &operator=(const Status &status);
  bool operator==(StatusCode code) const;
  bool operator!=(StatusCode code) const;

  StatusCode Code() const;
  const std::string &Message() const;

private:
  StatusCode code_;
  std::string message_;
};

template <typename T> class StatusOr {
public:
  StatusOr() : status_(StatusCode::INTERNAL_ERROR), value_() {}
  StatusOr(StatusCode code) : status_(code), value_() {}
  StatusOr(const Status &status) : status_(status), value_() {}
  StatusOr(const T &value) : status_(StatusCode::OK), value_(value) {}
  StatusOr(T &&value) : status_(StatusCode::OK), value_(std::move(value)) {}

  const Status &GetStatus() const { return status_; }

  bool Ok() const { return status_ == StatusCode::OK; }

  T &ValueOrDie() {
    assert(status_ == StatusCode::OK);
    return value_;
  }

private:
  Status status_;
  T value_;
};

std::ostream &operator<<(std::ostream &os, const Status &status);

Status &operator<<(Status &status, std::ostream &os);

#define RETURN_IF_ERROR(status)                                                \
  do {                                                                         \
    if (status != StatusCode::OK) {                                            \
      return status;                                                           \
    }                                                                          \
  } while (false)

#define ASSIGN_OR_RETURN(what, expr)                                           \
  do {                                                                         \
    auto __status_or = expr;                                                   \
    RETURN_IF_ERROR(__status_or.GetStatus());                                  \
    what = __status_or.ValueOrDie();                                           \
  } while (false)

#define MOVE_OR_RETURN(what, expr)                                             \
  do {                                                                         \
    auto __status_or = expr;                                                   \
    RETURN_IF_ERROR(__status_or.GetStatus());                                  \
    what = std::move(__status_or.ValueOrDie());                                \
  } while (false)

#define RETURN_ERROR(code, what)                                               \
  do {                                                                         \
    std::stringstream message;                                                 \
    message << what;                                                           \
    return Status(code, message.str());                                        \
  } while (false)

} // namespace bt
