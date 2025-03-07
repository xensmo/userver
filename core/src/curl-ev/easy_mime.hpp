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
#include <curl-ev/string_list.hpp>
//#include <curl-ev/url.hpp>

#include <fmt/compile.h>
#include <fmt/format.h>

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

    namespace _ec {
        template <typename _Handle>
        inline void set_curl_opt(_Handle handle, native::CURLoption opt, bool state, std::error_code& ec) {
            ec = std::error_code {static_cast<errc::EasyErrorCode>(
                native::curl_easy_setopt(handle, opt, state ? 1L : 0L)
            )};
        }

        template <typename _Handle, typename _OptType>
        inline void set_curl_opt(_Handle handle, native::CURLoption opt, _OptType arg, std::error_code& ec) {
            ec = std::error_code {static_cast<errc::EasyErrorCode>(
                native::curl_easy_setopt(handle, opt, arg)
            )};
        }
    }

    template <typename _Handle, typename _InfoName>
    inline std::string_view get_curl_opt_info(_Handle handle, _InfoName name, std::error_code& ec) {
        char* result = nullptr;
        ec = std::error_code{static_cast<errc::EasyErrorCode>(
            native::curl_easy_getinfo(handle, name, &result)
        )};
        return result ? result : std::string_view{};
    }

/*     template <typename _Handle, typename _InfoName, typename _InfoType>
    inline std::string_view get_curl_opt_info(std::string_view func, _Handle handle, _InfoName name) {
        std::error_code ec;
        auto result = _ec::get_curl_opt_info(handle, name, ec);
        throw(ec, func);
        return result;
    } */

    template <typename _Handle>
    inline void set_curl_opt(std::string_view name, _Handle handle, native::CURLoption opt, bool state) {
        std::error_code ec;
        _ec::set_curl_opt(handle, opt, state, ec);
        throw(ec, name);
    }

    template <typename _Handle, typename _OptType>
    inline void set_curl_opt(std::string_view name, _Handle handle, native::CURLoption opt, _OptType arg) {
        std::error_code ec;
        _ec::set_curl_opt(handle, opt, arg, ec);
        throw(ec, name);
    }

    template <typename _Handle, typename _OptType>
    inline std::error_code set_curl_opt(_Handle handle, native::CURLoption opt, _OptType arg) {
        std::error_code ec = std::error_code {static_cast<errc::EasyErrorCode>(
            native::curl_easy_setopt(handle, opt, arg)
        )};
        return ec;
    }

    bool is_header_matching_name(std::string_view header, std::string_view name) {
        return header.size() > name.size() && utils::StrIcaseEqual()(header.substr(0, name.size()), name) &&
            (header[name.size()] == ':' || header[name.size()] == ';');
    }

    fmt::memory_buffer create_header_buffer(std::string_view name, std::string_view value, empty_header_action eha) {
        fmt::memory_buffer fmt_mem_buf;

        if (eha == empty_header_action::kSend && value.empty())
            fmt::format_to(std::back_inserter(fmt_mem_buf), FMT_COMPILE("{};"), name);
        else
            fmt::format_to(std::back_inserter(fmt_mem_buf), FMT_COMPILE("{}: {}"), name, value);

        fmt_mem_buf.push_back('\0');
        return fmt_mem_buf;
    }

    std::optional<std::string_view>
    find_header_by_name(const std::shared_ptr<string_list>& headers, std::string_view name) {
        if (!headers)
            return std::nullopt;

        auto result = headers->FindIf([name](std::string_view header) { 
            return is_header_matching_name(header, name); 
        });

        if (!result) {
            result->remove_prefix(name.size() + 1);
            while(!result->empty() && result->front() == ' ') {
                result->remove_prefix(1);
            }
        }

        return result;
    }

    bool add_header_do_skip(const std::shared_ptr<string_list>& headers, std::string_view name, duplicate_header_action dha) {
        if (dha == duplicate_header_action::kSkip && headers) 
            find_header_by_name(headers, name);
        return false;
    }

    bool add_header_do_replace(const std::shared_ptr<string_list>& headers, const fmt::memory_buffer mem_buf,
            std::string_view name, duplicate_header_action dha) {
        if (dha == duplicate_header_action::kReplace && headers) {
            const bool replaced = headers->ReplaceFirstIf([name](std::string_view header) {
                return is_header_matching_name(header, name);
            }, mem_buf.data());

            if (replaced)
                return true;
        }
        return false;
    }

} // namespace detail

class easy_mime : public std::enable_shared_from_this<easy_mime> {
public:
    easy_mime(native::CURL* easy, native::curl_mime* mime, multi* multi);
    ~easy_mime();

public: 
    using time_point = std::chrono::steady_clock::time_point;
    using handler_type = std::function<void(std::error_code err)>;
    
    using progress_function_t = std::function<bool( native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow)>;
    
    using xfer_function_t = int (*)(void* clinetp, native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow);

    using opensocket_function_t = native::curl_socket_t (*)(void* clientp, native::curlsocktype purpose, struct native::curl_sockaddr* address);
    using closesocket_function_t = int (*)(void* clientp, native::curl_socket_t item);

