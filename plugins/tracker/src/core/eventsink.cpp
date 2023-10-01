// EventSink.cpp
#include "eventsink.h"
#include <atlbase.h>

namespace WmiUtil {
ULONG EventSink::AddRef() {
  return InterlockedIncrement(&ref_);
}

ULONG EventSink::Release() {
  auto ref = InterlockedDecrement(&ref_);
  if (ref <= 0)
    delete this;

  return ref;
}

HRESULT EventSink::QueryInterface(REFIID riid, void** ppv) {
  if (riid == IID_IUnknown || riid == IID_IWbemObjectSink) {
    *ppv = reinterpret_cast<IWbemObjectSink*>(this);
    AddRef();
    return WBEM_S_NO_ERROR;
  }

  return E_NOINTERFACE;
}

HRESULT EventSink::Indicate(long lObjectCount, IWbemClassObject** apObjArray) {
  for (long i = 0; i < lObjectCount; i++) {
    VARIANT vtProp{};
    HRESULT hr = apObjArray[i]->Get(L"TargetInstance", 0, &vtProp, 0, 0);
    if (SUCCEEDED(hr) &&
        (vtProp.vt == VT_UNKNOWN || vtProp.vt == VT_DISPATCH)) {
      CComPtr<IWbemClassObject> pEventObject;
      hr = vtProp.punkVal->QueryInterface(IID_PPV_ARGS(&pEventObject));
      if (FAILED(hr))
        continue;

      VARIANT vtProcessName;
      hr = pEventObject->Get(L"Name", 0, &vtProcessName, 0, 0);
      if (FAILED(hr) || vtProcessName.vt != VT_BSTR)
        continue;

      std::string process_name;

      // Convert OLECHAR* (wide char) to std::string using WIN32 APIs
      BSTR bstr = vtProcessName.bstrVal;
      int len = ::SysStringLen(bstr);
      int buffer_size = ::WideCharToMultiByte(
          CP_UTF8, 0, bstr, len, nullptr, 0, nullptr, nullptr);

      if (buffer_size > 0) {
        process_name.resize(buffer_size);
        ::WideCharToMultiByte(CP_UTF8, 0, bstr, len, process_name.data(),
            buffer_size, nullptr, nullptr);
      }

      int pid = 0;
      VARIANT vtPID;
      hr = pEventObject->Get(L"ProcessId", 0, &vtPID, 0, 0);
      if (SUCCEEDED(hr) && vtPID.vt == VT_I4)
        pid = vtPID.intVal;

      if (callback_)
        callback_(event_type_, process_name, pid);
    }
  }

  return WBEM_S_NO_ERROR;
}

HRESULT EventSink::SetStatus(LONG lFlags,
    HRESULT hResult,
    BSTR strParam,
    IWbemClassObject __RPC_FAR* pObjParam) {
  return WBEM_S_NO_ERROR;
}
}  // namespace WmiUtil