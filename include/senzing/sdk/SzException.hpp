// SzException.hpp -- exception hierarchy for the Senzing C++ SDK.
//
// Ported from the official C# SDK (senzing-garage/sz-sdk-csharp@4.3.0,
// Senzing.Sdk/SzException.cs and the subclass tree). Names mirror the C# SDK
// character-for-character. The root SzException derives from std::exception and
// carries an optional numeric Senzing error code (mirroring C# `long? ErrorCode`).
//
// SzEnvironmentDestroyedException is deliberately NOT part of this tree: in C#
// it derives from InvalidOperationException (a misuse/logic error, not a
// Senzing engine error), so here it derives from std::logic_error.
#ifndef SENZING_SDK_SZEXCEPTION_HPP
#define SENZING_SDK_SZEXCEPTION_HPP

#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

namespace senzing::sdk {

/// Base exception for all Senzing engine errors.
///
/// Mirrors the six C# constructors. The optional cause is captured as a
/// human-readable string appended to the message (C++ std::exception has no
/// built-in cause chaining like .NET's InnerException).
class SzException : public std::exception {
public:
    SzException() = default;

    explicit SzException(std::string message)
        : message_(std::move(message)) {}

    SzException(std::optional<int64_t> errorCode, std::string message)
        : message_(std::move(message)), errorCode_(errorCode) {}

    explicit SzException(const std::exception& cause)
        : message_(cause.what()) {}

    SzException(std::string message, const std::exception& cause)
        : message_(std::move(message) + ": " + cause.what()) {}

    SzException(std::optional<int64_t> errorCode, std::string message,
                const std::exception& cause)
        : message_(std::move(message) + ": " + cause.what()),
          errorCode_(errorCode) {}

    ~SzException() override = default;

    /// The underlying Senzing error code, or std::nullopt if none was
    /// associated with this exception.
    [[nodiscard]] std::optional<int64_t> ErrorCode() const noexcept {
        return errorCode_;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return message_.c_str();
    }

protected:
    std::string message_;
    std::optional<int64_t> errorCode_;
};

// Macro to declare an SzException subclass that re-exposes the six-constructor
// surface and forwards to the named base. Keeps the tree faithful to C# while
// avoiding hand-repeated boilerplate.
#define SENZING_SDK_DECLARE_EXCEPTION(Name, Base)                            \
    class Name : public Base {                                               \
    public:                                                                  \
        Name() = default;                                                    \
        explicit Name(std::string message) : Base(std::move(message)) {}     \
        Name(std::optional<int64_t> errorCode, std::string message)          \
            : Base(errorCode, std::move(message)) {}                         \
        explicit Name(const std::exception& cause) : Base(cause) {}          \
        Name(std::string message, const std::exception& cause)               \
            : Base(std::move(message), cause) {}                             \
        Name(std::optional<int64_t> errorCode, std::string message,          \
             const std::exception& cause)                                    \
            : Base(errorCode, std::move(message), cause) {}                  \
    }

// ---- Tier 1: direct children of SzException ----
SENZING_SDK_DECLARE_EXCEPTION(SzBadInputException, SzException);
SENZING_SDK_DECLARE_EXCEPTION(SzConfigurationException, SzException);
SENZING_SDK_DECLARE_EXCEPTION(SzReplaceConflictException, SzException);
SENZING_SDK_DECLARE_EXCEPTION(SzRetryableException, SzException);
SENZING_SDK_DECLARE_EXCEPTION(SzUnrecoverableException, SzException);

// ---- Tier 2: children of SzBadInputException ----
SENZING_SDK_DECLARE_EXCEPTION(SzNotFoundException, SzBadInputException);
SENZING_SDK_DECLARE_EXCEPTION(SzUnknownDataSourceException, SzBadInputException);

// ---- Tier 2: children of SzRetryableException ----
SENZING_SDK_DECLARE_EXCEPTION(SzDatabaseConnectionLostException,
                              SzRetryableException);
SENZING_SDK_DECLARE_EXCEPTION(SzDatabaseTransientException,
                              SzRetryableException);
SENZING_SDK_DECLARE_EXCEPTION(SzRetryTimeoutExceededException,
                              SzRetryableException);

// ---- Tier 2: children of SzUnrecoverableException ----
SENZING_SDK_DECLARE_EXCEPTION(SzDatabaseException, SzUnrecoverableException);
SENZING_SDK_DECLARE_EXCEPTION(SzLicenseException, SzUnrecoverableException);
SENZING_SDK_DECLARE_EXCEPTION(SzNotInitializedException,
                              SzUnrecoverableException);
SENZING_SDK_DECLARE_EXCEPTION(SzUnhandledException, SzUnrecoverableException);

#undef SENZING_SDK_DECLARE_EXCEPTION

/// Thrown when an SzEnvironment (or a subsystem it owns) is used after
/// Destroy(). This is a programming/misuse error -- NOT a Senzing engine error
/// -- so it is rooted at std::logic_error, mirroring the C# SDK's choice of
/// InvalidOperationException.
class SzEnvironmentDestroyedException : public std::logic_error {
public:
    SzEnvironmentDestroyedException()
        : std::logic_error("The Senzing environment has been destroyed.") {}
    explicit SzEnvironmentDestroyedException(const std::string& message)
        : std::logic_error(message) {}
    // Mirrors the C# (string message, Exception cause) and (Exception cause)
    // constructors. std::logic_error carries no separate cause, so -- exactly as
    // SzException does -- the cause's text is folded into the message.
    SzEnvironmentDestroyedException(const std::string& message,
                                    const std::exception& cause)
        : std::logic_error(message + ": " + cause.what()) {}
    explicit SzEnvironmentDestroyedException(const std::exception& cause)
        : std::logic_error(cause.what()) {}
};

}  // namespace senzing::sdk

#endif  // SENZING_SDK_SZEXCEPTION_HPP
