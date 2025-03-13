#pragma once

/** @file curl-ev/mime.cpp */

#include <memory>
#include <string>
#include <vector>

#include <curl-ev/native.hpp>

USERVER_NAMESPACE_BEGIN

namespace curl {

class mime final {
public:
    mime(native::curl_mime* mime_ptr);
    ~mime();

    mime(const mime& rhs) = delete;
    mime(mime&& rhs) = delete;

    mime& operator= (const mime& rhs) = delete;
    mime& operator= (mime&& rhs) = delete;

    inline native::curl_mime*       native_mime() { return mime_; }
    inline native::curl_mimepart*   native_part() { return part_; }

public:
    void add_content(std::string_view key, std::string_view content);
    void add_content(std::string_view key, std::string_view content, std::string_view content_type);

    void add_buffer(std::string_view key, std::string_view name, const std::shared_ptr<std::string>& buffer);
    void add_buffer(std::string_view key, std::string_view name, std::string_view content_type, const std::shared_ptr<std::string>& buffer);
private:
    native::curl_mime*      mime_ { nullptr };
    native::curl_mimepart*  part_ { nullptr };
    std::vector<std::shared_ptr<std::string>> buffers_ { nullptr };
};

} // namespace curl

USERVER_NAMESPACE_END