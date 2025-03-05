/** @file curl-ev/easy_mime.hpp
        curl-ev: upgrade wrapper for integrating libcurl with libev applications
        Copyright (c) 2025 Taras Litvinenko <taraslitvinenko@yandex.kz>
        See COPYING for license information.

        C++ wrapper for libcurl's easy interface
*/

#include <sys/socket.h>
#include <unistd.h>

#include <istream>
#include <utility>

#include <curl-ev/easy_mime.hpp>
//#include <curl-ev/error_code.hpp>
//#include <curl-ev/form.hpp>
//#include <curl-ev/multi.hpp>
//#include <curl-ev/share.hpp>
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


easy_mime::easy_mime() {

}

easy_mime::~easy_mime() {

}

std::shared_ptr<const easy_mime> easy_mime::CreateEasyBlocking() {
    //impl::CurlGlobal::Init();
    
    //auto* handle = native::curl_easy_init();
    //if(!handle) { throw std::bad_alloc(); }

    return std::make_shared<const easy_mime>();
}

//easy_mime* easy_mime::from_native(native::CURL* native_easy) {
//    easy_mime* scheme = nullptr;
    //native::curl_easy_getinfo(native_easy, native::CURLINFO_SCHEME, scheme);
//    return scheme;
//}

void easy_mime::add_header(std::string_view name, std::string_view value, 
    empty_header_action eha = empty_header_action::kSend, duplicate_header_action dha = duplicate_header_action::kAdd) {

}

void easy_mime::add_header(std::string_view name, std::string_view value, std::error_code& ec, 
    empty_header_action eha = empty_header_action::kSend, duplicate_header_action dha = duplicate_header_action::kAdd) {

}

void easy_mime::add_header(std::string_view name, std::string_view value,
    duplicate_header_action dha) {

}

void easy_mime::add_header(std::string_view name, std::string_view value, std::error_code& ec,
    duplicate_header_action dha) {

}

void easy_mime::add_header(const std::string& header) {
    std::error_code ec;
    add_header(header, ec);
    throw(ec, "add_header");
}

void easy_mime::add_header(const std::string& header, std::error_code& ec) {
    
}


} // namespace curl

USERVER_NAMESPACE_END