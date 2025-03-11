#pragma once

// !TODO this @file curl-ev/mime.cpp create in 2025 year                    //
// !@author <<< xensmo >>> Taras Litvinenko <taraslitvinenko@yandex.kz>     //

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

    inline native::curl_mime*       native_mime() { return mime_; }
    inline native::curl_mimepart*   native_part() { return part_; }

    // contents
public:
    //void add_content(std::string_view key, std::string_view content);
    //void add_content(std::string_view key, std::string_view content, std::error_code& ec);
    //void add_content(std::string_view key, std::string_view content, const std::string& content_type);
    //void add_content(std::string_view key, std::string_view content, const std::string& content_type, std::error_code& ec);

    //void add_file_content(std::string_view key, std::string_view file_path);
    //void add_file_content(std::string_view key, std::string_view file_path, std::error_code& ec);
    //void add_file_content(std::string_view key, std::string_view file_path, const std::string& content_type);
    //void add_file_content(std::string_view key, std::string_view file_path, const std::string& content_type, std::error_code& ec);

    //void add_file_using_name(std::string_view key, std::string_view file_path, std::string_view file_name);
    //void add_file_using_name(std::string_view key, std::string_view file_path, std::string_view file_name, std::error_code ec);
    //void add_file_using_name(std::string_view key, std::string_view file_path, std::string_view file_name, const std::string& content_type);
    //void add_file_using_name(std::string_view key, std::string_view file_path, std::string_view file_name, const std::string& content_type, std::error_code& ec);

    // buffers 
public:
    //void add_buffer(std::string_view key, std::string_view file_name, 
    //        const std::shared_ptr<std::string>& buffer);
    //void add_buffer(std::string_view key, std::string_view file_name, 
    //        const std::shared_ptr<std::string>& buffer, std::error_code& ec);
    //void add_buffer(std::string_view key, std::string_view file_name, 
    //        const std::shared_ptr<std::string>& buffer, std::string_view content_type);
    //void add_buffer(std::string_view key, std::string_view file_name, 
    //        const std::shared_ptr<std::string>& buffer, std::string_view content_type, std::error_code& ec);


public:
    mime(const mime& rhs) = delete;
    mime(mime&& rhs) noexcept = default;

    mime& operator= (const mime& rhs) = delete;
    mime& operator= (mime&& rhs) noexcept = default;

private:
    native::curl_mime*      mime_ { nullptr };
    native::curl_mimepart*  part_ { nullptr };

    //std::vector<std::shared_ptr<std::string>> buffers_;
};

} // namespace curl

USERVER_NAMESPACE_END