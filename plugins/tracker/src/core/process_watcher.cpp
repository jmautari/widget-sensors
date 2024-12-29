/**
 * Widget Sensors
 * Process Tracer plug-in
 * Copyright (C) 2021-2023 John Mautari - All rights reserved
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "process_watcher.hpp"
#include <wrl/client.h>
#include "shared/logger.hpp"
#include <array>

using Microsoft::WRL::ComPtr;

namespace core {
ProcessWatcher::~ProcessWatcher() {
  Stop();
  if (runner_.joinable())
    runner_.join();
}

void ProcessWatcher::WatcherThread() {
  do {
    // Step 3: ---------------------------------------------------
    // Obtain the initial locator to WMI -------------------------

    ComPtr<IWbemLocator> pLoc;

    HRESULT hres = CoCreateInstance(
        CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pLoc));
    if (FAILED(hres)) {
      LOG(ERROR) << "Failed to create IWbemLocator object. Err code = 0x"
                 << std::hex << hres;
      break;
    }

    // Step 4: ---------------------------------------------------
    // Connect to WMI through the IWbemLocator::ConnectServer method

    ComPtr<IWbemServices> pSvc;

    // Connect to the local root\cimv2 namespace
    // and obtain pointer pSvc to make IWbemServices calls.
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) {
      LOG(ERROR) << "Could not connect. Error code = 0x" << std::hex << hres;
      break;
    }

    LOG(INFO) << "Connected to ROOT\\CIMV2 WMI namespace";

    // Step 5: --------------------------------------------------
    // Set security levels on the proxy -------------------------

    hres = CoSetProxyBlanket(pSvc.Get(),  // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,                // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,                 // RPC_C_AUTHZ_xxx
        NULL,                             // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,           // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE,      // RPC_C_IMP_LEVEL_xxx
        NULL,                             // client identity
        EOAC_NONE                         // proxy capabilities
    );
    if (FAILED(hres)) {
      LOG(ERROR) << "Could not set proxy blanket. Error code = 0x" << std::hex
                 << hres;
      break;
    }

    // Step 6: -------------------------------------------------
    // Receive event notifications -----------------------------

    // Use an unsecured apartment for security
    ComPtr<IUnsecuredApartment> pUnsecApp;
    hres = CoCreateInstance(CLSID_UnsecuredApartment, NULL, CLSCTX_LOCAL_SERVER,
        IID_PPV_ARGS(&pUnsecApp));

    auto create_event_listener = [&](auto&& et, auto&& q,
                                     ComPtr<IWbemObjectSink>& pStubSink)
        -> std::unique_ptr<WmiUtil::EventSink> {
      auto pSink = std::make_unique<WmiUtil::EventSink>(
          et, [&](auto&& event_type, auto&& process_name, int pid) {
            if (callback_)
              callback_(event_type, process_name, pid);
          });

      ComPtr<IUnknown> pStubUnk;
      hres = pUnsecApp->CreateObjectStub(pSink.get(), &pStubUnk);
      if (FAILED(hres))
        return nullptr;

      hres = pStubUnk->QueryInterface(IID_PPV_ARGS(&pStubSink));
      if (FAILED(hres))
        return nullptr;

      // The ExecNotificationQueryAsync method will call
      // The EventQuery::Indicate method when an event occurs
      hres = pSvc->ExecNotificationQueryAsync(
          _bstr_t("WQL"), _bstr_t(q), WBEM_FLAG_SEND_STATUS, NULL, pStubSink.Get());
      if (FAILED(hres))
        return nullptr;

      return pSink;
    };

    std::array<std::pair<std::string, std::string>, 2> queries = {
      std::make_pair("started",
          "SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE "
          "TargetInstance ISA 'Win32_Process'"),
      std::make_pair("terminated",
          "SELECT * FROM __InstanceDeletionEvent WITHIN 1 WHERE "
          "TargetInstance ISA 'Win32_Process'")
    };
    std::array<std::unique_ptr<WmiUtil::EventSink>, 2> events;
    std::array<ComPtr<IWbemObjectSink>, 2> stubs;
    for (size_t i = 0; i < sizeof(queries) / sizeof(queries[0]); i++) {
      auto p = create_event_listener(
          queries[i].first, queries[i].second.c_str(), stubs[i]);
      if (FAILED(hres)) {
        LOG(ERROR) << "Could not create event listener for " << queries[i].first
                   << ". HR=0x" << hres;
        break;
      }

      events[i] = std::move(p);
    }

    if (FAILED(hres)) {
      LOG(ERROR) << "General failure. HR=0x" << std::hex << hres;
      break;
    }

    // Wait for the done event
    {
      std::unique_lock lock(mutex_);
      cv_.wait(lock, [&] { return done_; });
    }

    for (size_t i = 0; i < sizeof(queries) / sizeof(queries[0]); i++) {
      hres = pSvc->CancelAsyncCall(stubs[i].Get());
      if (FAILED(hres))
        LOG(ERROR) << "Failure. HR=0x" << std::hex << hres;

      events[i].release();
    }
  } while (false);

  CoUninitialize();
}

bool ProcessWatcher::Start(WmiUtil::event_callback_t callback) {
  callback_ = std::move(callback);
  runner_ = std::thread([this] { WatcherThread(); });
  return runner_.joinable();
}

void ProcessWatcher::Stop() {
  std::unique_lock lock(mutex_);
  if (done_)
    return;

  done_ = true;
  cv_.notify_all();
}
}  // namespace pw