#pragma once

#include <spdlog/fmt/bundled/format.h>
#include <spdlog/fmt/ostr.h>

namespace AddressEvaluator {

    enum class Error {
        None,
        UnexpectedEndOfInput,
        UnexpectedToken,
        DivisionByZero,
        UnknownIdentifier
    };

    struct Result {
        Error Status = Error::None;
        uintptr_t Value = 0;

        Result(Error status)
            : Status(status) {}

        Result(uintptr_t value)
            : Value(value) {}

        [[nodiscard]] bool IsError() const { return Status != Error::None; }
    };

    template <typename OStream>
    OStream& operator<<(OStream& os, const Error& error)
    {
        switch (error) {
            case Error::None: os << "No error"; break;
            case Error::UnexpectedEndOfInput: os << "Unexpected end of input"; break;
            case Error::UnexpectedToken: os << "Unexpected token"; break;
            case Error::DivisionByZero: os << "Division by zero"; break;
            case Error::UnknownIdentifier: os << "Unknown identifier"; break;
            default: os << "Unknown error"; break;
        }
        return os;
    }
}

template <>
struct fmt::formatter<AddressEvaluator::Error> : ostream_formatter {};
