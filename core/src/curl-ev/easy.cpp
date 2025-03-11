/** @file curl-ev/easy.hpp
        curl-ev: wrapper for integrating libcurl with libev applications
        Copyright (c) 2013 Oliver Kuckertz <oliver.kuckertz@mologie.de>
        See COPYING for license information.

        C++ wrapper for libcurl's easy interface
*/

// !TODO this @file curl-ev/easy.cpp upgrade in 2025 year                   //
// !@author <<< xensmo >>> Taras Litvinenko <taraslitvinenko@yandex.kz>     //

#include <sys/socket.h>
#include <unistd.h>

#include <istream>
#include <type_traits>
#include <utility>

#include <curl-ev/easy.hpp>
#include <curl-ev/error_code.hpp>
#include <curl-ev/multi.hpp>
#include <curl-ev/share.hpp>
#include <curl-ev/string_list.hpp>
#include <curl-ev/wrappers.hpp>

#include <engine/ev/thread_control.hpp>
#include <server/net/listener_impl.hpp>
#include <userver/engine/async.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/algo.hpp>
#include <userver/utils/str_icase.hpp>
#include <userver/utils/strerror.hpp>

USERVER_NAMESPACE_BEGIN

namespace curl {
namespace detail {
    using detail_eha = empty_header_action;
    using detail_dha = duplicate_header_action;

    bool is_header_matching_name(std::string_view header, std::string_view name) {
        return header.size() > name.size() && utils::StrIcaseEqual()(header.substr(0, name.size()), name) &&
            (header[name.size()] == ':' || header[name.size()] == ';');
    }

