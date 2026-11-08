#pragma once

#include <objbase.h>

namespace peek {

// RAII COM initialization for the current thread. Pairs every successful CoInitializeEx
// (including S_FALSE, which also requires a matching CoUninitialize) with CoUninitialize.
// RPC_E_CHANGED_MODE means the thread already has a different apartment; COM is still usable.
class ComScope {
public:
    explicit ComScope(DWORD model = COINIT_MULTITHREADED) : result_(CoInitializeEx(nullptr, model)) {}
    ~ComScope() { if (SUCCEEDED(result_)) CoUninitialize(); }
    ComScope(const ComScope&) = delete;
    ComScope& operator=(const ComScope&) = delete;

    [[nodiscard]] bool Usable() const { return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE; }
    [[nodiscard]] HRESULT Result() const { return result_; }

private:
    HRESULT result_;
};

} // namespace peek
