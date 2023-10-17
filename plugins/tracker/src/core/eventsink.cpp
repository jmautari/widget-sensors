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