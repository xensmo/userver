/** @file curl-ev/easy_mime.hpp
        curl-ev: upgrade curl-ev/easy.hpp wrapper for integrating libcurl with libev applications
                         Copyright (c) 2013 Oliver Kuckertz <oliver.kuckertz@mologie.de>
        Copyright (c) 2025 Taras Litvinenko <taraslitvinenko@yandex.kz>
        See COPYING for license information.

        C++ wrapper for libcurl's easy interface
*/

#include <sys/socket.h>
#include <unistd.h>

#include <istream>
#include <utility>

#include <curl-ev/easy_mime.hpp>
#include <curl-ev/error_code.hpp>
#include <curl-ev/multi.hpp>
#include <curl-ev/share.hpp>
//#include <curl-ev/string_list.hpp>
//#include <curl-ev/wrappers.hpp>

#include <fmt/compile.h>
#include <fmt/format.h>

#include <engine/ev/thread_control.hpp>
#include <server/net/listener_impl.hpp>
#include <userver/engine/async.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/algo.hpp>
#include <userver/utils/str_icase.hpp>
#include <userver/utils/strerror.hpp>

USERVER_NAMESPACE_BEGIN

namespace curl {
namespace {

} // namespace anon


easy_mime::easy_mime(native::CURL* easy, native::curl_mime* mime, multi* multi) :
        easy_handle_(easy), mime_(mime), multi_handle_(multi), construct_ts_(std::chrono::steady_clock::now()) {
    form_mime_->init_curl_mime(mime);
}

easy_mime::~easy_mime() {
    if (easy_handle_) {
        native::curl_easy_cleanup(easy_handle_);
        easy_handle_ = nullptr;
    }
}

std::shared_ptr<const easy_mime> easy_mime::create_easy_mime_blocking() {
    //impl::CurlGlobal::Init();
    
    auto* handle = native::curl_easy_init();
    if (!handle) { throw std::bad_alloc(); }

    auto* mime = native::curl_mime_init(handle);
    if (!mime) { throw std::bad_alloc(); }
    
    return std::make_shared<const easy_mime>(handle, mime, nullptr);
}

easy_mime* easy_mime::from_native(native::CURL* native_easy) {
    easy_mime* easy_handle = nullptr;
    native::curl_easy_getinfo(native_easy, native::CURLINFO_PRIVATE, easy_handle);
    return easy_handle;
}

engine::ev::ThreadControl& easy_mime::get_thread_control() {
    return multi_handle_->GetThreadControl();
}

void easy_mime::async_perform(handler_type handler) {
    LOG_TRACE() << "easy_mime::async_perform start " << this;
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
    LOG_TRACE() << "easy_mime::async_perform finished " << this;
}

using BusyMarker = utils::statistics::BusyMarker;

void easy_mime::do_ev_async_perform(handler_type handler, size_t request_num) {
    if (request_num <= cancelled_request_max_) {
        LOG_DEBUG() << "async_perform requests allready canceled";
        return;
    }

    LOG_TRACE() << "easy_mime::do_ev_async_perform start " << this;
    mark_start_performing();

    if (!multi_handle_) 
        std::runtime_error("Attempt to perform async. Operation without assigning a multi object.");
    
    BusyMarker busy(multi_handle_->Statistics().get_busy_storage());

    cancel(request_num - 1);

    // open sock func & data
    

    // close sock func & data

    handler_ = std::move(handler);
    multi_registered_ = true;

    LOG_TRACE() << "easy_mime::do_ev_async_perform before multi_->add() " << this;
    multi_handle_->add(this);
}

void easy_mime::do_ev_cancel(size_t request_num) {
    if (cancelled_request_max_ < request_num) 
        cancelled_request_max_ = request_num;
    if (multi_registered_) {
        BusyMarker busy(multi_handle_->Statistics().get_busy_storage());
        handle_completion(std::make_error_code(std::errc::operation_canceled));
        multi_handle_->remove(this);
    }
}

void easy_mime::mark_start_performing() {
    if (start_performing_ts_ == time_point())
        start_performing_ts_ = std::chrono::steady_clock::now();
}

void easy_mime::mark_open_socket() {
    ++sockets_opened_;
}

void easy_mime::handle_completion(const std::error_code& ec) {
    LOG_TRACE() << "easy_mime::handle_completion easy_mime = " << this;
    multi_registered_ = false;
    auto handler = std::function<void(std::error_code)>([](std::error_code) {});
    std::swap(handler, handler_);
    handler(ec);
}

void easy_mime::cancel() {
    cancel(request_counter_);
}

void easy_mime::cancel(size_t request_num) {
    if(multi_handle_) 
        multi_handle_->GetThreadControl().RunInEvLoopSync([this, request_num] { do_ev_cancel(request_num); });
}

std::error_code easy_mime::rate_limit_error() const {
    return rate_limit_error_;
}

easy_mime::time_point::duration easy_mime::time_to_start() const {
    if (start_performing_ts_ != time_point{}) 
        return start_performing_ts_ - construct_ts_;
    return {};
}

void easy_mime::add_header(std::string_view name, std::string_view value, 
    detail_eha eha = detail_eha::kSend, detail_dha dha = detail_dha::kAdd) {

}

void easy_mime::add_header(std::string_view name, std::string_view value, std::error_code& ec, 
    detail_eha eha = detail_eha::kSend, detail_dha dha = detail_dha::kAdd) {

}

void easy_mime::add_header(std::string_view name, std::string_view value, detail_dha dha) {

}

void easy_mime::add_header(std::string_view name, std::string_view value, std::error_code& ec, detail_dha dha) {

}

void easy_mime::add_header(const std::string& header) {
    std::error_code ec;
    add_header(header, ec);
    throw(ec, "add_header");
}

void easy_mime::add_header(const std::string& header, std::error_code& ec) {
    
}

// setters
void easy_mime::set_progress_callback(progress_callback_t progress_callback) {
    progress_callback_ = progress_callback;
    set_no_progress(false);
    set_xferinfo_function(&easy_mime::xfer_function);
    set_xferinfo_data(this);
}

void easy_mime::unset_progress_callback() {
    set_no_progress(true);
    set_xferinfo_function(nullptr);
    set_xferinfo_data(nullptr);
}

void easy_mime::set_no_progress(bool state) {
    std::error_code ec;
    set_no_progress(state, ec);
    throw(ec, "set_no_progress");
}

void easy_mime::set_no_progress(bool state, std::error_code& ec) {
    ec = std::error_code{static_cast<errc::EasyErrorCode>(
        native::curl_easy_setopt(easy_handle_, native::CURLOPT_NOPROGRESS, state)
    )};
}

void easy_mime::set_xferinfo_function(xferfunc_callback_t xferfunc_callback) {

}

void easy_mime::set_xferinfo_data(void* ptr) {

}

void easy_mime::set_opensocket_function(opensocket_callback_t opensocket_callback) {

}

void easy_mime::set_opensocket_data(void* ptr) {

}

void easy_mime::set_url(std::string_view url) {
    std::error_code ec;
    set_url(url, ec);
    throw(ec, "set_url");
}

void easy_mime::set_url(std::string_view url, std::error_code ec) {
    ec = std::error_code{static_cast<errc::EasyErrorCode>(
        native::curl_easy_setopt(easy_handle_, native::CURLOPT_URL, url.data())
    )};
}

void easy_mime::set_http_version(long version) {
    std::error_code ec;
    set_http_version(version, ec);
    throw(ec, "set_http_version");
}

void easy_mime::set_http_version(long version, std::error_code ec) {
    ec = std::error_code{static_cast<errc::EasyErrorCode>(
        native::curl_easy_setopt(easy_handle_, native::CURLOPT_HTTP_VERSION, version)
    )};
}

void easy_mime::set_http_post(std::unique_ptr<form_mime> mime) {
    std::error_code ec;
    set_http_post(std::move(mime), ec);
    throw(ec, "set_http_post");
}

void easy_mime::set_http_post(std::unique_ptr<form_mime> mime, std::error_code& ec) {
    form_mime_ = std::move(mime);
    if (form_mime_) {
        ec = std::error_code{static_cast<errc::EasyErrorCode>(
            native::curl_easy_setopt(easy_handle_, native::CURLOPT_MIMEPOST, form_mime_->native_mime())
        )};
    } else {
        ec = std::error_code{static_cast<errc::EasyErrorCode>(
            native::curl_easy_setopt(easy_handle_, native::CURLOPT_MIMEPOST, NULL)
        )};
    }
}

// protected getters
std::string_view easy_mime::get_effective_url() {
    std::error_code ec;
    auto result = get_effective_url(ec);
    throw(ec, "get_effective_url");
}

std::string_view easy_mime::get_effective_url(std::error_code& ec) {
    char* result = nullptr;
    ec = std::error_code{static_cast<errc::EasyErrorCode>(
        native::curl_easy_getinfo(easy_handle_, native::CURLINFO_EFFECTIVE_URL, &result)
    )};
    return result ? result : std::string_view{};
}

// private static
native::curl_socket_t easy_mime::open_socket(void* clientp, native::curlsocktype purpose, struct native::curl_sockaddr* address) noexcept {
    easy_mime* self = static_cast<easy_mime*>(clientp);
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
        socket = self->open_tcp_socket(address);
        if (socket != -1 && multi_handle) {
            multi_handle->Statistics().mark_open_socket();
            self->mark_open_socket();
        }
        return socket;
    }
    return CURL_SOCKET_BAD;
}

