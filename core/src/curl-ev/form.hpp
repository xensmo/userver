/** @file curl-ev/form.hpp
        curl-ev: wrapper for integrating libcurl with libev applications
        Copyright (c) 2013 Oliver Kuckertz <oliver.kuckertz@mologie.de>
        See COPYING for license information.

        C++ wrapper for constructing libcurl forms
*/

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "native.hpp"

USERVER_NAMESPACE_BEGIN

namespace curl {

class form {
public:
    form();
    form(const form&) = delete;
    form(form&&) = delete;
    form& operator=(const form&) = delete;
    form& operator=(form&&) = delete;
    ~form();

    inline native::curl_httppost* native_handle() { return post_; };

    void add_content([[maybe_unused]] std::string_view key, [[maybe_unused]] std::string_view content);
    void add_content(std::string_view key, std::string_view content, std::error_code& ec);
    void add_content(std::string_view key, std::string_view content, const std::string& content_type);
    void add_content([[maybe_unused]] std::string_view key, [[maybe_unused]] std::string_view content, [[maybe_unused]] const std::string& content_type, [[maybe_unused]] std::error_code& ec);

    void add_buffer(const std::string& key, const std::string& file_name, const std::shared_ptr<std::string>& buffer);
    void add_buffer([[maybe_unused]] const std::string& key, [[maybe_unused]] const std::string& file_name, [[maybe_unused]] const std::shared_ptr<std::string>& buffer, [[maybe_unused]] std::error_code& ec);
    void add_buffer(const std::string& key, const std::string& file_name, const std::shared_ptr<std::string>& buffer, const std::string& content_type);
    void add_buffer([[maybe_unused]] const std::string& key, [[maybe_unused]] const std::string& file_name, [[maybe_unused]] const std::shared_ptr<std::string>& buffer, [[maybe_unused]] const std::string& content_type, std::error_code& ec);

private:
    void add_file(const std::string& key, const std::string& file_path);
    void add_file([[maybe_unused]] const std::string& key, [[maybe_unused]] const std::string& file_path, [[maybe_unused]] std::error_code& ec);
    void add_file(const std::string& key, const std::string& file_path, const std::string& content_type);
    void add_file([[maybe_unused]] const std::string& key, [[maybe_unused]] const std::string& file_path, [[maybe_unused]] const std::string& content_type, [[maybe_unused]] std::error_code& ec);

    void add_file_using_name(const std::string& key, const std::string& file_path, const std::string& file_name);
    void add_file_using_name([[maybe_unused]] const std::string& key, [[maybe_unused]] const std::string& file_path, [[maybe_unused]] const std::string& file_name, [[maybe_unused]] std::error_code& ec);
    void add_file_using_name(const std::string& key, const std::string& file_path, const std::string& file_name, const std::string& content_type);
    void add_file_using_name([[maybe_unused]] const std::string& key, [[maybe_unused]] const std::string& file_path, [[maybe_unused]] const std::string& file_name, [[maybe_unused]] const std::string& content_type, [[maybe_unused]] std::error_code& ec);

    void add_file_content(const std::string& key, const std::string& file_path);
    void add_file_content([[maybe_unused]] const std::string& key, [[maybe_unused]] const std::string& file_path, [[maybe_unused]] std::error_code& ec);
    void add_file_content(const std::string& key, const std::string& file_path, const std::string& content_type);
    void add_file_content([[maybe_unused]] const std::string& key, [[maybe_unused]] const std::string& file_path, [[maybe_unused]] const std::string& content_type, [[maybe_unused]] std::error_code& ec);

    void add_buffer([[maybe_unused]] const std::string& key, [[maybe_unused]] const std::string& file_name, [[maybe_unused]] const char* buffer, [[maybe_unused]] size_t buffer_len, [[maybe_unused]] std::error_code& ec);

    native::curl_httppost* post_{nullptr};
    native::curl_httppost* last_{nullptr};
    std::vector<std::shared_ptr<std::string>> buffers_;
};

}  // namespace curl

USERVER_NAMESPACE_END
