#pragma once

/** @file curl-ev/easy.hpp
        curl-ev: wrapper for integrating libcurl with libev applications
        Copyright (c) 2013 Oliver Kuckertz <oliver.kuckertz@mologie.de>
        See COPYING for license information.

        C++ wrapper for libcurl's easy interface
*/

// !TODO this @file curl-ev/easy.hpp upgrade in 2025 year                   //
// !@author <<< xensmo >>> Taras Litvinenko <taraslitvinenko@yandex.kz>     //

#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include <curl-ev/mime.hpp>
#include <curl-ev/ratelimit.hpp>
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
class string_list;

namespace detail {
    enum class empty_header_action      { kSend, kDoNotSend };
    enum class duplicate_header_action  { kAdd, kSkip, kReplace };
    enum class http_version_t : unsigned long {
        http_version_none               = native::CURL_HTTP_VERSION_NONE,
        http_version_1_0                = native::CURL_HTTP_VERSION_1_0,
        http_version_1_1                = native::CURL_HTTP_VERSION_1_1,
        http_version_2_0                = native::CURL_HTTP_VERSION_2_0,
        http_version_2tls               = native::CURL_HTTP_VERSION_2TLS,
        http_version_2_prior_knowledge  = native::CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE,
        http_version_3_0                = native::CURL_HTTP_VERSION_3,
        http_version_3_0_only           = native::CURL_HTTP_VERSION_3ONLY
    };
    enum class ssl_version_t : unsigned long {
        ssl_version_default     = native::CURL_SSLVERSION_DEFAULT,
        ssl_version_tls_v1      = native::CURL_SSLVERSION_TLSv1,
        ssl_version_ssl_v2      = native::CURL_SSLVERSION_SSLv2,
        ssl_version_ssl_v3      = native::CURL_SSLVERSION_SSLv3
    };
    enum class use_ssl_t : unsigned long {
        use_ssl_none            = native::CURLUSESSL_NONE,
        use_ssl_try             = native::CURLUSESSL_TRY,
        use_ssl_control         = native::CURLUSESSL_CONTROL,
        use_ssl_all             = native::CURLUSESSL_ALL
    };
    enum class ip_resolve_t : unsigned long {
        ip_resolve_whatever     = CURL_IPRESOLVE_WHATEVER,
        ip_resolve_v4           = CURL_IPRESOLVE_V4,
        ip_resolve_v6           = CURL_IPRESOLVE_V6
    };
    enum class time_condition_t : unsigned long {
        if_modified_since       = native::CURL_TIMECOND_IFMODSINCE,
        if_unmodified_since     = native::CURL_TIMECOND_IFUNMODSINCE
    };
    enum class netrc_t : unsigned long {
        netrc_optional          = native::CURL_NETRC_OPTIONAL,
        netrc_ignored           = native::CURL_NETRC_IGNORED,
        netrc_required          = native::CURL_NETRC_REQUIRED
    };
    enum class httpauth_t : unsigned long {
        auth_basic              = CURLAUTH_BASIC,
        auth_digest             = CURLAUTH_DIGEST,
        auth_digest_ie          = CURLAUTH_DIGEST_IE,
        auth_negotiate          = CURLAUTH_NEGOTIATE,
        auth_ntlm               = CURLAUTH_NTLM,
        auth_ntlm_wb            = CURLAUTH_NTLM_WB,
        auth_any                = CURLAUTH_ANY,
        auth_any_safe           = CURLAUTH_ANYSAFE
    };
    enum class proxyauth_t : unsigned long {
        proxy_auth_basic        = CURLAUTH_BASIC,
        proxy_auth_digest       = CURLAUTH_DIGEST,
        proxy_auth_digest_ie    = CURLAUTH_DIGEST_IE,
        proxy_auth_bearer       = CURLAUTH_BEARER,
        proxy_auth_negotiate    = CURLAUTH_NEGOTIATE,
        proxy_auth_ntlm         = CURLAUTH_NTLM,
        proxy_auth_ntlm_wb      = CURLAUTH_NTLM_WB,
        proxy_auth_any          = CURLAUTH_ANY,
        proxy_auth_anysafe      = CURLAUTH_ANYSAFE
    };
} // namespace detail

/*********************************************************************
 * * cURL easy wrapper class
 * * @class easy 
 *   TODO: @brief curl-ev wrapper shared class for libcurl with libev
 *********************************************************************/ 
class easy final : public std::enable_shared_from_this<easy> {
public:
    explicit easy(native::CURL* easy_handle, native::curl_mime* mime_ptr, multi* multi_ptr);

    easy(const easy& rhs) = delete;
    easy(easy&& rhs) noexcept = default;