int easy_mime::close_socket(void* clientp, native::curl_socket_t item) noexcept {
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

int easy_mime::seek_function(void* instream, native::curl_off_t offset, int origin) noexcept {
    easy_mime* self = static_cast<easy_mime*>(instream);

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

size_t easy_mime::read_runction(char* ptr, size_t size, size_t nmemb, void* userdata) noexcept {
    easy_mime* self = static_cast<easy_mime*>(userdata);
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

size_t easy_mime::write_function(char* ptr, size_t size, size_t nmemb, void* userdata) noexcept {
    easy_mime* self = static_cast<easy_mime*>(userdata);
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

size_t easy_mime::xfer_function(void* clinetp, native::curl_off_t dltotal, native::curl_off_t dlnow,
        native::curl_off_t ultotal, native::curl_off_t ulnow) noexcept {
    easy_mime* self = static_cast<easy_mime*>(clinetp);
    try {
        return self->progress_callback_(dltotal, dlnow, ultotal, ulnow) ? 0 : 1;
    } catch (const std::exception& ex) {
        LOG_LIMITED_WARNING() << "progress callback failed: " << ex;
        return 1;
    }
}

native::curl_socket_t easy_mime::open_tcp_socket(struct native::curl_sockaddr* address) noexcept {
    std::error_code ec;

    LOG_TRACE() << "easy_mime::open_tcp_socket family = " << address->family;
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