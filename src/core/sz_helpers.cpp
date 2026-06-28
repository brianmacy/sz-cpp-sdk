// sz_helpers.cpp -- native call pattern helpers.
#include "sz_helpers.hpp"

#include <array>
#include <cstddef>

#include "SzExceptionMapper.hpp"

namespace senzing::sdk::core {

namespace {

// Buffer size for last-exception message retrieval (matches the C# SDK's 4KB).
constexpr std::size_t kExceptionBufferSize = 4096;

}  // namespace

int64_t GetLastExceptionCode(SzModule module) {
    switch (module) {
        case SzModule::Engine:
            return Sz_getLastExceptionCode();
        case SzModule::Config:
            return SzConfig_getLastExceptionCode();
        case SzModule::ConfigMgr:
            return SzConfigMgr_getLastExceptionCode();
        case SzModule::Diagnostic:
            return SzDiagnostic_getLastExceptionCode();
        case SzModule::Product:
            return SzProduct_getLastExceptionCode();
    }
    return 0;
}

std::string GetLastException(SzModule module) {
    std::array<char, kExceptionBufferSize> buffer{};
    int64_t copied = 0;
    switch (module) {
        case SzModule::Engine:
            copied = Sz_getLastException(buffer.data(), buffer.size());
            break;
        case SzModule::Config:
            copied = SzConfig_getLastException(buffer.data(), buffer.size());
            break;
        case SzModule::ConfigMgr:
            copied = SzConfigMgr_getLastException(buffer.data(), buffer.size());
            break;
        case SzModule::Diagnostic:
            copied = SzDiagnostic_getLastException(buffer.data(), buffer.size());
            break;
        case SzModule::Product:
            copied = SzProduct_getLastException(buffer.data(), buffer.size());
            break;
    }
    if (copied <= 0) {
        return std::string{};
    }
    // The returned length INCLUDES the trailing NUL (verified empirically), and
    // the buffer is NUL-terminated, so constructing from the C string is safe
    // and yields exactly the message without the terminator.
    return std::string{buffer.data()};
}

void ClearLastException(SzModule module) {
    switch (module) {
        case SzModule::Engine:
            Sz_clearLastException();
            break;
        case SzModule::Config:
            SzConfig_clearLastException();
            break;
        case SzModule::ConfigMgr:
            SzConfigMgr_clearLastException();
            break;
        case SzModule::Diagnostic:
            SzDiagnostic_clearLastException();
            break;
        case SzModule::Product:
            SzProduct_clearLastException();
            break;
    }
}

void CheckReturnCode(int64_t returnCode, SzModule module) {
    if (returnCode == 0) {
        return;
    }
    const int64_t errorCode = GetLastExceptionCode(module);
    const std::string message = GetLastException(module);
    ClearLastException(module);
    ThrowSzException(errorCode, message);
}

std::string TakeResponse(char* response) {
    if (response == nullptr) {
        return std::string{};
    }
    std::string result{response};
    SzHelper_free(response);
    return result;
}

}  // namespace senzing::sdk::core
