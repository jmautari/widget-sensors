#pragma once

#include "eventsink.h"
#include <wbemcli.h>
#include <comdef.h>
#include <atlbase.h>
#include <thread>
#include <condition_variable>
#include <mutex>

namespace core {
class ProcessWatcher {
public:
  ProcessWatcher() = default;
  ~ProcessWatcher();

  bool Start(WmiUtil::event_callback_t callback);
  void Stop();

private:
  void WatcherThread();

  WmiUtil::event_callback_t callback_;
  std::thread runner_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool done_{};
};
}  // namespace pw