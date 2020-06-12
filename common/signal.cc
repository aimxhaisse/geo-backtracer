#include <glog/logging.h>
#include <condition_variable>
#include <csignal>
#include <ctime>
#include <mutex>

#include "common/signal.h"

namespace bt {
namespace utils {

std::mutex gExitMutex;
std::condition_variable gDoExit;
std::time_t gSignaledAt;
bool gKilled = false;

constexpr int kSignalExpireDelaySec = 5;

namespace {

void HandleSignal(int sig) {
  if (sig == SIGINT || sig == SIGKILL) {
    std::unique_lock lock(gExitMutex);
    gSignaledAt = std::time(nullptr);
    if (sig == SIGKILL) {
      gKilled = true;
    }
  }
  gDoExit.notify_one();
}

}  // anonymous namespace

Status WaitForExitSignal() {
  std::signal(SIGINT, HandleSignal);

  LOG(INFO) << "waiting for signal, press ctrl+c to exit...";

  std::time_t previous_signal_at = 0;
  std::unique_lock lock(gExitMutex);
  while (true) {
    if (gKilled) {
      LOG(INFO) << "killed, exiting...";
      break;
    }

    if (gSignaledAt) {
      if ((gSignaledAt - previous_signal_at) < kSignalExpireDelaySec) {
        LOG(INFO) << "interrupted twice in a short time, exiting...";
        break;
      } else {
        LOG(INFO)
            << "are you sure to exit? ctrl+c to confirm (offer expires in "
            << kSignalExpireDelaySec << "seconds)";
      }
      previous_signal_at = gSignaledAt;
    }

    gDoExit.wait(lock);
  }

  return StatusCode::OK;
}

}  // namespace utils
}  // namespace bt