    easy& operator= (const easy& rhs) noexcept = default;
    easy& operator= (easy&& rhs) noexcept = default;

    ~easy();

public: 
    using time_point = std::chrono::steady_clock::time_point;
    using handler_type = std::function<void(std::error_code err)>;
    
    // * this function unused, use async_perform() 
    [[maybe_unused]] void perform(){}

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

    inline bool operator<(const easy& other) const { return (this < &other); }

public:
    using detail_eha = detail::empty_header_action;
    using detail_dha = detail::duplicate_header_action;
    
    void add_header(std::string_view name, std::string_view value, 
            detail_eha eha = detail_eha::kSend, detail_dha dha = detail_dha::kAdd);
    void add_header(std::string_view name, std::string_view value, detail_dha dha);
    void add_header(const char* header);
    void add_header(const std::string& header);
    void add_proxy_header(std::string_view name, std::string_view value, 
        detail_eha = detail_eha::kSend, detail_dha = detail_dha::kAdd);
    void add_proxy_header(const char* header);
    void add_proxy_header(const std::string& header);
    void add_resolve(const std::string& host, const std::string& port, const std::string& addr);
    void add_http200_alias(const std::string& http200_alias);

    std::optional<std::string_view> find_header_by_name(std::string_view name) const;

public:
/// cURL Opt setters
    void set_verbose(bool state);
    void set_header(bool state);
    void set_no_progress(bool state);
    void set_no_signal(bool state);
    void set_wildcard_match(bool state);
    void set_post(bool state);

    void set_source(std::shared_ptr<std::istream> source);
    void set_sink(std::string* sink);
    void set_private(void* ptr);
    void set_custom_request(const std::string& str);
    void set_new_directory_perms(long perms);
    void set_new_file_perms(long perms);

    void set_url(std::string&& url_str, std::error_code& ec);
    void set_progress_data(void* ptr);

    void set_error_buffer(char* buffer);
    void set_stderr(FILE* file);
    void set_fail_on_error(bool state);

