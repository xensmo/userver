#include <userver/clients/http/mime.hpp>

USERVER_NAMESPACE_BEGIN

namespace clients::http {
Mime::Mime() {
    
}

Mime::~Mime() {

}

Mime::Mime(Mime&&) noexcept = default;
Mime& Mime::operator= (Mime&&) noexcept = default;

std::unique_ptr<curl::mime> Mime::GetNative() && { return std::move(mime_); }

void Mime::AddContent(std::string_view key, std::string_view content) {
    mime_->add_content(key, content);
}

void Mime::AddContent(std::string_view key, std::string_view content, std::string_view content_type) {
    mime_->add_content(key, content, content_type);
}

void Mime::AddBuffer(std::string_view key, std::string_view name, const std::shared_ptr<std::string>& buffer) {
    mime_->add_buffer(key, name, buffer);
}

void Mime::AddBuffer(std::string_view key, std::string_view name, std::string_view content_type, const std::shared_ptr<std::string>& buffer) {
    mime_->add_buffer(key, name, content_type, buffer);
}

} // namespace clients::http

USERVER_NAMESPACE_END