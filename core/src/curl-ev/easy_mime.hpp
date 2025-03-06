#pragma once

/** @file curl-ev/easy_mime.hpp
        curl-ev: upgrade curl-ev/easy.hpp wrapper for integrating libcurl with libev applications
                         Copyright (c) 2013 Oliver Kuckertz <oliver.kuckertz@mologie.de>
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

#include <curl-ev/form_mime.hpp>
//#include <curl-ev/ratelimit.hpp>
//#include <curl-ev/url.hpp>

#include <userver/clients/http/local_stats.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine::ev {

    class ThreadControl;
    
}  // namespace engine::ev

namespace curl {
class multi;
class share;
//class string_list;

namespace detail {
    enum class empty_header_action      { kSend, kDoNotSend };
    enum class duplicate_header_action  { kAdd, kSkip, kReplace };
} // namespace detail

class easy_mime : public std::enable_shared_from_this<easy_mime> {
public:
    easy_mime(native::CURL* easy, native::curl_mime* mime, multi* multi);
    ~easy_mime();

public: 
    using time_point = std::chrono::steady_clock::time_point;
    using handler_type = std::function<void(std::error_code err)>;
    
    using progress_callback_t = std::function<bool( native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow)>;
    
    using xferfunc_callback_t = std::function<size_t(void* clinetp, native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow)>;

    using opensocket_callback_t = std::function<native::curl_socket_t(void* clientp, native::curlsocktype purpose, struct native::curl_sockaddr* address)>;

    void perform();
    void perform(std::error_code& ec);
    void async_perform(handler_type handler);
    void cancel();
    void reset();

// public sys
    void handle_completion(const std::error_code& ec);
    void set_progress_callback(progress_callback_t progress_callback);
    void unset_progress_callback();

// public static     
    static easy_mime* from_native(native::CURL* native_easy);
    static std::shared_ptr<const easy_mime> create_easy_mime_blocking();

// public
    std::error_code rate_limit_error() const;
    time_point::duration time_to_start() const;

// public inline     
    inline native::CURL* native_handle() { return easy_handle_; }

public:
    using detail_eha = detail::empty_header_action;
    using detail_dha = detail::duplicate_header_action;
    
    void add_header(std::string_view name, std::string_view value, 
            detail_eha eha = detail_eha::kSend, detail_dha dha = detail_dha::kAdd);
    void add_header(std::string_view name, std::string_view value, std::error_code& ec, 
            detail_eha eha = detail_eha::kSend, detail_dha dha = detail_dha::kAdd);
    void add_header(std::string_view name, std::string_view value, detail_dha dha);
    void add_header(std::string_view name, std::string_view value, std::error_code& ec, detail_dha dha);

    void add_header(const std::string& header);
    void add_header(const std::string& header, std::error_code& ec);

    void async_perform(handler_type handler);

public:
// public setters
    void set_no_progress(bool state);
    void set_xferinfo_function(xferfunc_callback_t xferfunc_callback);
    void set_xferinfo_data(void* ptr);
    void set_opensocket_function(opensocket_callback_t opensocket_callback);
    void set_opensocket_data(void* ptr);
    
    void set_url(std::string_view url);
    void set_http_version(long version);
    void set_http_post(std::unique_ptr<form_mime> mime);

// public getters
std::string_view get_effective_url();    
    
private:
    easy_mime(const easy_mime& rhs) = delete;
    easy_mime(easy_mime&& rhs) = delete;

    easy_mime& operator= (const easy_mime& rhs) = delete;
    easy_mime& operator= (easy_mime&& rhs) = delete;

private:
// private static
    static native::curl_socket_t open_socket(void* clientp, native::curlsocktype purpose, struct native::curl_sockaddr* address) noexcept;
    static int close_socket(void* clientp, native::curl_socket_t item) noexcept;
    static int seek_function(void* instream, native::curl_off_t offset, int origin) noexcept;
    static size_t read_runction(char* ptr, size_t size, size_t nmemb, void* userdata) noexcept;
    static size_t write_function(char* ptr, size_t size, size_t nmemb, void* userdata) noexcept;
    static size_t xfer_function(void* clinetp, native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow) noexcept;

    native::curl_socket_t open_tcp_socket(struct native::curl_sockaddr* address) noexcept;

protected:
// protected setters
    void set_no_progress(bool state, std::error_code& ec);
    void set_xferinfo_function(xferfunc_callback_t xferfunc_callback, std::error_code& ec);
    void set_opensocket_function(opensocket_callback_t opensocket_callback, std::error_code& ec);
    
    void set_url(std::string_view url, std::error_code ec);
    void set_http_version(long version, std::error_code ec);
    void set_http_post(std::unique_ptr<form_mime> mime, std::error_code& ec);

// protected getters
    std::string_view get_effective_url(std::error_code& ec);

protected:
    engine::ev::ThreadControl& get_thread_control();
    
// do_ev_* methods run in libev thread
    void do_ev_async_perform(handler_type handler, size_t request_num);
    void do_ev_cancel(size_t request_num);

    void mark_start_performing();
    void mark_open_socket();

    void cancel(size_t request_num);

protected:
    native::CURL*                   easy_handle_;
    native::curl_mime*              mime_;
    multi*                          multi_handle_;
    
    size_t                          request_counter_        { 0 };
    size_t                          cancelled_request_max_  { 0 };
    size_t                          sockets_opened_         { 0 };

    bool                            multi_registered_       { false };

    handler_type                    handler_;
    progress_callback_t             progress_callback_;

    std::error_code                 rate_limit_error_;

    std::string*                    sink_                   { nullptr };

    std::shared_ptr<form_mime>      form_mime_;
    std::shared_ptr<share>          share_;
    std::shared_ptr<std::istream>   source_;

    time_point start_performing_ts_{};
    const time_point construct_ts_;
};
} // namespace curl

USERVER_NAMESPACE_END