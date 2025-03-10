#pragma once

/** @file curl-ev/easy.hpp
        curl-ev: wrapper for integrating libcurl with libev applications
            upgrade curl-ev/easy.hpp Copyright (c) 2013 Oliver Kuckertz <oliver.kuckertz@mologie.de>
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
#include <utility>

#include <curl-ev/form_mime.hpp>
#include <curl-ev/ratelimit.hpp>
#include <curl-ev/string_list.hpp>
#include <curl-ev/url.hpp>

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
    enum class http_version_t {
        http_version_none               = native::CURL_HTTP_VERSION_NONE,
        http_version_1_0                = native::CURL_HTTP_VERSION_1_0,
        http_version_1_1                = native::CURL_HTTP_VERSION_1_1,
        http_version_2_0                = native::CURL_HTTP_VERSION_2_0,
        http_version_2tls               = native::CURL_HTTP_VERSION_2TLS,
        http_version_2_prior_knowledge  = native::CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE,
        http_version_3_0                = native::CURL_HTTP_VERSION_3,
        http_version_3_0_only           = native::CURL_HTTP_VERSION_3ONLY
    };
    enum class ssl_version_t {
        ssl_version_default = native::CURL_SSLVERSION_DEFAULT,
        ssl_version_tls_v1  = native::CURL_SSLVERSION_TLSv1,
        ssl_version_ssl_v2  = native::CURL_SSLVERSION_SSLv2,
        ssl_version_ssl_v3  = native::CURL_SSLVERSION_SSLv3
    };
    enum class use_ssl_t {
        use_ssl_none        = native::CURLUSESSL_NONE,
        use_ssl_try         = native::CURLUSESSL_TRY,
        use_ssl_control     = native::CURLUSESSL_CONTROL,
        use_ssl_all         = native::CURLUSESSL_ALL
    };
    enum class ip_resolve_t {
        ip_resolve_whatever = CURL_IPRESOLVE_WHATEVER,
        ip_resolve_v4       = CURL_IPRESOLVE_V4,
        ip_resolve_v6       = CURL_IPRESOLVE_V6
    };
    enum class time_condition_t {
        if_modified_since   = native::CURL_TIMECOND_IFMODSINCE,
        if_unmodified_since = native::CURL_TIMECOND_IFUNMODSINCE
    };
} // namespace detail

/*********************************************************************
 * * cURL easy wrapper class
 * * @class easy 
 *   TODO: @brief curl-ev wrapper shared class for libcurl with libev
 *********************************************************************/ 
class easy : public std::enable_shared_from_this<easy> {
public:
    easy(native::CURL* easy, native::curl_mime* mime, multi* multi);
    ~easy();

public: 
    using time_point = std::chrono::steady_clock::time_point;
    using handler_type = std::function<void(std::error_code err)>;
        
    void perform();
    void perform(std::error_code& ec);
    void async_perform(handler_type handler);
    void cancel();
    void reset();
    void mark_retry();

// public sys
    void handle_completion(const std::error_code& ec);

    void set_share(std::shared_ptr<share> share);

// public static     
    static easy* from_native(native::CURL* native_easy);
    static std::shared_ptr<const easy> create_easy_blocking();

// public
    std::error_code rate_limit_error() const;
    time_point::duration time_to_start() const;
    clients::http::LocalStats get_local_stats();
    const std::string& get_post_data() const;

    std::string extract_post_data();

    bool has_post_data() const;

// public inline     
    inline native::CURL* native_handle() { return easy_handle_; }
    inline const multi* get_multi() const { return multi_handle_; }
    inline const native::curl_mime* get_mime() const { return mime_; }

    inline bool operator<(const easy& other) const { return (this < &other); }

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

    void add_resolve(const std::string& host, const std::string& port, const std::string& addr);

public:
/// cURL Opt setters
    [[noreturn]] void set_verbose(bool state);
    [[noreturn]] void set_header(bool state);
    [[noreturn]] void set_no_progress(bool state);
    [[noreturn]] void set_no_signal(bool state);
    [[noreturn]] void set_wildcard_match(bool state);
    [[noreturn]] void set_post(bool state);

    [[noreturn]] void set_source(std::shared_ptr<std::istream> source);
    [[noreturn]] void set_sink(std::string* sink);
    [[noreturn]] void set_private(void* ptr);
    [[noreturn]] void set_post_fields(void* ptr);
    [[noreturn]] void set_custom_request(std::string_view sv);
    [[noreturn]] void set_new_directory_perms(long perms);
    [[noreturn]] void set_new_file_perms(long perms);

