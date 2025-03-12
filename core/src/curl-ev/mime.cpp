// !TODO this @file curl-ev/mime.cpp create in 2025 year                    //
// !@author <<< xensmo >>> Taras Litvinenko <taraslitvinenko@yandex.kz>     //

#include <curl-ev/mime.hpp>
#include <curl-ev/error_code.hpp>
#include <curl-ev/wrappers.hpp>

USERVER_NAMESPACE_BEGIN
namespace curl {

mime::mime(native::curl_mime* mime_ptr) :
    mime_(mime_ptr) {
    
}

mime::~mime() {
    if (mime_) {
        native::curl_mime_free(mime_);
        mime_ = nullptr;
    }
}

//void mime::add_content(std::string_view key, std::string_view content) {
//    std::error_code ec;
//    add_content(key, content, ec);
//    throw(ec, "add_content");
//}

//void mime::add_content(std::string_view key, std::string_view content, std::error_code& ec) {
//    part_ = native::curl_mime_addpart(mime_);
//    
//    ec = std::error_code { static_cast<errc::EasyErrorCode>(native::curl_mime_data(part_, key.data(), key.size())) };
//    native::curl_mime_name(part_, content.data());
//}

//void mime::add_content(std::string_view key, std::string_view content, const std::string& content_type) {
//    std::error_code ec;
//    add_content(key, content, content_type, ec);
//    throw(ec, "add_content");
//}

//void mime::add_content(std::string_view key, std::string_view content, const std::string& content_type, std::error_code& ec) {
//    part_ = native::curl_mime_addpart(mime_);
//    ec = std::error_code { static_cast<errc::EasyErrorCode>(native::curl_mime_data(part_, key.data(), key.size())) };

//    part_ = native::curl_mime_addpart(mime_);
//    ec = std::error_code { static_cast<errc::EasyErrorCode>(native::curl_mime_data(part_, content.data(), content.size())) };

//    part_ = native::curl_mime_addpart(mime_);
//    native::curl_mime_type(part_, content_type.c_str());
//}

/* void form_mime::add_file_content(std::string_view key, std::string_view file_path) {

} */

/* void form_mime::add_file_content(std::string_view key, std::string_view file_path, std::error_code& ec) {

} */

/* void form_mime::add_file_content(std::string_view key, std::string_view file_path, const std::string& content_type) {

} */

/* void form_mime::add_file_content(std::string_view key, std::string_view file_path, const std::string& content_type, std::error_code& ec) {

} */

/* void form_mime::add_file_using_name(std::string_view key, std::string_view file_path, std::string_view file_name) {

} */

/* void form_mime::add_file_using_name(std::string_view key, std::string_view file_path, std::string_view file_name, std::error_code ec) {

} */

/* void form_mime::add_file_using_name(std::string_view key, std::string_view file_path, std::string_view file_name, const std::string& content_type) {

} */

/* void form_mime::add_file_using_name(std::string_view key, std::string_view file_path, std::string_view file_name, const std::string& content_type, std::error_code& ec) {
    
} */

//void mime::add_buffer(std::string_view key, std::string_view file_name, 
//        const std::shared_ptr<std::string>& buffer) {
//    std::error_code ec;
//    add_buffer(key, file_name, buffer, ec);
//    throw(ec, "add_buffer");
//}

//void mime::add_buffer(std::string_view key, std::string_view file_name, 
//        const std::shared_ptr<std::string>& buffer, std::error_code& ec) {
//    buffers_.push_back(buffer);
//}

//void mime::add_buffer(std::string_view key, std::string_view file_name, 
//        const std::shared_ptr<std::string>& buffer, std::string_view content_type) {
//    std::error_code ec;
//    add_buffer(key, file_name, buffer, content_type, ec);
//    throw(ec, "add_buffer");
//}

//void mime::add_buffer(std::string_view key, std::string_view file_name, 
//        const std::shared_ptr<std::string>& buffer, std::string_view content_type, std::error_code& ec) {
//    buffers_.push_back(buffer);
//}

} // curl namespace
USERVER_NAMESPACE_END