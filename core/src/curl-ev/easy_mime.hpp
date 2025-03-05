#pragma once

/** @file curl-ev/easy_mime.hpp
        curl-ev: upgrade wrapper for integrating libcurl with libev applications
        Copyright (c) 2025 Taras Litvinenko <taraslitvinenko@yandex.kz>
        See COPYING for license information.

        C++ wrapper for libcurl's easy interface
*/

#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

//#include <curl-ev/error_code.hpp>
#include <curl-ev/form_mime.hpp>
//#include <curl-ev/ratelimit.hpp>
//#include <curl-ev/url.hpp>

#include <userver/clients/http/local_stats.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine::ev {

    class ThreadControl;
    
}  // namespace engine::ev

namespace curl {
// class form;
//class multi;
//class share;
//class string_list;

enum class empty_header_action      { kSend, kDoNotSend };
enum class duplicate_header_action  { kAdd, kSkip, kReplace };

class easy_mime : public std::enable_shared_from_this<easy_mime> {
public:
    easy_mime();
    ~easy_mime();

public:
//    using handler_type = std::function<void(std::error_code err)>;
//    using time_point = std::chrono::steady_clock::time_point;
    
    //static easy_mime* from_native(native::CURL* native_easy);
    
    static std::shared_ptr<const easy_mime> CreateEasyBlocking();

public:
    void add_header(std::string_view name, std::string_view value, 
            empty_header_action eha = empty_header_action::kSend, duplicate_header_action dha = duplicate_header_action::kAdd);
    void add_header(std::string_view name, std::string_view value, std::error_code& ec, 
            empty_header_action eha = empty_header_action::kSend, duplicate_header_action dha = duplicate_header_action::kAdd);
    void add_header(std::string_view name, std::string_view value, duplicate_header_action dha);
    void add_header(std::string_view name, std::string_view value, std::error_code& ec, duplicate_header_action dha);

    void add_header(const std::string& header);
    void add_header(const std::string& header, std::error_code& ec);



// setters
    //void set_http_post(std::unique_ptr<form_mime> mime);
    //void set_http_post(std::unique_ptr<form_mime> mime, std::error_code& ec);

    void set_headers();

protected:
    
    


};
} // namespace curl

USERVER_NAMESPACE_END