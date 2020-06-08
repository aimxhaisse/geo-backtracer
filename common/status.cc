#include <ostream>

#include "common/status.h"

namespace bt {

Status::Status() : code_(StatusCode::OK) {}

Status::Status(StatusCode code, const std::string &message)
    : code_(code), message_(message) {}

Status::Status(StatusCode code) : code_(code) {}

Status::Status(const Status &status)
    : code_(status.code_), message_(status.message_) {}

Status &Status::operator=(const Status &status) {
  if (this != &status) {
    code_ = status.code_;
    message_ = status.message_;
  }
  return *this;
}

bool Status::operator==(StatusCode code) const { return code_ == code; }

bool Status::operator!=(StatusCode code) const { return !operator==(code); }

StatusCode Status::Code() const { return code_; }

const std::string &Status::Message() const { return message_; }

std::ostream &operator<<(std::ostream &os, const Status &status) {
  std::string str_code;

  switch (status.Code()) {
  case OK:
    str_code = "OK";
    break;
  case INTERNAL_ERROR:
    str_code = "INTERNAL_ERROR";
    break;
  case NOT_YET_IMPLEMENTED:
    str_code = "NOT_YET_IMPLEMENTED";
    break;
  case INVALID_CONFIG:
    str_code = "INVALID_CONFIG";
    break;
  case INVALID_ARGUMENT:
    str_code = "INVALID_ARGUMENT";
    break;
  default:
    str_code = "UNKNOWN";
    break;
  }

  if (status == StatusCode::OK) {
    os << str_code;
  } else {
    os << str_code;
    if (!status.Message().empty()) {
      os << " (" << status.Message() << ")";
    }
  }

  return os;
}

} // namespace bt