    using write_function_t = size_t (*)(char* ptr, size_t size, size_t nmemb, void* userdata);
    using read_function_t = std::size_t (*)(void* ptr, size_t size, size_t nmemb, void* userdata);

    using seek_function_t = int (*)(void* instream, native::curl_off_t offset, int origin);
    using sockopt_function_t = int (*)(void* clientp, native::curl_socket_t curlfd, native::curlsocktype purpose);

    void perform();
    void perform(std::error_code& ec);
    void async_perform(handler_type handler);
    void cancel();
    void reset();

// public sys
    void handle_completion(const std::error_code& ec);
    void set_progress_callback(progress_function_t func);
    void unset_progress_callback();

// public static     
    static easy_mime* from_native(native::CURL* native_easy);
    static std::shared_ptr<const easy_mime> create_easy_mime_blocking();

// public
    std::error_code rate_limit_error() const;
    time_point::duration time_to_start() const;

// public inline     
    inline native::CURL* native_handle() { return easy_handle_; }
    inline const multi* get_multi() const { return multi_handle_; }
    inline const native::curl_mime* get_mime() const { return mime_; }

public:
    using detail_eha = detail::empty_header_action;
    using detail_dha = detail::duplicate_header_action;
    
    void add_header(std::string_view name, std::string_view value, 
            detail_eha eha = detail_eha::kSend, detail_dha dha = detail_dha::kAdd);
    void add_header(std::string_view name, std::string_view value, std::error_code& ec, 
            detail_eha eha = detail_eha::kSend, detail_dha dha = detail_dha::kAdd);
    void add_header(std::string_view name, std::string_view value, detail_dha dha);

    void add_header(const std::string& header);
    void add_header(const std::string& header, std::error_code& ec);

    void async_perform(handler_type handler);

public:
/// cURL Opt setters
    void set_verbose(bool state);
    void set_header(bool state);
    void set_no_progress(bool state);
    void set_no_signal(bool state);
    void set_wildcard_match(bool state);

    void set_source(std::shared_ptr<std::istream> source);
    void set_sink(std::string* sink);

    void set_write_function(write_function_t func, std::error_code& ec);
    void set_write_data(void* ptr, std::error_code& ec);
    void set_read_function(read_function_t func, std::error_code& ec);
    void set_read_data(void* ptr, std::error_code& ec);
    void set_seek_function(seek_function_t func, std::error_code& ec);
    void set_seek_data(void* ptr, std::error_code& ec);
    void set_sockopt_function(sockopt_function_t func);
    void set_sockopt_data(void* ptr);
    void set_xferinfo_function(xfer_function_t func);
    void set_xferinfo_data(void* ptr);
    void set_opensocket_function(opensocket_function_t func);
    void set_opensocket_data(void* ptr);
    void set_closesocket_function(closesocket_function_t func);
    void set_closesocket_data(void* ptr);
    
    void set_url(std::string_view url);
    void set_headers(std::shared_ptr<string_list> headers);
    void set_http_version(long version);
    //void set_http_post(std::unique_ptr<form_mime> mime);

// public getters
    std::shared_ptr<easy_mime> get_bound_blocking(multi& multi_handle) const;  

    std::string_view get_effective_url(std::error_code& ec);
    
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
    static size_t read_runction(void* ptr, size_t size, size_t nmemb, void* userdata) noexcept;
    static size_t write_function(char* ptr, size_t size, size_t nmemb, void* userdata) noexcept;
    static int xfer_function(void* clinetp, native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow) noexcept;

    native::curl_socket_t open_tcp_socket(struct native::curl_sockaddr* address) noexcept;

protected:
    engine::ev::ThreadControl& get_thread_control();

// cURL protected
    void set_source(std::shared_ptr<std::istream> source, std::error_code& ec);
    void set_sink(std::string* sink, std::error_code& ec);
    
// do_ev_* methods run in libev thread
    void do_ev_async_perform(handler_type handler, size_t request_num);
    void do_ev_cancel(size_t request_num);

    void mark_start_performing();
    void mark_open_socket();

    void cancel(size_t request_num);

protected:
    native::CURL*                   easy_handle_;
    native::curl_mime*              mime_;
    native::curl_mimepart*          mime_part_;
    multi*                          multi_handle_;
    
    size_t                          request_counter_        { 0 };
    size_t                          retries_counter_        { 0 };
    size_t                          cancelled_request_max_  { 0 };
    size_t                          sockets_opened_         { 0 };

    bool                            multi_registered_       { false };

    std::string*                    sink_                   { nullptr };
    std::string                     orig_url_str_           {};
    std::string                     post_fields_            {};                    

    std::shared_ptr<form_mime>      form_mime_;
    std::shared_ptr<share>          share_                  { nullptr };
    std::shared_ptr<std::istream>   source_                 { nullptr };
    std::shared_ptr<string_list>    headers_                { nullptr };
    std::shared_ptr<string_list>    proxy_headers_          { nullptr };
    std::shared_ptr<string_list>    http200_aliases_        { nullptr };
    std::shared_ptr<string_list>    resolved_hosts_         { nullptr };

    handler_type                    handler_;
    progress_function_t             progress_function_;

    std::error_code                 rate_limit_error_;

    time_point start_performing_ts_{};
    const time_point construct_ts_;
};
} // namespace curl

USERVER_NAMESPACE_END