    [[noreturn]] void set_url(std::string_view url);
    [[noreturn]] void set_headers(std::shared_ptr<string_list> headers);

// * puplic http setters @fn set_* @return none
    [[noreturn]] void set_http_proxy_tunnel(bool state);
    [[noreturn]] void set_socks5_gsapi_nec(bool state);
    [[noreturn]] void set_tcp_no_delay(bool state);
    [[noreturn]] void set_tcp_keep_alive(bool state);
    [[noreturn]] void set_auto_referrer(bool state);
    [[noreturn]] void set_follow_location(bool state);
    [[noreturn]] void set_unrestricted_auth(bool state);
    [[noreturn]] void set_protocols(long value);

    [[noreturn]] void set_redir_protocols_str(std::string_view sv);

// * puplic protocol setters @fn set_* @return none
    [[noreturn]] void set_transfer_text(bool state);
    [[noreturn]] void set_transfer_mode(bool state);
    [[noreturn]] void set_crlf(bool state);
    [[noreturn]] void set_file_time(bool state);
    [[noreturn]] void set_upload(bool state);
    [[noreturn]] void set_no_body(bool state);
    [[noreturn]] void set_ignore_content_length(bool state);
    [[noreturn]] void set_http_content_decoding(bool state);
    [[noreturn]] void set_http_transfer_decoding(bool state);
    [[noreturn]] void set_http_get(bool state);
    [[noreturn]] void set_cookie_session(bool state);
    [[noreturn]] void set_resume_from(long value);
    [[noreturn]] void set_in_file_size(long value);
    [[noreturn]] void set_max_file_size(long value);
    [[noreturn]] void set_time_value(long value);
    [[noreturn]] void set_resume_from_large(native::curl_off_t value);
    [[noreturn]] void set_in_file_size_large(native::curl_off_t value);
    [[noreturn]] void set_max_file_size_large(native::curl_off_t value);
    [[noreturn]] void set_time_value(native::curl_off_t value);
    [[noreturn]] void set_range(std::string_view sv);
    [[noreturn]] void set_cookie_list(std::string_view sv);
    [[noreturn]] void set_cookie(std::string_view sv);
    [[noreturn]] void set_cookie_file(std::string_view sv);
    [[noreturn]] void set_cookie_jar(std::string_view sv);
    [[noreturn]] void set_http_version(detail::http_version_t version);
    [[noreturn]] void set_time_condition(detail::time_condition_t condition);

// * puplic connection setters @fn set_* @return none
    [[noreturn]] void set_fresh_connect(bool state);
    [[noreturn]] void set_forbit_reuse(bool state);
    [[noreturn]] void set_connect_only(bool state);
    [[noreturn]] void set_timeout(long timeout);
    [[noreturn]] void set_timeout_ms(long timeout);
    [[noreturn]] void set_low_speed_limit(long limit);
    [[noreturn]] void set_low_speed_time(long time);
    [[noreturn]] void set_max_connects(long connects);
    [[noreturn]] void set_connect_timeout(long timeout);
    [[noreturn]] void set_connect_timeout_ms(long timeout);
    [[noreturn]] void set_accept_timeout_ms(long timeout);
    [[noreturn]] void set_max_send_speed_large(native::curl_off_t large);
    [[noreturn]] void set_max_recv_speed_large(native::curl_off_t large);
    [[noreturn]] void set_dns_servers(std::string_view sv);
    [[noreturn]] void set_ip_resolve(detail::ip_resolve_t resolve);
    [[noreturn]] void set_resolves(std::shared_ptr<string_list> resolved_hosts);

// * puplic ssh setters @fn set_* @return none
    [[noreturn]] void set_ssh_auth_types(long types);
    [[noreturn]] void set_ssh_host_public_key_md5(std::string_view sv);
    [[noreturn]] void set_ssh_public_key_file(std::string_view sv);
    [[noreturn]] void set_ssh_private_key_file(std::string_view sv);
    [[noreturn]] void set_ssh_known_hosts(std::string_view sv);
    [[noreturn]] void set_ssh_key_function(void* ptr); // !TODO ssh key callback ?
    [[noreturn]] void set_ssh_key_data(void* ptr);

// * ssl and security setters @fn set_* @return none
    [[noreturn]] void set_ssl_cert(std::string_view sv);
    [[noreturn]] void set_ssl_cert_type(std::string_view sv);
    [[noreturn]] void set_ssl_key(std::string_view sv);
    [[noreturn]] void set_ssl_key_type(std::string_view sv);
    [[noreturn]] void set_ssl_key_passwd(std::string_view sv);
    [[noreturn]] void set_ssl_engine(std::string_view sv);
    [[noreturn]] void set_ssl_engine_default(std::string_view sv);
    [[noreturn]] void set_ca_info(std::string_view sv);
    [[noreturn]] void set_ca_file(std::string_view sv);
    [[noreturn]] void set_crl_file(std::string_view sv);
    [[noreturn]] void set_issuer_cert(std::string_view sv);
    [[noreturn]] void set_ssl_cipher_list(std::string_view sv);
    [[noreturn]] void set_krb_level(std::string_view sv);
    [[noreturn]] void set_ssl_options(long options);
    [[noreturn]] void set_gssapi_delegation(long arg);
    [[noreturn]] void set_cert_info(bool state);
    [[noreturn]] void set_ssl_session_id_cache(bool state);
    [[noreturn]] void set_ssl_verify_peer(bool state);
    [[noreturn]] void set_ssl_verify_host(bool state);
    [[noreturn]] void set_ssl_version(detail::ssl_version_t version);
    [[noreturn]] void set_use_ssl(detail::use_ssl_t ssl);
    
#if LIBCURL_VERSION_NUM >= 0x074700    
    [[noreturn]] void set_ssl_cert_blob_copy(std::string_view sv);
    [[noreturn]] void set_ssl_cert_blob_no_copy(std::string_view sv);
    [[noreturn]] void set_ssl_key_blob_copy(std::string_view sv);
    [[noreturn]] void set_ssl_key_blob_no_copy(std::string_view sv);
#endif

#if LIBCURL_VERSION_NUM >= 0x074D00
    [[noreturn]] void set_ca_info_blob_copy(std::string_view sv);
    [[noreturn]] void set_ca_info_blob_no_copy(std::string_view sv);
#endif

// deprecated setters    
    [[deprecated("Use set_mimepost())")]] [[noreturn]] void set_http_post(void*) { }
    [[deprecated("Serves no purpose anymore")]] [[noreturn]] 
    void set_random_file(std::string_view) { } // CURLOPT_RANDOM_FILE
    [[deprecated("Serves no purpose anymore")]] [[noreturn]] 
    void set_egd_socket(std::string_view) { }  // CURLOPT_EGDSOCKET
    [[deprecated("Use set_redir_protocols_str()")]] [[noreturn]]
    void set_redir_protocols(long) { } // CURLOPT_REDIR_PROTOCOLS

// * puplic callback function @fn set_* @return none  
    using progress_function_t = std::function<bool( native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow)>;
    void set_progress_callback(progress_function_t func);
    void unset_progress_callback();    
      
