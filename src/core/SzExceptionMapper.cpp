// SzExceptionMapper.cpp -- implementation of the errno -> SzException mapping.
#include "SzExceptionMapper.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

#include "senzing/sdk/SzException.hpp"

namespace senzing::sdk::core {

namespace {

using ErrorMapEntry = std::pair<int64_t, SzErrorClass>;

// Generated, sorted {errno, SzErrorClass} table from szerrors.json. Codes that
// map to the base SzException are omitted (base is the implicit default).
// Explicit element type so each braced row constructs an ErrorMapEntry (CTAD on
// std::array cannot deduce through nested brace-init rows).
constexpr ErrorMapEntry kErrorMap[] = {
#include "sz_error_map.inc"
};

}  // namespace

SzErrorClass ClassForErrorCode(int64_t errorCode) {
    // kErrorMap is sorted by code; binary search.
    const auto* const first = std::begin(kErrorMap);
    const auto* const last = std::end(kErrorMap);
    const auto* it = std::lower_bound(
        first, last, errorCode,
        [](const ErrorMapEntry& entry, int64_t code) {
            return entry.first < code;
        });
    if (it != last && it->first == errorCode) {
        return it->second;
    }
    return SzErrorClass::Base;
}

void ThrowSzException(int64_t errorCode, const std::string& message) {
    const std::optional<int64_t> code{errorCode};
    switch (ClassForErrorCode(errorCode)) {
        case SzErrorClass::BadInput:
            throw SzBadInputException(code, message);
        case SzErrorClass::NotFound:
            throw SzNotFoundException(code, message);
        case SzErrorClass::UnknownDataSource:
            throw SzUnknownDataSourceException(code, message);
        case SzErrorClass::Configuration:
            throw SzConfigurationException(code, message);
        case SzErrorClass::ReplaceConflict:
            throw SzReplaceConflictException(code, message);
        case SzErrorClass::RetryTimeoutExceeded:
            throw SzRetryTimeoutExceededException(code, message);
        case SzErrorClass::DatabaseConnectionLost:
            throw SzDatabaseConnectionLostException(code, message);
        case SzErrorClass::DatabaseTransient:
            throw SzDatabaseTransientException(code, message);
        case SzErrorClass::Database:
            throw SzDatabaseException(code, message);
        case SzErrorClass::License:
            throw SzLicenseException(code, message);
        case SzErrorClass::NotInitialized:
            throw SzNotInitializedException(code, message);
        case SzErrorClass::Unhandled:
            throw SzUnhandledException(code, message);
        case SzErrorClass::Base:
            break;
    }
    throw SzException(code, message);
}

}  // namespace senzing::sdk::core
