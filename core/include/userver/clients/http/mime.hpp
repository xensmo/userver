#pragma once

#include <curl-ev/mime.hpp>

USERVER_NAMESPACE_BEGIN

namespace curl {
    class mime;
}

namespace clients::http {
    class Mime final {
    public:
        Mime();
        ~Mime();

        Mime(const Mime& rhs) = delete;
        Mime(Mime&& rhs) noexcept = default;

        Mime& operator= (const Mime& rhs) = delete;
        Mime& operator= (Mime&& rhs) noexcept = default;

    public:
        [[nodiscard]] inline std::unique_ptr<curl::mime> get_native() && { return std::move(mime_); }

    private:
        std::unique_ptr<curl::mime> mime_;

    };
}

USERVER_NAMESPACE_END