    using write_function_t = size_t (*)(char* ptr, size_t size, size_t nmemb, void* userdata);
    [[noreturn]] void set_write_function(write_function_t func, std::error_code& ec);
    [[noreturn]] void set_write_data(void* ptr, std::error_code& ec);

    using read_function_t = std::size_t (*)(void* ptr, size_t size, size_t nmemb, void* userdata);
    [[noreturn]] void set_read_function(read_function_t func, std::error_code& ec);
    [[noreturn]] void set_read_data(void* ptr, std::error_code& ec);

    using seek_function_t = int (*)(void* instream, native::curl_off_t offset, int origin);
    [[noreturn]] void set_seek_function(seek_function_t func, std::error_code& ec);
    [[noreturn]] void set_seek_data(void* ptr, std::error_code& ec);

    using sockopt_function_t = int (*)(void* clientp, native::curl_socket_t curlfd, native::curlsocktype purpose);
    [[noreturn]] void set_sockopt_function(sockopt_function_t func);
    [[noreturn]] void set_sockopt_data(void* ptr);

    using xfer_function_t = int (*)(void* clinetp, native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow);
    [[noreturn]] void set_xferinfo_function(xfer_function_t func);
    [[noreturn]] void set_xferinfo_data(void* ptr);

    using opensocket_function_t = native::curl_socket_t (*)(void* clientp, 
        native::curlsocktype purpose, struct native::curl_sockaddr* address);
    [[noreturn]] void set_opensocket_function(opensocket_function_t func);
    [[noreturn]] void set_opensocket_data(void* ptr);

    using closesocket_function_t = int (*)(void* clientp, native::curl_socket_t item);
    [[noreturn]] void set_closesocket_function(closesocket_function_t func);
    [[noreturn]] void set_closesocket_data(void* ptr);
    