    void set_mimepost(std::unique_ptr<mime> mime_ptr);
    
// * puplic http setters @fn set_* @return none
    void set_http_proxy_tunnel(bool state);
    void set_socks5_gsapi_nec(bool state);
    void set_tcp_no_delay(bool state);
    void set_tcp_keep_alive(bool state);
    void set_auto_referrer(bool state);
    void set_follow_location(bool state);
    void set_unrestricted_auth(bool state);
    void set_max_redirs(long value);
    void set_post_redir(long value);
    void set_post_field_size(long value);
    void set_proxy_port(long value);
    void set_proxy_type(long value);
    void set_local_port(long value);
    void set_local_port_range(long value);
    void set_dns_cache_timeout(long value);
    void set_buffer_size(long value);
    void set_port(long value);
    void set_address_scope(long value);
    void set_tcp_keep_idle(long value);
    void set_tcp_keep_intvl(long value);
    void set_connect_to(native::curl_slist* slist);
    void set_post_field_size_large(native::curl_off_t value);
    void set_post_fields(void* ptr);
    void set_post_fields(std::string&& post_fields);
    void set_proxy(std::string_view sv);
    void set_interface(std::string_view sv);
    void set_unix_socket_path(std::string_view sv);
    void set_no_proxy(std::string_view sv);
    void set_accept_encoding(const std::string& str);
    void set_transfer_encoding(std::string_view sv);
    void set_referer(std::string_view sv);
    void set_user_agent(std::string_view sv);
    void set_protocols_str(const std::string& str);
    void set_redir_protocols_str(const std::string& str);
    void set_headers(std::shared_ptr<string_list> headers);
    void set_http200_aliases(std::shared_ptr<string_list> http200_aliases);

// * puplic auth setters @fn set_* @return none
    void set_tls_auth_type(long value);
    void set_tls_auth_user(std::string_view sv);
    void set_tls_auth_password(std::string_view sv);
    void set_netrc_file(std::string_view sv);
    void set_user(std::string_view sv);
    void set_password(std::string_view sv);
    void set_proxy_user(std::string_view sv);
    void set_proxy_password(std::string_view sv);
    void set_netrc(detail::netrc_t netrc);
    void set_http_auth(detail::httpauth_t httpauth, bool auth_only);
    void set_proxy_auth(detail::proxyauth_t proxyauth);

// * puplic protocol setters @fn set_* @return none
    void set_transfer_text(bool state);
    void set_transfer_mode(bool state);
    void set_crlf(bool state);
    void set_file_time(bool state);
    void set_upload(bool state);
    void set_no_body(bool state);
    void set_ignore_content_length(bool state);
    void set_http_content_decoding(bool state);
    void set_http_transfer_decoding(bool state);
    void set_http_get(bool state);
    void set_cookie_session(bool state);
    void set_resume_from(long value);
    void set_in_file_size(long value);
    void set_max_file_size(long value);
    void set_time_value(long value);
    void set_resume_from_large(native::curl_off_t value);
    void set_in_file_size_large(native::curl_off_t value);
    void set_max_file_size_large(native::curl_off_t value);
    void set_range(std::string_view sv);
    void set_cookie_list(std::string_view sv);
    void set_cookie(std::string_view sv);
    void set_cookie_file(std::string_view sv);
    void set_cookie_jar(std::string_view sv);
    void set_http_version(detail::http_version_t version);
    void set_time_condition(detail::time_condition_t condition);

// * puplic connection setters @fn set_* @return none
    void set_fresh_connect(bool state);
    void set_forbit_reuse(bool state);
    void set_connect_only(bool state);
    void set_timeout(long timeout);
    void set_timeout_ms(long timeout);
    void set_low_speed_limit(long limit);
    void set_low_speed_time(long time);
    void set_max_connects(long connects);
    void set_connect_timeout(long timeout);
    void set_connect_timeout_ms(long timeout);
    void set_accept_timeout_ms(long timeout);
    void set_max_send_speed_large(native::curl_off_t large);
    void set_max_recv_speed_large(native::curl_off_t large);
    void set_dns_servers(std::string_view sv);
    void set_ip_resolve(detail::ip_resolve_t resolve);
    void set_resolves(std::shared_ptr<string_list> resolved_hosts);

// * puplic ssh setters @fn set_* @return none
    void set_ssh_auth_types(long types);
    void set_ssh_host_public_key_md5(std::string_view sv);
    void set_ssh_public_key_file(std::string_view sv);
    void set_ssh_private_key_file(std::string_view sv);
    void set_ssh_known_hosts(std::string_view sv);
    void set_ssh_key_function(void* ptr); // !TODO ssh key callback ?
    void set_ssh_key_data(void* ptr);

// * ssl and security setters @fn set_* @return none
    void set_ssl_cert(std::string_view sv);
    void set_ssl_cert_type(std::string_view sv);
    void set_ssl_key(std::string_view sv);
    void set_ssl_key_type(std::string_view sv);
    void set_ssl_key_passwd(std::string_view sv);
    void set_ssl_engine(std::string_view sv);
    void set_ssl_engine_default(std::string_view sv);
    void set_ca_info(std::string_view sv);
    void set_ca_file(std::string_view sv);
    void set_crl_file(std::string_view sv);
    void set_issuer_cert(std::string_view sv);
    void set_ssl_cipher_list(std::string_view sv);
    void set_krb_level(std::string_view sv);
    void set_ssl_options(long options);
    void set_gssapi_delegation(long arg);
    void set_cert_info(bool state);
    void set_ssl_session_id_cache(bool state);
    void set_ssl_verify_peer(bool state);
    void set_ssl_verify_host(bool state);
    void set_ssl_version(detail::ssl_version_t version);
    void set_use_ssl(detail::use_ssl_t ssl);
    
#if LIBCURL_VERSION_NUM >= 0x074700    
    void set_ssl_cert_blob_copy(std::string_view sv);
    void set_ssl_cert_blob_no_copy(std::string_view sv);
    void set_ssl_key_blob_copy(std::string_view sv);
    void set_ssl_key_blob_no_copy(std::string_view sv);

    static constexpr bool is_set_ssl_cert_blob_available = true;
    static constexpr bool is_set_ssl_key_blob_available = true;
#endif

#if LIBCURL_VERSION_NUM >= 0x074D00
    void set_ca_info_blob_copy(std::string_view sv);
    void set_ca_info_blob_no_copy(std::string_view sv);
    
    static constexpr bool is_set_ca_info_blob_available = true;
#endif

    // this deprecated function use set_mimepost()    
    [[deprecated("Use set_mimepost())")]] [[noreturn]]
    void set_http_post(void*);
    // this deprecated function
    [[deprecated("Serves no purpose anymore")]] [[noreturn]]
    void set_random_file(std::string_view); // CURLOPT_RANDOM_FILE
    // this deprecated function
    [[deprecated("Serves no purpose anymore")]] [[noreturn]]
    void set_egd_socket(std::string_view);  // CURLOPT_EGDSOCKET
    // this deprecated function
    [[deprecated("Use set_redir_protocols_str()")]] [[noreturn]]
    void set_redir_protocols(long); // CURLOPT_REDIR_PROTOCOLS
    [[deprecated("Use set_protocols_str()")]] [[noreturn]]
    void set_protocols(long); // CURLOPT_PROTOCOLS

// * puplic callback function @fn set_* @return none  
    using progress_function_t = std::function<bool( native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow)>;
    void set_progress_callback(progress_function_t func);
    void unset_progress_callback();    

