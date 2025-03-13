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

        Mime(const Mime&) = delete;
        Mime(Mime&&) noexcept;

        Mime& operator= (const Mime&) = delete;
        Mime& operator= (Mime&&) noexcept;

    public:
        void AddContent(std::string_view key, std::string_view content);
        void AddContent(std::string_view key, std::string_view content, std::string_view content_type);

        void AddBuffer(std::string_view key, std::string_view name, const std::shared_ptr<std::string>& buffer);
        void AddBuffer(std::string_view key, std::string_view name, std::string_view content_type, const std::shared_ptr<std::string>& buffer);

    public:
        std::unique_ptr<curl::mime> GetNative() &&;

    private:
        std::unique_ptr<curl::mime> mime_;

    };
}

USERVER_NAMESPACE_END