    using ssl_ctx_function_t = native::CURLcode (*)(native::CURL* curl, void* sslctx, void* parm);
    [[noreturn]] void set_ssl_ctx_function(ssl_ctx_function_t func);
    [[noreturn]] void set_ssl_ctx_data(void* ptr);

// * getters @fn get_* @return std::shared_ptr<easy>
    [[nodiscard]] std::shared_ptr<easy> get_bound_blocking(multi& multi_handle) const;

// * getters @fn get_* @return std::vector<std::string>
    [[nodiscard]] std::vector<std::string> get_ssl_engines();
    [[nodiscard]] std::vector<std::string> get_cookielist();

// * getters @fn get_* @return std::string_view
    [[nodiscard]] std::string_view get_effective_url(std::error_code& ec);
    [[nodiscard]] std::string_view get_scheme();
    [[nodiscard]] std::string_view get_local_ip();
    [[nodiscard]] std::string_view get_rtsp_session_id();
    [[nodiscard]] std::string_view get_primary_ip();
    [[nodiscard]] std::string_view get_redirect_url();
    [[nodiscard]] std::string_view get_ftp_entry_path();
    [[nodiscard]] std::string_view get_content_type();

// * getters @fn get_* @return long
    [[nodiscard]] long get_http_version();
    [[nodiscard]] long get_proxy_ssl_verifyresult();
    [[nodiscard]] long get_local_port();
    [[nodiscard]] long get_primary_port();
    [[nodiscard]] long get_rtsp_cseq_recv();
    [[nodiscard]] long get_rtsp_server_cseq();
    [[nodiscard]] long get_rtsp_client_cseq();
    [[nodiscard]] long get_condition_unmet();
    [[nodiscard]] long get_num_connects();
    [[nodiscard]] long get_os_errno();
    [[nodiscard]] long get_proxyauth_avail();
    [[nodiscard]] long get_httpauth_avail();
    [[nodiscard]] long get_http_connectcode();
    [[nodiscard]] long get_redirect_count();
    [[nodiscard]] long get_redirect_time();
    [[nodiscard]] long get_header_size();
    [[nodiscard]] long get_request_size();
    [[nodiscard]] long get_ssl_verifyresult();
    [[nodiscard]] long get_response_code();

// * getters stats @fn get_* @return long   
    [[nodiscard]] long get_content_length_upload();     // return native::curl_off_t ?
    [[nodiscard]] long get_content_length_download();   // return native::curl_off_t ?
    [[nodiscard]] long get_filetime_sec();              // return native::curl_off_t ?
    [[nodiscard]] long get_speed_upload_bps();          // return native::curl_off_t ?
    [[nodiscard]] long get_speed_download_bps();        // return native::curl_off_t ?
    [[nodiscard]] long get_size_download();             // return native::curl_off_t ?
    [[nodiscard]] long get_size_upload();               // return native::curl_off_t ?

// * getters stats @fn get_* @return long
    [[nodiscard]] long get_total_time_usec();           // return native::curl_off_t ?
    [[nodiscard]] long get_namelookup_time_usec();      // return native::curl_off_t ?
    [[nodiscard]] long get_connect_time_usec();         // return native::curl_off_t ?
    [[nodiscard]] long get_pretransfer_time_usec();     // return native::curl_off_t ?
    [[nodiscard]] long get_starttransfer_time_usec();   // return native::curl_off_t ?
    [[nodiscard]] long get_redirect_time_usec();        // return native::curl_off_t ?
    [[nodiscard]] long get_appconnect_time_usec();      // return native::curl_off_t ?
    [[nodiscard]] long get_retry_after_sec();           // return native::curl_off_t ?

// public deprecated getters
    [[deprecated("Use get_scheme()")]] [[nodiscard]] long get_protocol() {} // CURLINFO_PROTOCOL

    
private:
    easy(const easy& rhs) = delete;
    easy(easy&& rhs) = delete;

    easy& operator= (const easy& rhs) = delete;
    easy& operator= (easy&& rhs) = delete;

private:
// private static
    static native::curl_socket_t open_socket(void* clientp, native::curlsocktype purpose, struct native::curl_sockaddr* address) noexcept;
    static int close_socket(void* clientp, native::curl_socket_t item) noexcept;
    static int seek_function(void* instream, native::curl_off_t offset, int origin) noexcept;
    static size_t header_function(void* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t read_runction(void* ptr, size_t size, size_t nmemb, void* userdata) noexcept;
    static size_t write_function(char* ptr, size_t size, size_t nmemb, void* userdata) noexcept;
    static int xfer_function(void* clinetp, native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow) noexcept;

    native::curl_socket_t open_tcp_socket(struct native::curl_sockaddr* address) noexcept;

    // do_ev_* methods run in libev thread
    void do_ev_async_perform(handler_type handler, size_t request_num);
    void do_ev_cancel(size_t request_num);

    void mark_start_performing();
    void mark_open_socket();

    void cancel(size_t request_num);    

protected:
    engine::ev::ThreadControl& get_thread_control();

// cURL protected
    void set_source(std::shared_ptr<std::istream> source, std::error_code& ec);
    void set_sink(std::string* sink, std::error_code& ec);
    
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