#pragma once

#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>
#include <string>
#include <functional>

#pragma comment(lib, "wbemuuid.lib")

namespace WmiUtil {
using event_callback_t = std::function<void(std::string, std::string, int pid)>;

class EventSink : public IWbemObjectSink {
  LONG ref_{};
  std::string event_type_;
  event_callback_t callback_;

public:
  EventSink(std::string event_type, event_callback_t callback)
      : event_type_(std::move(event_type)), callback_(std::move(callback)) {
  }

  virtual ULONG STDMETHODCALLTYPE AddRef();
  virtual ULONG STDMETHODCALLTYPE Release();
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

  virtual HRESULT STDMETHODCALLTYPE Indicate(LONG lObjectCount,
      IWbemClassObject __RPC_FAR* __RPC_FAR* apObjArray);

  virtual HRESULT STDMETHODCALLTYPE SetStatus(LONG lFlags,
      HRESULT hResult,
      BSTR strParam,
      IWbemClassObject __RPC_FAR* pObjParam);
};
}  // namespace WmiUtil