    fmt::memory_buffer 
    create_header_buffer(std::string_view name, std::string_view value, detail_eha eha) {
        fmt::memory_buffer fmt_mem_buf;

        if (eha == detail_eha::kSend && value.empty())
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

    bool add_header_do_skip(const std::shared_ptr<string_list>& headers, std::string_view name, detail_dha dha) {
        if (dha == detail_dha::kSkip && headers) 
            find_header_by_name(headers, name);
        return false;
    }

    bool add_header_do_replace(const std::shared_ptr<string_list>& headers, const fmt::memory_buffer& mem_buf,
            std::string_view name, detail_dha dha) {
        if (dha == detail_dha::kReplace && headers) {
            const bool replaced = headers->ReplaceFirstIf([name](std::string_view header) {
                return is_header_matching_name(header, name);
            }, mem_buf.data());

            if (replaced)
                return true;
        }
        return false;
    }

    template <typename _Et, typename = std::enable_if_t<std::is_enum_v<_Et>, _Et>>
    inline constexpr unsigned long to_integral(_Et val) {
        return static_cast<std::underlying_type_t<_Et>>(val);
    }

    template <typename _FuncName, typename _Handle>
    inline void set_curl_opt(_FuncName name, _Handle handle, native::CURLoption opt, bool state) {
         std::error_code ec = std::error_code {static_cast<errc::EasyErrorCode>(
            native::curl_easy_setopt(handle, opt, state ? 1L : 0L)
        )};
        throw_error(ec, name);
    }

    template <typename _FuncName, typename _Handle, typename _OptType>
    inline void set_curl_opt(_FuncName name, _Handle handle, native::CURLoption opt, _OptType arg) {
        std::error_code ec = std::error_code {static_cast<errc::EasyErrorCode>(
            native::curl_easy_setopt(handle, opt, arg)
        )};
        throw_error(ec, name);
    }

    template <typename _Handle, typename _OptType>
    inline std::error_code set_curl_opt(_Handle handle, native::CURLoption opt, _OptType arg) {
        std::error_code ec = std::error_code {static_cast<errc::EasyErrorCode>(
            native::curl_easy_setopt(handle, opt, arg)
        )};
        return ec;
    }

    template <typename _FuncName, typename _Handle>
    inline void set_curl_opt_blob(_FuncName name, std::string_view sv, unsigned int flags, _Handle handle, native::CURLoption opt) {
        native::curl_blob blob{};
        blob.data = const_cast<void*>(static_cast<const void*>(sv.data()));
        blob.len = sv.size();
        blob.flags = flags;
        std::error_code ec = std::error_code {static_cast<errc::EasyErrorCode>(
            native::curl_easy_setopt(handle, opt, &blob)
        )};
        throw_error(ec, name);
    }

    template <typename _Handle, typename _Return>
    inline std::error_code get_curl_info(_Handle handle, native::CURLINFO info, _Return value) {
        std::error_code ec = std::error_code{static_cast<errc::EasyErrorCode>(
            native::curl_easy_getinfo(handle, info, value)
        )};
        return ec;
    }

    template <typename _FuncName, typename _Handle>
    inline std::string_view get_curl_info_string_view(_FuncName name, _Handle handle, native::CURLINFO info) {
        char* result = nullptr;
        std::error_code ec = get_curl_info(handle, info, &result);
        throw_error(ec, name);
        return result ? result : std::string_view{};
    }

    template <typename _Handle>
    inline std::string_view get_curl_info_string_view(_Handle handle, native::CURLINFO info, std::error_code& ec) {
        char* result = nullptr;
        ec = get_curl_info(handle, info, &result);
        return result ? result : std::string_view{};
    }

    template <typename _FuncName, typename _Handle>
    inline long get_curl_info_long(_FuncName name, _Handle handle, native::CURLINFO info) {
        long result;
        std::error_code ec = get_curl_info(handle, info, &result);
        throw_error(ec, name);
        return result;
    }

    template <typename _FuncName, typename _Handle>
    inline std::vector<std::string> get_curl_info_list(_FuncName name, _Handle handle, native::CURLINFO info) {
        std::vector<std::string> results;
        struct native::curl_slist* slist;
        std::error_code ec = get_curl_info(handle, info, &slist);
        throw_error(ec , name);
        struct native::curl_slist* it = slist;

        while (it) {
            results.emplace_back(it->data);
            it = it->next;
        }

        native::curl_slist_free_all(slist);
        return results;
    }

} // namespace detail

easy::easy(native::CURL* easy_handle, native::curl_mime* mime_ptr, multi* multi_ptr) :
    easy_handle_(easy_handle), 
    mime_{std::make_shared<mime>(mime_ptr)},
    multi_handle_(multi_ptr), 
    construct_ts_(std::chrono::steady_clock::now()) 
{    
    UASSERT(easy_handle_);        
    set_private(this);
}

easy::~easy() {
    cancel();

    if (easy_handle_) {
        native::curl_easy_cleanup(easy_handle_);
        easy_handle_ = nullptr;
    }
}

std::shared_ptr<const easy> easy::create_easy_blocking() {
    impl::CurlGlobal::Init();
    
    // Note: curl_easy_init() is blocking.
    auto* handle_ptr = native::curl_easy_init();
    if (!handle_ptr) { throw std::bad_alloc(); }

    auto* mime_ptr = native::curl_mime_init(handle_ptr);
    if (!mime_ptr) { throw std::bad_alloc(); }
    
    return std::make_shared<const easy>(handle_ptr, mime_ptr, nullptr);
}

std::shared_ptr<easy> easy::get_bound_blocking(multi& multi_handle) const {
     // Note: curl_easy_init() is blocking.
    auto* cloned_easy = native::curl_easy_duphandle(easy_handle_);
    if (!cloned_easy)
        throw std::bad_alloc();

    auto* cloned_mime = mime_->native_mime();
    if (!cloned_mime)
        throw std::bad_alloc();

    return std::make_shared<easy>(cloned_easy, cloned_mime, &multi_handle);
}

easy* easy::from_native(native::CURL* native_easy) {
    easy* easy_handle = nullptr;
    native::curl_easy_getinfo(native_easy, native::CURLINFO_PRIVATE, &easy_handle);
    return easy_handle;
}

engine::ev::ThreadControl& easy::get_thread_control() {
    return multi_handle_->GetThreadControl();
}

void easy::async_perform(handler_type handler) {
    LOG_TRACE() << "easy::async_perform start " << this;
    size_t request_num = ++request_counter_;

    if (multi_handle_) {
        multi_handle_->GetThreadControl().RunInEvLoopAsync(
            [self = shared_from_this(), this, handler = std::move(handler), request_num]() mutable {
                return do_ev_async_perform(handler, request_num);
            }
        );
    } else {
        throw std::runtime_error("no multi!");
    }
    LOG_TRACE() << "easy::async_perform finished " << this;
}

void easy::reset() {
    LOG_TRACE() << "easy::reset start " << this;

    orig_url_str_.clear();
    std::string{}.swap(post_fields_);
    mime_.reset();

    if (headers_)
        headers_->clear();
    if (proxy_headers_)
        proxy_headers_->clear();
    if (http200_aliases_)
        http200_aliases_->clear();
    if (resolved_hosts_)
        resolved_hosts_->clear();

    share_.reset();

    retries_counter_ = 0;
    sockets_opened_ = 0;
    rate_limit_error_.clear();

    set_custom_request(nullptr);
    set_no_body(false);
    set_post(false);

    std::error_code ec;
    set_ssl_ctx_data(nullptr, ec);
    if (!ec) {
        set_ssl_ctx_function(nullptr, ec);
    } else if(ec != errc::EasyErrorCode::kNotBuiltIn) {
        throw_error(ec, "set_ssl_ctx_data");
    }

    UASSERT(!multi_registered_);
    native::curl_easy_reset(easy_handle_);
    set_private(this);

    LOG_TRACE() << "easy::reset finished " << this;    
}

using BusyMarker = utils::statistics::BusyMarker;

void easy::do_ev_async_perform(handler_type handler, size_t request_num) {
    if (request_num <= cancelled_request_max_) {
        LOG_DEBUG() << "async_perform requests allready canceled";
        return;
    }

    LOG_TRACE() << "easy::do_ev_async_perform start " << this;
    mark_start_performing();

    if (!multi_handle_) 
        std::runtime_error("Attempt to perform async. Operation without assigning a multi object.");
    
    BusyMarker busy(multi_handle_->Statistics().get_busy_storage());

    cancel(request_num - 1);

    // open sock func & data
    set_opensocket_function(&easy::open_socket);
    set_opensocket_data(this);
    
    // close sock func & data
    set_closesocket_function(&easy::close_socket);
    set_closesocket_data(multi_handle_);

    handler_ = std::move(handler);
    multi_registered_ = true;

    LOG_TRACE() << "easy::do_ev_async_perform before multi_->add() " << this;
    multi_handle_->add(this);
}

void easy::do_ev_cancel(size_t request_num) {
    // RunInEvLoopAsync(do_ev_async_perform) and RunInEvLoopSync(do_ev_cancel) are
    // not synchronized. So we need to count last cancelled request to prevent its
    // execution in do_ev_async_perform()
    if (cancelled_request_max_ < request_num) 
        cancelled_request_max_ = request_num;
    if (multi_registered_) {
        BusyMarker busy(multi_handle_->Statistics().get_busy_storage());
        handle_completion(std::make_error_code(std::errc::operation_canceled));
        multi_handle_->remove(this);
    }
}

void easy::mark_start_performing() {
    if (start_performing_ts_ == time_point())
        start_performing_ts_ = std::chrono::steady_clock::now();
}

void easy::mark_open_socket() {
    ++sockets_opened_;
}

void easy::mark_retry() {
    ++retries_counter_;
}

void easy::handle_completion(const std::error_code& ec) {
    LOG_TRACE() << "easy::handle_completion easy = " << this;
    multi_registered_ = false;
    auto handler = std::function<void(std::error_code)>([](std::error_code) {});
    std::swap(handler, handler_);

    /* It's OK to call handler in libev thread context as it is limited to
     * Request::on_retry and Request::on_completed. All user code is executed in
     * coro context.
     */
    handler(ec);
}

void easy::cancel() {
    cancel(request_counter_);
}

void easy::cancel(size_t request_num) {
    if(multi_handle_) 
        multi_handle_->GetThreadControl().RunInEvLoopSync([this, request_num] { do_ev_cancel(request_num); });
}

std::error_code easy::rate_limit_error() const {
    return rate_limit_error_;
}

easy::time_point::duration easy::time_to_start() const {
    if (start_performing_ts_ != time_point{}) 
        return start_performing_ts_ - construct_ts_;
    return {};
}

clients::http::LocalStats easy::get_local_stats() {
    clients::http::LocalStats stats;

    stats.open_socket_count = sockets_opened_;
    stats.retries_count = retries_counter_;
    stats.time_to_connect = std::chrono::microseconds(get_connect_time_usec());
    stats.time_to_process = std::chrono::microseconds(get_total_time_usec());

    return stats;
}

const std::string& easy::get_post_data() const {
    return post_fields_;
}

std::string easy::extract_post_data() {
    auto result = std::move(post_fields_);
    set_post_fields({});
    return result;
}

bool easy::has_post_data() const {
    return !post_fields_.empty() || mime_;
}

void easy::add_header(std::string_view name, std::string_view value, detail_eha eha, detail_dha dha) {
    if (detail::add_header_do_skip(headers_, name, dha)) { return; }
    auto buffer = detail::create_header_buffer(name, value, eha);
    if (detail::add_header_do_replace(headers_, buffer, name, dha)) { return; }
    add_header(buffer.data());
}

void easy::add_header(std::string_view name, std::string_view value, detail_dha dha) {
    add_header(name, value, detail_eha::kSend, dha);
}

void easy::add_header(const char* header) {
    if (!headers_)
        headers_ = std::make_shared<string_list>();
    
    headers_->add(header);
    std::error_code ec = detail::set_curl_opt(
        easy_handle_, native::CURLOPT_HTTPHEADER, headers_->native_handle());
    throw_error(ec, "add_header");
}

void easy::add_header(const std::string& header) {
    add_header(header.c_str());
}

void easy::add_proxy_header(std::string_view name, std::string_view value, detail_eha eha, detail_dha dha) {
    if (detail::add_header_do_skip(proxy_headers_, name, dha)) { return; }
    auto buffer = detail::create_header_buffer(name, value, eha);
    if (detail::add_header_do_replace(proxy_headers_, buffer, name, dha)) { return; }
    add_proxy_header(buffer.data());
}

void easy::add_proxy_header(const char* header) {
    if (!proxy_headers_)
        proxy_headers_ = std::make_shared<string_list>();

    proxy_headers_->add(header);
    std::error_code ec = detail::set_curl_opt(
        easy_handle_, native::CURLOPT_PROXYHEADER, proxy_headers_->native_handle());
    throw_error(ec, "add_proxy_header");
}

void easy::add_proxy_header(const std::string& header) {
    add_proxy_header(header.c_str());
}

void easy::add_resolve(const std::string& host, const std::string& port, const std::string& addr) {
    if (!resolved_hosts_)
        resolved_hosts_ = std::make_shared<string_list>();

    auto host_port_addr = utils::StrCat(host, ":", port, ":", addr);
    const std::string_view host_port_view {host_port_addr.data(), host_port_addr.size() - addr.size()};

    if (!resolved_hosts_->ReplaceFirstIf(
        [host_port_view](const auto& entry) {
                // host_port_addr, of which we hold a string_view, might be moved in
                // ReplaceFirstIf, but it's guaranteed that this
                // lambda is not called after that.
                return std::string_view{entry}.substr(host_port_view.size()) == host_port_view;
        },
        std::move(host_port_addr)
    )) {
        UASSERT_MSG(!host_port_addr.empty(), "ReplaceFirstIf moved the string out, when it shouldn't have done so.");
        resolved_hosts_->add(std::move(host_port_addr));
    }

    set_resolves(resolved_hosts_);

    std::error_code ec = detail::set_curl_opt(
        easy_handle_, native::CURLOPT_RESOLVE, resolved_hosts_->native_handle());

    throw_error(ec, "add_resolve");
}

void easy::add_http200_alias(const std::string& http200_alias) {
    if (!http200_aliases_)
        http200_aliases_ = std::make_shared<string_list>();

    http200_aliases_->add(http200_alias);    
    std::error_code ec = detail::set_curl_opt(
        easy_handle_, native::CURLOPT_HTTP200ALIASES, http200_aliases_->native_handle());
    throw_error(ec, "add_http200_alias");
}

std::optional<std::string_view> easy::find_header_by_name(std::string_view name) const {
    return detail::find_header_by_name(headers_, name);
}

// setters

void easy::set_share(std::shared_ptr<share> share) {
    share_ = std::move(share);

    if (share) {
        std::error_code ec = detail::set_curl_opt(
            easy_handle_, native::CURLOPT_SHARE, share->native_handle());
        throw_error(ec, "set_share native_handle failed!");
    } else {
        std::error_code ec = detail::set_curl_opt(
            easy_handle_, native::CURLOPT_SHARE, nullptr);
        throw_error(ec, "set_share nullptr failed!");
    }
}

// cURL Opt setters
void easy::set_verbose(bool state) {
    detail::set_curl_opt(
        "set_verbose", easy_handle_, native::CURLOPT_VERBOSE, state);
}

void easy::set_header(bool state) {
    detail::set_curl_opt(
        "set_header", easy_handle_, native::CURLOPT_HEADER, state);
}

void easy::set_no_progress(bool state) {
    detail::set_curl_opt(
        "set_no_progress", easy_handle_, native::CURLOPT_NOPROGRESS, state);
}

void easy::set_no_signal(bool state) {
    detail::set_curl_opt(
        "set_no_signal", easy_handle_, native::CURLOPT_NOSIGNAL, state);
}

void easy::set_wildcard_match(bool state) {
    detail::set_curl_opt(
        "set_wildcard_match", easy_handle_, native::CURLOPT_WILDCARDMATCH, state);
}

void easy::set_post(bool state) {
    detail::set_curl_opt(
        "set_post", easy_handle_, native::CURLOPT_POST, state);
}

void easy::set_source(std::shared_ptr<std::istream> source) {
    std::error_code ec;
    source_ = std::move(source);
    set_read_function(&easy::read_runction, ec);
    if (!ec)
        set_read_data(this, ec);
    if (!ec)
        set_seek_function(&easy::seek_function, ec);
    if (!ec)
        set_seek_data(this, ec);

    throw_error(ec, "set_source");
}

void easy::set_sink(std::string* sink) {
    std::error_code ec;
    sink_ = sink;
    set_write_function(&easy::write_function, ec);
    if (!ec)
        set_write_data(this, ec);
    throw_error(ec, "set_sink");
}

void easy::set_private(void* ptr) {
    detail::set_curl_opt(
        "set_private", easy_handle_, native::CURLOPT_PRIVATE, ptr);
}

void easy::set_custom_request(const std::string& str) {
    detail::set_curl_opt(
        "set_custom_request", easy_handle_, native::CURLOPT_CUSTOMREQUEST, str.c_str());
}

void easy::set_new_directory_perms(long perms) {
    detail::set_curl_opt(
        "set_new_directory_perms", easy_handle_, native::CURLOPT_NEW_DIRECTORY_PERMS, perms);
}

void easy::set_new_file_perms(long perms) {
    detail::set_curl_opt(
        "set_new_file_perms", easy_handle_, native::CURLOPT_NEW_FILE_PERMS, perms);
}

void easy::set_url(std::string&& url_str, std::error_code& ec) {
    url_.SetAbsoluteUrl(url_str.c_str(), ec);
    if (!ec) {
        detail::set_curl_opt(
            "set_url", easy_handle_, native::CURLOPT_URL, url_.native_handle());
        throw_error(ec, "set_url native_handle failed!");
    }

    if (!ec) 
        orig_url_str_ = std::move(url_str);
    else
        orig_url_str_.clear();
}

void easy::set_progress_data(void* ptr) {
    detail::set_curl_opt(
        "set_progress_data", easy_handle_, native::CURLOPT_XFERINFODATA, ptr); // CURLOPT_PROGRESSDATA
}

void easy::set_error_buffer(char* buffer) {
    detail::set_curl_opt(
        "set_error_buffer", easy_handle_, native::CURLOPT_ERRORBUFFER, buffer);
}

void easy::set_stderr(FILE* file) {
    detail::set_curl_opt(
        "set_stderr", easy_handle_, native::CURLOPT_STDERR, file);
}

void easy::set_fail_on_error(bool state) {
    detail::set_curl_opt(
        "set_fail_on_error", easy_handle_, native::CURLOPT_FAILONERROR, state);
}

void easy::set_mimepost(std::unique_ptr<mime> mime_ptr) {
    mime_ = std::move(mime_ptr);
    if (mime_) {
        std::error_code ec = detail::set_curl_opt(
            easy_handle_, native::CURLOPT_MIMEPOST, mime_->native_mime());
        throw_error(ec, "set_mimepost native_mime failed!");
    } else {
        std::error_code ec = detail::set_curl_opt(
            easy_handle_, native::CURLOPT_MIMEPOST, nullptr);
        throw_error(ec, "set_mimepost nullptr failed!");
    }
}

void easy::set_http_proxy_tunnel(bool state) {
    detail::set_curl_opt(
        "set_http_proxy_tunnel", easy_handle_, native::CURLOPT_HTTPPROXYTUNNEL, state);
}

void easy::set_socks5_gsapi_nec(bool state) {
    detail::set_curl_opt(
        "set_socks5_gsapi_nec", easy_handle_, native::CURLOPT_SOCKS5_GSSAPI_NEC, state);
}

void easy::set_tcp_no_delay(bool state) {
    detail::set_curl_opt(
        "set_tcp_no_delay", easy_handle_, native::CURLOPT_TCP_NODELAY, state);
}

void easy::set_tcp_keep_alive(bool state) {
    detail::set_curl_opt(
        "set_tcp_keep_alive", easy_handle_, native::CURLOPT_TCP_KEEPALIVE, state);
}

void easy::set_auto_referrer(bool state) {
    detail::set_curl_opt(
        "set_auto_referrer", easy_handle_, native::CURLOPT_AUTOREFERER, state);
}

void easy::set_follow_location(bool state) {
    detail::set_curl_opt(
        "set_follow_location", easy_handle_, native::CURLOPT_FOLLOWLOCATION, state);
}

void easy::set_unrestricted_auth(bool state) {
    detail::set_curl_opt(
        "set_unrestricted_auth", easy_handle_, native::CURLOPT_UNRESTRICTED_AUTH, state);
}

void easy::set_max_redirs(long value) {
    detail::set_curl_opt(
        "set_max_redirs", easy_handle_, native::CURLOPT_MAXREDIRS, value);
}
void easy::set_post_redir(long value) {
    detail::set_curl_opt(
        "set_post_redir", easy_handle_, native::CURLOPT_POSTREDIR, value);
}

void easy::set_post_field_size(long value) {
    detail::set_curl_opt(
        "set_post_field_size", easy_handle_, native::CURLOPT_POSTFIELDSIZE, value);
}

void easy::set_proxy_port(long value) {
    detail::set_curl_opt(
        "set_proxy_port", easy_handle_, native::CURLOPT_PROXYPORT, value);
}

void easy::set_proxy_type(long value) {
    detail::set_curl_opt(
        "set_proxy_port", easy_handle_, native::CURLOPT_PROXYTYPE, value);
}

void easy::set_local_port(long value) {
    detail::set_curl_opt(
        "set_local_port", easy_handle_, native::CURLOPT_LOCALPORT, value);
}

void easy::set_local_port_range(long value) {
    detail::set_curl_opt(
        "set_local_port_range", easy_handle_, native::CURLOPT_LOCALPORTRANGE, value);
}

void easy::set_dns_cache_timeout(long value) {
    detail::set_curl_opt(
        "set_dns_cache_timeout", easy_handle_, native::CURLOPT_DNS_CACHE_TIMEOUT, value);
}

void easy::set_buffer_size(long value) {
    detail::set_curl_opt(
        "set_buffer_size", easy_handle_, native::CURLOPT_BUFFERSIZE, value);
}

void easy::set_port(long value) {
    detail::set_curl_opt(
        "set_port", easy_handle_, native::CURLOPT_PORT, value);
}

void easy::set_address_scope(long value) {
    detail::set_curl_opt(
        "set_address_scope", easy_handle_, native::CURLOPT_ADDRESS_SCOPE, value);
}

void easy::set_tcp_keep_idle(long value) {
    detail::set_curl_opt(
        "set_tcp_keep_idle", easy_handle_, native::CURLOPT_TCP_KEEPIDLE, value);
}

void easy::set_tcp_keep_intvl(long value) {
    detail::set_curl_opt(
        "set_tcp_keep_intvl", easy_handle_, native::CURLOPT_TCP_KEEPINTVL, value);
}

void easy::set_connect_to(native::curl_slist* slist) {
    detail::set_curl_opt(
        "set_connect_to", easy_handle_, native::CURLOPT_CONNECT_TO, slist);
}

void easy::set_post_field_size_large(native::curl_off_t value) {
    detail::set_curl_opt(
        "set_post_field_size_large", easy_handle_, native::CURLOPT_POSTFIELDSIZE_LARGE, value);
}

void easy::set_post_fields(void* ptr) {
    detail::set_curl_opt(
        "set_post_fields", easy_handle_, native::CURLOPT_POSTFIELDS, ptr);
}

void easy::set_post_fields(std::string&& post_fields) {
    post_fields_ = std::move(post_fields);
    std::error_code ec;
    if (!post_fields_.empty())
        ec = detail::set_curl_opt(easy_handle_, native::CURLOPT_POSTFIELDS, post_fields_.data());

    if (!ec)
        set_post_field_size_large(static_cast<native::curl_off_t>(post_fields_.size()));

    throw_error(ec, "set_post_fields");
}

void easy::set_proxy(std::string_view sv) {
    detail::set_curl_opt(
        "set_proxy", easy_handle_, native::CURLOPT_PROXY, sv.data());
}

void easy::set_no_proxy(std::string_view sv) {
    detail::set_curl_opt(
        "set_no_proxy", easy_handle_, native::CURLOPT_NOPROXY, sv.data());
}

void easy::set_interface(std::string_view sv) {
    detail::set_curl_opt(
        "set_interface", easy_handle_, native::CURLOPT_INTERFACE, sv.data());
}

void easy::set_unix_socket_path(std::string_view sv) {
    detail::set_curl_opt(
        "set_unix_socket_path", easy_handle_, native::CURLOPT_UNIX_SOCKET_PATH, sv.data());
}

void easy::set_accept_encoding(const std::string& str) {
    detail::set_curl_opt(
        "set_accept_encoding", easy_handle_, native::CURLOPT_ACCEPT_ENCODING, str.data());
}

void easy::set_transfer_encoding(std::string_view sv) {
    detail::set_curl_opt(
        "set_transfer_encoding", easy_handle_, native::CURLOPT_TRANSFER_ENCODING, sv.data());
}

void easy::set_referer(std::string_view sv) {
    detail::set_curl_opt(
        "set_referer", easy_handle_, native::CURLOPT_REFERER, sv.data());
}

void easy::set_user_agent(std::string_view sv) {
    detail::set_curl_opt(
        "set_user_agent", easy_handle_, native::CURLOPT_USERAGENT, sv.data());
}

void easy::set_protocols_str(const std::string& str) {
    detail::set_curl_opt(
        "set_protocols", easy_handle_, native::CURLOPT_PROTOCOLS_STR, str.c_str());
}

void easy::set_redir_protocols_str(const std::string& str) {
    detail::set_curl_opt(
        "set_redir_protocols_str", easy_handle_, native::CURLOPT_REDIR_PROTOCOLS_STR, str.c_str());
}

void easy::set_headers(std::shared_ptr<string_list> headers) {
    headers_ = std::move(headers);

    if (headers_) {
        std::error_code ec = detail::set_curl_opt(
            easy_handle_, native::CURLOPT_HTTPHEADER, headers_->native_handle());
        throw_error(ec, "set_headers native_handle failed!");
    } else {
        std::error_code ec = detail::set_curl_opt(
            easy_handle_, native::CURLOPT_HTTPHEADER, nullptr);
        throw_error(ec, "set_headers nullptr failed!");
    }
}

void easy::set_http200_aliases(std::shared_ptr<string_list> http200_aliases) {
    http200_aliases_ = std::move(http200_aliases);

    if (http200_aliases_) {
        std::error_code ec = detail::set_curl_opt(
            easy_handle_, native::CURLOPT_HTTP200ALIASES, http200_aliases_->native_handle());
        throw_error(ec, "set_http200_aliases native_handle failed!");
    } else {
        std::error_code ec = detail::set_curl_opt(
            easy_handle_, native::CURLOPT_HTTP200ALIASES, nullptr);
        throw_error(ec, "set_http200_aliases nullptr failed!");
    }
}

void easy::set_tls_auth_type(long value) {
    detail::set_curl_opt(
        "set_tls_auth_type", easy_handle_, native::CURLOPT_TLSAUTH_TYPE, value);
}

void easy::set_tls_auth_user(std::string_view sv) {
    detail::set_curl_opt(
        "set_tls_auth_user", easy_handle_, native::CURLOPT_TLSAUTH_USERNAME, sv.data());
}

void easy::set_tls_auth_password(std::string_view sv) {
    detail::set_curl_opt(
        "set_tls_auth_password", easy_handle_, native::CURLOPT_TLSAUTH_PASSWORD, sv.data());
}

void easy::set_netrc_file(std::string_view sv) {
    detail::set_curl_opt(
        "set_netrc_file", easy_handle_, native::CURLOPT_NETRC_FILE, sv.data());
}

void easy::set_user(std::string_view sv) {
    detail::set_curl_opt(
        "set_user", easy_handle_, native::CURLOPT_USERNAME, sv.data());
}

void easy::set_password(std::string_view sv) {
    detail::set_curl_opt(
        "set_password", easy_handle_, native::CURLOPT_PASSWORD, sv.data());
}

void easy::set_proxy_user(std::string_view sv) {
    detail::set_curl_opt(
        "set_proxy_user", easy_handle_, native::CURLOPT_PROXYUSERNAME, sv.data());
}

void easy::set_proxy_password(std::string_view sv) {
    detail::set_curl_opt(
        "set_proxy_password", easy_handle_, native::CURLOPT_PROXYPASSWORD, sv.data());
}

void easy::set_netrc(detail::netrc_t netrc) {
    detail::set_curl_opt(
        "set_netrc", easy_handle_, native::CURLOPT_NETRC, detail::to_integral(netrc));
}

void easy::set_http_auth(detail::httpauth_t httpauth, bool auth_only) {
    auto result = detail::to_integral(httpauth) | (auth_only ? CURLAUTH_ONLY : 0UL);
    detail::set_curl_opt(
        "set_http_auth", easy_handle_, native::CURLOPT_HTTPAUTH, result);
}

void easy::set_proxy_auth(detail::proxyauth_t proxyauth) {
    detail::set_curl_opt(
        "set_proxy_auth", easy_handle_, native::CURLOPT_PROXYAUTH, detail::to_integral(proxyauth));
}

void easy::set_transfer_text(bool state) {
    detail::set_curl_opt(
        "set_transfer_text", easy_handle_, native::CURLOPT_TRANSFERTEXT, state);
}

void easy::set_transfer_mode(bool state) {
    detail::set_curl_opt(
        "set_transfer_mode", easy_handle_, native::CURLOPT_PROXY_TRANSFER_MODE, state);
}

void easy::set_crlf(bool state) {
    detail::set_curl_opt(
        "set_crlf", easy_handle_, native::CURLOPT_CRLF, state);
}

void easy::set_file_time(bool state) {
    detail::set_curl_opt(
        "set_file_time", easy_handle_, native::CURLOPT_FILETIME, state);
}

void easy::set_upload(bool state) {
    detail::set_curl_opt(
        "set_upload", easy_handle_, native::CURLOPT_UPLOAD, state);
}

void easy::set_no_body(bool state) {
    detail::set_curl_opt(
        "set_no_body", easy_handle_, native::CURLOPT_NOBODY, state);
}

void easy::set_ignore_content_length(bool state) {
    detail::set_curl_opt(
        "set_ignore_content_length", easy_handle_, native::CURLOPT_IGNORE_CONTENT_LENGTH, state);
}

void easy::set_http_content_decoding(bool state) {
    detail::set_curl_opt(
        "set_http_content_decoding", easy_handle_, native::CURLOPT_HTTP_CONTENT_DECODING, state);
}

void easy::set_http_transfer_decoding(bool state) {
    detail::set_curl_opt(
        "set_http_transfer_decoding", easy_handle_, native::CURLOPT_HTTP_TRANSFER_DECODING, state);
}

void easy::set_http_get(bool state) {
    detail::set_curl_opt(
        "set_http_get", easy_handle_, native::CURLOPT_HTTPGET, state);
}

void easy::set_cookie_session(bool state) {
    detail::set_curl_opt(
        "set_cookie_session", easy_handle_, native::CURLOPT_COOKIESESSION, state);
}

void easy::set_resume_from(long value) {
    detail::set_curl_opt(
        "set_resume_from", easy_handle_, native::CURLOPT_RESUME_FROM, value);
}

void easy::set_in_file_size(long value) {
    detail::set_curl_opt(
        "set_in_file_size", easy_handle_, native::CURLOPT_INFILESIZE, value);
}

void easy::set_max_file_size(long value) {
    detail::set_curl_opt(
        "set_max_file_size", easy_handle_, native::CURLOPT_MAXFILESIZE, value);
}

void easy::set_time_value(long value) {
    detail::set_curl_opt(
        "set_time_value", easy_handle_, native::CURLOPT_TIMEVALUE, value);
}

void easy::set_resume_from_large(native::curl_off_t value) {
    detail::set_curl_opt(
        "set_resume_from_large", easy_handle_, native::CURLOPT_RESUME_FROM_LARGE, value);
}

void easy::set_in_file_size_large(native::curl_off_t value) {
    detail::set_curl_opt(
        "set_in_file_size_large", easy_handle_, native::CURLOPT_INFILESIZE_LARGE, value);
}

void easy::set_max_file_size_large(native::curl_off_t value) {
    detail::set_curl_opt(
        "set_max_file_size_large", easy_handle_, native::CURLOPT_MAXFILESIZE_LARGE, value);
}

void easy::set_range(std::string_view sv) {
    detail::set_curl_opt(
        "set_range", easy_handle_, native::CURLOPT_RANGE, sv.data());
}

void easy::set_cookie_list(std::string_view sv) {
    detail::set_curl_opt(
        "set_cookie_list", easy_handle_, native::CURLOPT_COOKIELIST, sv.data());
}

void easy::set_cookie(std::string_view sv) {
    detail::set_curl_opt(
        "set_cookie", easy_handle_, native::CURLOPT_COOKIE, sv.data());
}

void easy::set_cookie_file(std::string_view sv) {
    detail::set_curl_opt(
        "set_cookie_file", easy_handle_, native::CURLOPT_COOKIEFILE, sv.data());
}

void easy::set_cookie_jar(std::string_view sv) {
    detail::set_curl_opt(
        "set_cookie_jar", easy_handle_, native::CURLOPT_COOKIEJAR, sv.data());
}

void easy::set_http_version(detail::http_version_t version) {
    detail::set_curl_opt(
        "set_http_version", easy_handle_, native::CURLOPT_HTTP_VERSION, detail::to_integral(version));
}

void easy::set_time_condition(detail::time_condition_t condition) {
    detail::set_curl_opt(
        "set_time_condition", easy_handle_, native::CURLOPT_TIMECONDITION, detail::to_integral(condition));
}

void easy::set_fresh_connect(bool state) {
    detail::set_curl_opt(
        "set_fresh_connect", easy_handle_, native::CURLOPT_FRESH_CONNECT, state);
}

void easy::set_forbit_reuse(bool state) {
    detail::set_curl_opt(
        "set_forbit_reuse", easy_handle_, native::CURLOPT_FORBID_REUSE, state);
}

void easy::set_connect_only(bool state) {
    detail::set_curl_opt(
        "set_connect_only", easy_handle_, native::CURLOPT_CONNECT_ONLY, state);
}

void easy::set_timeout(long timeout) {
    detail::set_curl_opt(
        "set_timeout", easy_handle_, native::CURLOPT_TIMEOUT, timeout);
}

void easy::set_timeout_ms(long timeout) {
    detail::set_curl_opt(
        "set_timeout_ms", easy_handle_, native::CURLOPT_TIMEOUT_MS, timeout);
}

void easy::set_low_speed_limit(long limit) {
    detail::set_curl_opt(
        "set_low_speed", easy_handle_, native::CURLOPT_LOW_SPEED_LIMIT, limit);
}

void easy::set_low_speed_time(long time) {
    detail::set_curl_opt(
        "set_low_speed_time", easy_handle_, native::CURLOPT_LOW_SPEED_TIME, time);
}

void easy::set_max_connects(long connects) {
    detail::set_curl_opt(
        "set_max_connects", easy_handle_, native::CURLOPT_MAXCONNECTS, connects);
}

void easy::set_connect_timeout(long timeout) {
    detail::set_curl_opt(
        "set_connect_timeout", easy_handle_, native::CURLOPT_CONNECTTIMEOUT, timeout);
}

void easy::set_connect_timeout_ms(long timeout) {
    detail::set_curl_opt(
        "set_connect_timeout_ms", easy_handle_, native::CURLOPT_CONNECTTIMEOUT_MS, timeout);
}

void easy::set_accept_timeout_ms(long timeout) {
    detail::set_curl_opt(
        "set_accept_timeout_ms", easy_handle_, native::CURLOPT_ACCEPTTIMEOUT_MS, timeout);
}

void easy::set_max_send_speed_large(native::curl_off_t large) {
    detail::set_curl_opt(
        "set_max_send_speed_large", easy_handle_, native::CURLOPT_MAX_SEND_SPEED_LARGE, large);
}

void easy::set_max_recv_speed_large(native::curl_off_t large) {
    detail::set_curl_opt(
        "set_max_recv_speed_large", easy_handle_, native::CURLOPT_MAX_RECV_SPEED_LARGE, large);
}

void easy::set_dns_servers(std::string_view sv) {
    detail::set_curl_opt(
        "set_dns_servers", easy_handle_, native::CURLOPT_DNS_SERVERS, sv.data());
}

void easy::set_ip_resolve(detail::ip_resolve_t resolve) {
    detail::set_curl_opt(
        "set_ip_resolve", easy_handle_, native::CURLOPT_IPRESOLVE, detail::to_integral(resolve));
}

void easy::set_resolves(std::shared_ptr<string_list> resolved_hosts) {
    resolved_hosts_ = std::move(resolved_hosts);
    if (resolved_hosts_) {
        std::error_code ec = detail::set_curl_opt(
            easy_handle_, native::CURLOPT_RESOLVE, resolved_hosts_->native_handle());
        throw_error(ec, "set_resolves native_handle failed!");
    } else {
        std::error_code ec = detail::set_curl_opt(
            easy_handle_, native::CURLOPT_RESOLVE, nullptr);
        throw_error(ec, "set_resolves nullptr failed!");
    }
}

void easy::set_ssh_auth_types(long types) {
    detail::set_curl_opt(
        "set_ssh_auth_types", easy_handle_, native::CURLOPT_SSH_AUTH_TYPES, types);
}

void easy::set_ssh_host_public_key_md5(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssh_host_public_key_md5", easy_handle_, native::CURLOPT_SSH_HOST_PUBLIC_KEY_MD5, sv.data());
}

void easy::set_ssh_public_key_file(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssh_public_key_file", easy_handle_, native::CURLOPT_SSH_PUBLIC_KEYFILE, sv.data());
}

void easy::set_ssh_private_key_file(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssh_private_key_file", easy_handle_, native::CURLOPT_SSH_PRIVATE_KEYFILE, sv.data());
}

void easy::set_ssh_known_hosts(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssh_known_hosts", easy_handle_, native::CURLOPT_SSH_KNOWNHOSTS, sv.data());
}

void easy::set_ssh_key_function(void* ptr) {
    detail::set_curl_opt(
        "set_ssh_key_function", easy_handle_, native::CURLOPT_SSH_KEYFUNCTION, ptr);
}

void easy::set_ssh_key_data(void* ptr) {
    detail::set_curl_opt(
        "set_ssh_key_data", easy_handle_, native::CURLOPT_SSH_KEYDATA, ptr);
}

void easy::set_ssl_cert(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssl_cert", easy_handle_, native::CURLOPT_SSLCERT, sv.data());
}

void easy::set_ssl_cert_type(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssl_cert_type", easy_handle_, native::CURLOPT_SSLCERTTYPE, sv.data());
}

void easy::set_ssl_key(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssl_key", easy_handle_, native::CURLOPT_SSLKEY, sv.data());
}

void easy::set_ssl_key_type(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssl_key_type", easy_handle_, native::CURLOPT_SSLKEYTYPE, sv.data());
}

void easy::set_ssl_key_passwd(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssl_key_passwd", easy_handle_, native::CURLOPT_KEYPASSWD, sv.data());
}

void easy::set_ssl_engine(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssl_engine", easy_handle_, native::CURLOPT_SSLENGINE, sv.data());
}

void easy::set_ssl_engine_default(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssl_engine_default", easy_handle_, native::CURLOPT_SSLENGINE_DEFAULT, sv.data());
}

void easy::set_ca_info(std::string_view sv) {
    detail::set_curl_opt(
        "set_ca_info", easy_handle_, native::CURLOPT_CAINFO, sv.data());
}

void easy::set_ca_file(std::string_view sv) {
    detail::set_curl_opt(
        "set_ca_file", easy_handle_, native::CURLOPT_CAPATH, sv.data());
}

void easy::set_crl_file(std::string_view sv) {
    detail::set_curl_opt(
        "set_crl_file", easy_handle_, native::CURLOPT_CRLFILE, sv.data());
}

void easy::set_issuer_cert(std::string_view sv) {
    detail::set_curl_opt(
        "set_issuer_cert", easy_handle_, native::CURLOPT_ISSUERCERT, sv.data());
}

void easy::set_ssl_cipher_list(std::string_view sv) {
    detail::set_curl_opt(
        "set_ssl_cipher_list", easy_handle_, native::CURLOPT_SSL_CIPHER_LIST, sv.data());
}

void easy::set_krb_level(std::string_view sv) {
    detail::set_curl_opt(
        "set_krb_level", easy_handle_, native::CURLOPT_KRBLEVEL, sv.data());
}

void easy::set_ssl_options(long options) {
    detail::set_curl_opt(
        "set_ssl_options", easy_handle_, native::CURLOPT_SSL_OPTIONS, options);
}

void easy::set_gssapi_delegation(long arg) {
    detail::set_curl_opt(
        "set_gssapi_delegation", easy_handle_, native::CURLOPT_GSSAPI_DELEGATION, arg);
}

void easy::set_cert_info(bool state) {
    detail::set_curl_opt(
        "set_cert_info", easy_handle_, native::CURLOPT_CERTINFO, state);
}

void easy::set_ssl_session_id_cache(bool state) {
    detail::set_curl_opt(
        "set_ssl_session_id_cache", easy_handle_, native::CURLOPT_SSL_SESSIONID_CACHE, state);
}

void easy::set_ssl_verify_peer(bool state) {
    detail::set_curl_opt("set_ssl_verify_peer", easy_handle_, native::CURLOPT_SSL_VERIFYPEER, state);
}

void easy::set_ssl_verify_host(bool state) {
    std::error_code ec = detail::set_curl_opt(easy_handle_, native::CURLOPT_SSL_VERIFYHOST, state ? 2L : 0L);
    throw_error(ec, "set_ssl_verify_host");
}

void easy::set_ssl_version(detail::ssl_version_t version) {
    detail::set_curl_opt(
        "set_ssl_version", easy_handle_, native::CURLOPT_SSLVERSION, detail::to_integral(version));
}

void easy::set_use_ssl(detail::use_ssl_t ssl) {
    detail::set_curl_opt(
        "set_use_ssl", easy_handle_, native::CURLOPT_USE_SSL, detail::to_integral(ssl));
}

void easy::set_ssl_cert_blob_copy(std::string_view sv) {
    detail::set_curl_opt_blob(
        "set_ssl_cert_blob_copy", sv, CURL_BLOB_COPY, easy_handle_, native::CURLOPT_SSLCERT_BLOB);
}

void easy::set_ssl_cert_blob_no_copy(std::string_view sv) {
    detail::set_curl_opt_blob(
        "set_ssl_cert_blob_no_copy", sv, CURL_BLOB_NOCOPY, easy_handle_, native::CURLOPT_SSLCERT_BLOB);
}

void easy::set_ssl_key_blob_copy(std::string_view sv) {
    detail::set_curl_opt_blob(
        "set_ssl_key_blob_copy", sv, CURL_BLOB_COPY, easy_handle_, native::CURLOPT_SSLKEY_BLOB);
}

void easy::set_ssl_key_blob_no_copy(std::string_view sv) {
    detail::set_curl_opt_blob(
        "set_ssl_key_blob_no_copy", sv, CURL_BLOB_NOCOPY, easy_handle_, native::CURLOPT_SSLKEY_BLOB);
}

void easy::set_ca_info_blob_copy(std::string_view sv) {
    detail::set_curl_opt_blob(
        "set_ca_info_blob_copy", sv, CURL_BLOB_COPY, easy_handle_, native::CURLOPT_CAINFO_BLOB);
}

void easy::set_ca_info_blob_no_copy(std::string_view sv) {
    detail::set_curl_opt_blob(
        "set_ca_info_blob_no_copy", sv, CURL_BLOB_NOCOPY, easy_handle_, native::CURLOPT_CAINFO_BLOB);
}

// * puplic callback function @fn set_* @return none
void easy::set_progress_callback(progress_function_t func) {
    progress_function_ = std::move(func);
    set_no_progress(false);
    set_xferinfo_function(&easy::xfer_function);
    set_xferinfo_data(this);
}

void easy::unset_progress_callback() {
    set_no_progress(true);
    set_xferinfo_function(nullptr);
    set_xferinfo_data(nullptr);
}

void easy::set_header_function(header_function_t func) {
    detail::set_curl_opt(
        "set_header_function", easy_handle_, native::CURLOPT_HEADERFUNCTION, func);
}

void easy::set_header_data(void* ptr) {
    detail::set_curl_opt(
        "set_header_data", easy_handle_, native::CURLOPT_HEADERDATA, ptr);
}

void easy::set_debug_function(debug_function_t func) {
    detail::set_curl_opt(
        "set_debug_function", easy_handle_, native::CURLOPT_DEBUGFUNCTION, func);
}

void easy::set_debug_data(void* ptr) {
    detail::set_curl_opt(
        "set_debug_data", easy_handle_, native::CURLOPT_DEBUGDATA, ptr);
}

void easy::set_interleave_function(interleave_function_t func) {
    detail::set_curl_opt(
        "set_interleave_function", easy_handle_, native::CURLOPT_INTERLEAVEFUNCTION, func);
}

void easy::set_interleave_data(void* ptr) {
    detail::set_curl_opt(
        "set_interleave_data", easy_handle_, native::CURLOPT_INTERLEAVEDATA, ptr);
}

void easy::set_write_function(write_function_t func, std::error_code& ec) {
    ec = detail::set_curl_opt(easy_handle_, native::CURLOPT_WRITEFUNCTION, func);
}

void easy::set_write_data(void* ptr, std::error_code& ec) {
    ec = detail::set_curl_opt(easy_handle_, native::CURLOPT_WRITEDATA, ptr);
}

void easy::set_read_function(read_function_t func, std::error_code& ec) {
    ec = detail::set_curl_opt(easy_handle_, native::CURLOPT_READFUNCTION, func);
}

void easy::set_read_data(void* ptr, std::error_code& ec) {
    ec = detail::set_curl_opt(easy_handle_, native::CURLOPT_READDATA, ptr);
}

void easy::set_seek_function(seek_function_t func, std::error_code& ec) {
    ec = detail::set_curl_opt(easy_handle_, native::CURLOPT_SEEKFUNCTION, func);
}

void easy::set_seek_data(void* ptr, std::error_code& ec) {
    ec = detail::set_curl_opt(easy_handle_, native::CURLOPT_SEEKDATA, ptr);
}

void easy::set_sockopt_function(sockopt_function_t func) {
    detail::set_curl_opt(
        "set_sockopt_function", easy_handle_, native::CURLOPT_SOCKOPTFUNCTION, func);
}

void easy::set_sockopt_data(void* ptr) {
    detail::set_curl_opt(
        "set_sockopt_data", easy_handle_, native::CURLOPT_SOCKOPTDATA, ptr);
}

void easy::set_xferinfo_function(xfer_function_t func) {
    detail::set_curl_opt(
        "set_xferinfo_function", easy_handle_, native::CURLOPT_XFERINFOFUNCTION, func);
}

void easy::set_xferinfo_data(void* ptr) {
    detail::set_curl_opt(
        "set_xferinfo_data", easy_handle_, native::CURLOPT_XFERINFODATA, ptr);
}

void easy::set_opensocket_function(opensocket_function_t func) {
    detail::set_curl_opt(
        "set_opensocket_function", easy_handle_, native::CURLOPT_OPENSOCKETFUNCTION, func);
}

void easy::set_opensocket_data(void* ptr) {
    detail::set_curl_opt(
        "set_opensocket_data", easy_handle_, native::CURLOPT_OPENSOCKETDATA, ptr);
}

void easy::set_closesocket_function(closesocket_function_t func) {
    detail::set_curl_opt(
        "set_closesocket_function", easy_handle_, native::CURLOPT_CLOSESOCKETFUNCTION, func);
}

void easy::set_closesocket_data(void* ptr) {
    detail::set_curl_opt(
        "set_closesocket_data", easy_handle_, native::CURLOPT_CLOSESOCKETDATA, ptr);
}

void easy::set_ssl_ctx_function(ssl_ctx_function_t func, std::error_code& ec) {
    ec = detail::set_curl_opt(easy_handle_, native::CURLOPT_SSL_CTX_FUNCTION, func);
}

void easy::set_ssl_ctx_data(void* ptr, std::error_code& ec) {
    ec = detail::set_curl_opt(easy_handle_, native::CURLOPT_SSL_CTX_DATA, ptr);
}

// public getters
std::string_view easy::get_effective_url() {
    std::error_code ec;
    std::string_view sv = get_effective_url(ec);
    throw_error(ec, "get_effective_url");
    return sv;
}
std::string_view easy::get_effective_url(std::error_code& ec) {
    return detail::get_curl_info_string_view(easy_handle_, native::CURLINFO_EFFECTIVE_URL, ec);
}

std::vector<std::string> easy::get_ssl_engines() {
    return detail::get_curl_info_list(
        "get_ssl_engines", easy_handle_, native::CURLINFO_SSL_ENGINES);
}

std::vector<std::string> easy::get_cookielist() {
    return detail::get_curl_info_list(
        "get_cookielist", easy_handle_, native::CURLINFO_COOKIELIST);
}

std::string_view easy::get_scheme() {
    return detail::get_curl_info_string_view(
        "get_scheme", easy_handle_, native::CURLINFO_SCHEME);
}

std::string_view easy::get_local_ip() {
    return detail::get_curl_info_string_view(
        "get_local_ip", easy_handle_, native::CURLINFO_LOCAL_IP);
}

std::string_view easy::get_rtsp_session_id() {
    return detail::get_curl_info_string_view(
        "get_rtsp_session_id", easy_handle_, native::CURLINFO_RTSP_SESSION_ID);
}

std::string_view easy::get_primary_ip() {
    return detail::get_curl_info_string_view(
        "get_primary_ip", easy_handle_, native::CURLINFO_PRIMARY_IP);
}

std::string_view easy::get_redirect_url() {
    return detail::get_curl_info_string_view(
        "get_redirect_url", easy_handle_, native::CURLINFO_REDIRECT_URL);
}

std::string_view easy::get_ftp_entry_path() {
    return detail::get_curl_info_string_view(
        "get_ftp_entry_path", easy_handle_, native::CURLINFO_FTP_ENTRY_PATH);
}

std::string_view easy::get_content_type() {
    return detail::get_curl_info_string_view(
        "get_content_type", easy_handle_, native::CURLINFO_CONTENT_TYPE);
}

long easy::get_http_version() {
    return detail::get_curl_info_long(
        "get_http_version", easy_handle_, native::CURLINFO_HTTP_VERSION);
}

long easy::get_proxy_ssl_verifyresult() {
    return detail::get_curl_info_long(
        "get_proxy_ssl_verifyresult", easy_handle_, native::CURLINFO_PROXY_SSL_VERIFYRESULT);
}

long easy::get_local_port() {
    return detail::get_curl_info_long(
        "get_local_port", easy_handle_, native::CURLINFO_LOCAL_PORT);
}

long easy::get_primary_port() {
    return detail::get_curl_info_long(
        "get_primary_port", easy_handle_, native::CURLINFO_PRIMARY_PORT);
}

long easy::get_rtsp_cseq_recv() {
    return detail::get_curl_info_long(
        "get_rtsp_cseq_recv", easy_handle_, native::CURLINFO_RTSP_CSEQ_RECV);
}

long easy::get_rtsp_server_cseq() {
    return detail::get_curl_info_long(
        "get_rtsp_server_cseq", easy_handle_, native::CURLINFO_RTSP_SERVER_CSEQ);
}

long easy::get_rtsp_client_cseq() {
    return detail::get_curl_info_long(
        "get_rtsp_client_cseq", easy_handle_, native::CURLINFO_RTSP_CLIENT_CSEQ);
}

long easy::get_condition_unmet() {
    return detail::get_curl_info_long(
        "get_condition_unmet", easy_handle_, native::CURLINFO_CONDITION_UNMET);
}

long easy::get_num_connects() {
    return detail::get_curl_info_long(
        "get_num_connects", easy_handle_, native::CURLINFO_NUM_CONNECTS);
}

long easy::get_os_errno() {
    return detail::get_curl_info_long(
        "get_os_errno", easy_handle_, native::CURLINFO_OS_ERRNO);
}

long easy::get_proxyauth_avail() {
    return detail::get_curl_info_long(
        "get_proxyauth_avail", easy_handle_, native::CURLINFO_PROXYAUTH_AVAIL);
}

long easy::get_httpauth_avail() {
    return detail::get_curl_info_long(
        "get_httpauth_avail", easy_handle_, native::CURLINFO_HTTPAUTH_AVAIL);
}

long easy::get_http_connectcode() {
    return detail::get_curl_info_long(
        "get_http_connectcode", easy_handle_, native::CURLINFO_HTTP_CONNECTCODE);
}

long easy::get_redirect_count() {
    return detail::get_curl_info_long(
        "get_redirect_count", easy_handle_, native::CURLINFO_REDIRECT_COUNT);
}

long easy::get_redirect_time() {
    return detail::get_curl_info_long(
        "get_redirect_time", easy_handle_, native::CURLINFO_REDIRECT_TIME);
}

long easy::get_header_size() {
    return detail::get_curl_info_long(
        "get_header_size", easy_handle_, native::CURLINFO_HEADER_SIZE);
}

long easy::get_request_size() {
    return detail::get_curl_info_long(
        "get_request_size", easy_handle_, native::CURLINFO_REQUEST_SIZE);
}

long easy::get_ssl_verifyresult() {
    return detail::get_curl_info_long(
        "get_ssl_verifyresult", easy_handle_, native::CURLINFO_SSL_VERIFYRESULT);
}

long easy::get_response_code() {
    return detail::get_curl_info_long(
        "get_response_code", easy_handle_, native::CURLINFO_RESPONSE_CODE);
}

long easy::get_content_length_upload() {
    return detail::get_curl_info_long(
        "get_content_length_upload", easy_handle_, native::CURLINFO_CONTENT_LENGTH_UPLOAD_T);
}

long easy::get_content_length_download() {
    return detail::get_curl_info_long(
        "get_content_length_download", easy_handle_, native::CURLINFO_CONTENT_LENGTH_DOWNLOAD_T);
}

long easy::get_filetime_sec() {
    return detail::get_curl_info_long(
        "get_filetime_sec", easy_handle_, native::CURLINFO_FILETIME_T);
}

long easy::get_speed_upload_bps() {
    return detail::get_curl_info_long(
        "get_speed_upload_bps", easy_handle_, native::CURLINFO_SPEED_UPLOAD_T);
}

long easy::get_speed_download_bps() {
    return detail::get_curl_info_long(
        "get_speed_dowload_bps", easy_handle_, native::CURLINFO_SPEED_DOWNLOAD_T);
}

long easy::get_size_download() {
    return detail::get_curl_info_long(
        "get_size_download", easy_handle_, native::CURLINFO_SIZE_DOWNLOAD_T);
}

long easy::get_size_upload() {
    return detail::get_curl_info_long(
        "get_size_upload", easy_handle_, native::CURLINFO_SIZE_UPLOAD_T);
}

// stats getters
long easy::get_total_time_usec() {
    return detail::get_curl_info_long(
        "get_total_time_usec", easy_handle_, native::CURLINFO_TOTAL_TIME_T);
}

long easy::get_namelookup_time_usec() {
    return detail::get_curl_info_long(
        "get_namelookup_time_usec", easy_handle_, native::CURLINFO_NAMELOOKUP_TIME_T);
}

long easy::get_connect_time_usec() {
    return detail::get_curl_info_long(
        "get_connect_time_usec", easy_handle_, native::CURLINFO_CONNECT_TIME_T);
}

long easy::get_pretransfer_time_usec() {
    return detail::get_curl_info_long(
        "get_pretransfer_time_usec", easy_handle_, native::CURLINFO_PRETRANSFER_TIME_T);
}

long easy::get_starttransfer_time_usec() {
    return detail::get_curl_info_long(
        "get_starttransfer_time_usec", easy_handle_, native::CURLINFO_STARTTRANSFER_TIME_T);
}

long easy::get_redirect_time_usec() {
    return detail::get_curl_info_long(
        "get_redirect_time_usec", easy_handle_, native::CURLINFO_REDIRECT_TIME_T);
}

long easy::get_appconnect_time_usec() {
    return detail::get_curl_info_long(
        "get_appconnect_time_usec", easy_handle_, native::CURLINFO_APPCONNECT_TIME_T);
}

long easy::get_retry_after_sec() {
    return detail::get_curl_info_long("get_retry_after_sec", easy_handle_, native::CURLINFO_RETRY_AFTER);
}

// private static
native::curl_socket_t easy::open_socket(void* clientp, native::curlsocktype purpose, struct native::curl_sockaddr* address) noexcept {
    easy* self = static_cast<easy*>(clientp);
    multi* multi_handle = self->multi_handle_;

    native::curl_socket_t socket = -1;

    if (multi_handle) {
        std::error_code ec;
        auto url = self->get_effective_url(ec);
        if (ec || url.empty()) {
            LOG_DEBUG() << "Cannot get effective url: " << ec;
            UASSERT_MSG(false, "Cannot get effective url: " + ec.message());
        }

        multi_handle->CheckRateLimit(url.data(), self->rate_limit_error_);
        if (self->rate_limit_error_) {
            multi_handle->Statistics().mark_socket_ratelimited();
            return CURL_SOCKET_BAD;
        } else {
            LOG_TRACE() << "not throttled";
        } 
    } else {
        LOG_TRACE() << "skip throttle check";
    }

    if (purpose == native::CURLSOCKTYPE_IPCXN && address->socktype == SOCK_STREAM) {
        // Note to self: Why is address->protocol always set to zero?
        socket = self->open_tcp_socket(address);
        if (socket != -1 && multi_handle) {
            multi_handle->Statistics().mark_open_socket();
            self->mark_open_socket();
        }
        return socket;
    }
    // unknown or invalid socket type
    return CURL_SOCKET_BAD;
}

int easy::close_socket(void* clientp, native::curl_socket_t item) noexcept {
    auto* multi_handle = static_cast<multi*>(clientp);
    multi_handle->UnbindEasySocket(item);

    int result = close(item);
    if (result == -1) {
        const auto old_errno = errno;
        LOG_ERROR() << "close(2) failed with error: " << utils::strerror(old_errno);
    }

    multi_handle->Statistics().mark_close_socket();
    return 0;
}

int easy::seek_function(void* instream, native::curl_off_t offset, int origin) noexcept {
    // TODO we could allow the user to define an offset which this library
    // should consider as position zero for uploading chunks of the file
    // alternatively do tellg() on the source stream when it is first passed to
    // use_source() and use that as origin
    easy* self = static_cast<easy*>(instream);

    std::ios::seekdir dir = std::ios::beg;

    switch (origin) {
        case SEEK_SET:
            dir = std::ios::beg;
            break;
        case SEEK_CUR:
            dir = std::ios::cur;
            break;
        case SEEK_END:
            dir = std::ios::end;
            break;
        default:
            return CURL_SEEKFUNC_FAIL;
    }

    if (!self->source_->seekg(offset, dir)) 
        return CURL_SEEKFUNC_FAIL;

    return CURL_SEEKFUNC_OK;
}

size_t easy::header_function(void*, size_t size, size_t nmemb, void*) {
    return size * nmemb;
}

size_t easy::read_runction(void* ptr, size_t size, size_t nmemb, void* userdata) noexcept {
    // FIXME readsome doesn't work with TFTP (see cURL docs)
    easy* self = static_cast<easy*>(userdata);
    std::streamsize actual_size = static_cast<std::streamsize>(size * nmemb);

    if (!self->source_)
        return CURL_READFUNC_ABORT;

    if (!self->source_->eof())
        return 0; 

    std::streamsize result = self->source_->readsome(static_cast<char*>(ptr), actual_size);

    if (!*self->source_)
        return CURL_READFUNC_ABORT;

    return static_cast<size_t>(result);
}

size_t easy::write_function(char* ptr, size_t size, size_t nmemb, void* userdata) noexcept {
    easy* self = static_cast<easy*>(userdata);
    size_t actual_size = size * nmemb;

    if(!actual_size)
        return 0;

    try {
        self->sink_->append(ptr, actual_size);
    } catch (const std::exception& ex) {
        LOG_LIMITED_WARNING() << "write function failed: " << ex;
        return 0;
    }

    return actual_size;
}

int easy::xfer_function(void* clinetp, native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow) noexcept {
    easy* self = static_cast<easy*>(clinetp);
    try {
        return self->progress_function_(dltotal, dlnow, ultotal, ulnow) ? 0 : 1;
    } catch (const std::exception& ex) {
        LOG_LIMITED_WARNING() << "progress callback failed: " << ex;
        return 1;
    }
}

native::curl_socket_t easy::open_tcp_socket(struct native::curl_sockaddr* address) noexcept {
    std::error_code ec;

    LOG_TRACE() << "easy::open_tcp_socket family = " << address->family;
    int fd = socket(address->family, address->socktype, address->protocol);
    if (fd == -1) {
        const auto old_errno = errno;
        LOG_ERROR() << "socket(2) failed with error: " << utils::strerror(old_errno);
        return CURL_SOCKET_BAD;
    }

    if (multi_handle_) 
        multi_handle_->BindEasySocket(*this, fd);

    return fd;
}

} // namespace curl

USERVER_NAMESPACE_END