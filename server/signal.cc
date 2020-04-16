#include <condition_variable>
#include <csignal>
#include <ctime>
#include <glog/logging.h>
#include <mutex>

#include "server/signal.h"

namespace bt {

std::mutex gExitMutex;
std::condition_variable gDoExit;
std::time_t gSignaledAt;

constexpr int kSignalExpireDelaySec = 5;

namespace {

void HandleSignal(int sig) {
  if (sig == SIGINT) {
    std::unique_lock lock(gExitMutex);
    gSignaledAt = std::time(nullptr);
  }
  gDoExit.notify_one();
}

} // anonymous namespace

Status WaitForExit() {
  std::signal(SIGINT, HandleSignal);

  LOG(INFO) << "waiting for signal, press ctrl+c to exit...";

  std::time_t previous_signal_at = 0;
  std::unique_lock lock(gExitMutex);
  while (true) {
    if (gSignaledAt) {
      if ((gSignaledAt - previous_signal_at) < kSignalExpireDelaySec) {
        LOG(INFO) << "exiting...";
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

} // namespace bt