    using header_function_t = size_t (*)(void* ptr, size_t size, size_t nmemb, void* userdata);
    void set_header_function(header_function_t func);
    void set_header_data(void* ptr);

    using debug_function_t = int (*)(native::CURL*, native::curl_infotype, char*, size_t, void*);
    void set_debug_function(debug_function_t func);
    void set_debug_data(void* ptr);

    using interleave_function_t = size_t (*)(void* ptr, size_t size, size_t nmemb, void* userdata);
    void set_interleave_function(interleave_function_t func);
    void set_interleave_data(void* ptr);
      
    using write_function_t = size_t (*)(char* ptr, size_t size, size_t nmemb, void* userdata);
    void set_write_function(write_function_t func, std::error_code& ec);
    void set_write_data(void* ptr, std::error_code& ec);

    using read_function_t = std::size_t (*)(void* ptr, size_t size, size_t nmemb, void* userdata);
    void set_read_function(read_function_t func, std::error_code& ec);
    void set_read_data(void* ptr, std::error_code& ec);

    using seek_function_t = int (*)(void* instream, native::curl_off_t offset, int origin);
    void set_seek_function(seek_function_t func, std::error_code& ec);
    void set_seek_data(void* ptr, std::error_code& ec);

    using sockopt_function_t = int (*)(void* clientp, native::curl_socket_t curlfd, native::curlsocktype purpose);
    void set_sockopt_function(sockopt_function_t func);
    void set_sockopt_data(void* ptr);

    using xfer_function_t = int (*)(void* clinetp, native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow);
    void set_xferinfo_function(xfer_function_t func);
    void set_xferinfo_data(void* ptr);

    using opensocket_function_t = native::curl_socket_t (*)(void* clientp, 
        native::curlsocktype purpose, struct native::curl_sockaddr* address);
    void set_opensocket_function(opensocket_function_t func);
    void set_opensocket_data(void* ptr);

    using closesocket_function_t = int (*)(void* clientp, native::curl_socket_t item);
    void set_closesocket_function(closesocket_function_t func);
    void set_closesocket_data(void* ptr);
    
    using ssl_ctx_function_t = native::CURLcode (*)(native::CURL* curl, void* sslctx, void* parm);
    void set_ssl_ctx_function(ssl_ctx_function_t func, std::error_code& ec);
    void set_ssl_ctx_data(void* ptr, std::error_code& ec);

// * getters @fn get_* @return std::shared_ptr<easy>
    [[nodiscard]] std::shared_ptr<easy> get_bound_blocking(multi& multi_handle) const;
    [[nodiscard]] inline const std::string& get_original_url() const { return orig_url_str_; }
    [[nodiscard]] inline const url& get_easy_url() const { return url_; }

// * getters @fn get_* @return std::vector<std::string>
    [[nodiscard]] std::vector<std::string> get_ssl_engines();
    [[nodiscard]] std::vector<std::string> get_cookielist();

// * getters @fn get_* @return std::string_view
    [[nodiscard]] std::string_view get_effective_url();
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
    [[deprecated("Use get_scheme()")]] [[noreturn]]
    void get_protocol(); // CURLINFO_PROTOCOL

    engine::ev::ThreadControl& get_thread_control();
    
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
    native::CURL*                   easy_handle_;
    std::shared_ptr<mime>           mime_;
    multi*                          multi_handle_;
    url                             url_;                            
    
    size_t                          request_counter_        { 0 };
    size_t                          retries_counter_        { 0 };
    size_t                          cancelled_request_max_  { 0 };
    size_t                          sockets_opened_         { 0 };

    bool                            multi_registered_       { false };

    std::string*                    sink_                   { nullptr };
    std::string                     orig_url_str_           {};
    std::string                     post_fields_            {};                    

    std::shared_ptr<share>          share_                  { nullptr };
    std::shared_ptr<std::istream>   source_                 { nullptr };
    std::shared_ptr<string_list>    headers_                { nullptr };
    std::shared_ptr<string_list>    proxy_headers_          { nullptr };
    std::shared_ptr<string_list>    http200_aliases_        { nullptr };
    std::shared_ptr<string_list>    resolved_hosts_         { nullptr };

    handler_type                    handler_                {};
    progress_function_t             progress_function_      {};

    std::error_code                 rate_limit_error_       {};

    time_point start_performing_ts_{};
    const time_point construct_ts_;
};
} // namespace curl

USERVER_NAMESPACE_END