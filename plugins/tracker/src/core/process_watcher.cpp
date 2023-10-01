#include "process_watcher.hpp"
#include <array>

namespace core {
ProcessWatcher::~ProcessWatcher() {
  Stop();
  if (runner_.joinable())
    runner_.join();
}

void ProcessWatcher::WatcherThread() {
  // Step 1: --------------------------------------------------
  // Initialize COM. ------------------------------------------

  HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
  if (FAILED(hres)) {
    printf("Failed to initialize COM library. Error code = 0x%X", hres);
    return;
  }

  do {
    // Step 2: --------------------------------------------------
    // Set general COM security levels --------------------------

    hres = CoInitializeSecurity(NULL,
        -1,                           // COM negotiates service
        NULL,                         // Authentication services
        NULL,                         // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,    // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE,  // Default Impersonation
        NULL,                         // Authentication info
        EOAC_NONE,                    // Additional capabilities
        NULL                          // Reserved
    );
    if (FAILED(hres)) {
      printf("Failed to initialize security. Error code = 0x%X", hres);
      break;
    }

    // Step 3: ---------------------------------------------------
    // Obtain the initial locator to WMI -------------------------

    CComPtr<IWbemLocator> pLoc;

    hres = CoCreateInstance(
        CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pLoc));
    if (FAILED(hres)) {
      printf("Failed to create IWbemLocator object. Err code = 0x%X", hres);
      break;
    }

    // Step 4: ---------------------------------------------------
    // Connect to WMI through the IWbemLocator::ConnectServer method

    CComPtr<IWbemServices> pSvc;

    // Connect to the local root\cimv2 namespace
    // and obtain pointer pSvc to make IWbemServices calls.
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) {
      printf("Could not connect. Error code = 0x%X", hres);
      break;
    }

    printf("Connected to ROOT\\CIMV2 WMI namespace\n");

    // Step 5: --------------------------------------------------
    // Set security levels on the proxy -------------------------

    hres = CoSetProxyBlanket(pSvc,    // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,            // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,             // RPC_C_AUTHZ_xxx
        NULL,                         // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,       // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE,  // RPC_C_IMP_LEVEL_xxx
        NULL,                         // client identity
        EOAC_NONE                     // proxy capabilities
    );
    if (FAILED(hres)) {
      printf("Could not set proxy blanket. Error code = 0x%X", hres);
      break;
    }

    // Step 6: -------------------------------------------------
    // Receive event notifications -----------------------------

    // Use an unsecured apartment for security
    CComPtr<IUnsecuredApartment> pUnsecApp;
    hres = CoCreateInstance(CLSID_UnsecuredApartment, NULL, CLSCTX_LOCAL_SERVER,
        IID_PPV_ARGS(&pUnsecApp));

    auto create_event_listener = [&](auto&& et, auto&& q,
                                     CComPtr<IWbemObjectSink>& pStubSink)
        -> std::unique_ptr<WmiUtil::EventSink> {
      auto pSink = std::make_unique<WmiUtil::EventSink>(
          et, [&](auto&& event_type, auto&& process_name, int pid) {
            if (callback_)
              callback_(event_type, process_name, pid);
          });

      CComPtr<IUnknown> pStubUnk;
      hres = pUnsecApp->CreateObjectStub(pSink.get(), &pStubUnk);
      if (FAILED(hres))
        return nullptr;

      hres = pStubUnk->QueryInterface(IID_PPV_ARGS(&pStubSink));
      if (FAILED(hres))
        return nullptr;

      // The ExecNotificationQueryAsync method will call
      // The EventQuery::Indicate method when an event occurs
      hres = pSvc->ExecNotificationQueryAsync(
          _bstr_t("WQL"), _bstr_t(q), WBEM_FLAG_SEND_STATUS, NULL, pStubSink);
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
    std::array<CComPtr<IWbemObjectSink>, 2> stubs;
    for (size_t i = 0; i < sizeof(queries) / sizeof(queries[0]); i++) {
      auto p = create_event_listener(
          queries[i].first, queries[i].second.c_str(), stubs[i]);
      if (FAILED(hres)) {
        printf("Could not create event listener for %s. HR=0x%X\n",
            queries[i].first.c_str(), hres);
        break;
      }

      events[i] = std::move(p);
    }

    if (FAILED(hres))
      break;

    // Wait for the done event
    {
      std::unique_lock lock(mutex_);
      cv_.wait(lock, [&] { return done_; });
    }

    for (size_t i = 0; i < sizeof(queries) / sizeof(queries[0]); i++) {
      hres = pSvc->CancelAsyncCall(stubs[i]);
      if (FAILED(hres))
        printf("Failure\n");

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