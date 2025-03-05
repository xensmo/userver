#pragma once

/** @file curl-ev/form_mime.hpp
        curl-ev: upgrade wrapper for integrating libcurl with libev applications
        Copyright (c) 2025 Taras Litvinenko <taraslitvinenko@yandex.kz>
        See COPYING for license information.

        C++ wrapper for constructing libcurl
*/

#include <memory>
#include <string>
#include <vector>

#include <curl-ev/native.hpp>

USERVER_NAMESPACE_BEGIN

namespace curl {

class form_mime final {
public:
    form_mime();
    ~form_mime();

    inline native::curl_mime*       native_mime() { return mime_; }
    inline native::curl_mimepart*   native_part() { return part_; }

    // contents
public:
    void add_content(std::string_view key, std::string_view content);
    void add_content(std::string_view key, std::string_view content, std::error_code& ec);
    void add_content(std::string_view key, std::string_view content, const std::string& content_type);
    void add_content(std::string_view key, std::string_view content, const std::string& content_type, std::error_code& ec);

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
    void add_buffer(std::string_view key, std::string_view file_name, 
            const std::shared_ptr<std::string>& buffer);
    void add_buffer(std::string_view key, std::string_view file_name, 
            const std::shared_ptr<std::string>& buffer, std::error_code& ec);
    void add_buffer(std::string_view key, std::string_view file_name, 
            const std::shared_ptr<std::string>& buffer, std::string_view content_type);
    void add_buffer(std::string_view key, std::string_view file_name, 
            const std::shared_ptr<std::string>& buffer, std::string_view content_type, std::error_code& ec);


public:
    form_mime(const form_mime& rhs) = delete;
    form_mime(form_mime&& rhs) = delete;

    form_mime& operator= (const form_mime& rhs) = delete;
    form_mime& operator= (form_mime&& rhs) = delete;

private:
    native::curl_mime*      mime_ { nullptr };
    native::curl_mimepart*  part_ { nullptr };

    std::vector<std::shared_ptr<std::string>> buffers_;
};

} // namespace curl

USERVER_NAMESPACE_END