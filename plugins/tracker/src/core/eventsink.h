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