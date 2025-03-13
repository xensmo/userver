/** @file curl-ev/mime.cpp */

#include <curl-ev/mime.hpp>
#include <curl-ev/error_code.hpp>
#include <curl-ev/wrappers.hpp>

#include <userver/logging/log.hpp>

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

void mime::add_content(std::string_view key, std::string_view content) {
    LOG_TRACE() << "mime::add_content start " << this;

    if(!mime_) {
        LOG_ERROR() << "mime failed!";
        return;
    }
    // content part
    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_data(part_, content.data(), content.length());
    // key part
    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_data(part_, key.data(), key.length());
    native::curl_mime_name(part_, key.data());
    //
    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_subparts(part_, mime_);
    native::curl_mime_type(part_, "multipart/form-data");


    LOG_TRACE() << "mime::add_content finished " << this;
}

void mime::add_content(std::string_view key, std::string_view content, std::string_view content_type) {
    LOG_TRACE() << "mime::add_content start " << this;

    if(!mime_) {
        LOG_ERROR() << "mime failed!";
        return;
    }
    // content part
    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_data(part_, content.data(), content.length());
    native::curl_mime_type(part_, content_type.data());
    // key part
    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_data(part_, key.data(), key.length());
    native::curl_mime_name(part_, key.data());
    //
    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_subparts(part_, mime_);
    native::curl_mime_type(part_, "multipart/from-data");

    LOG_TRACE() << "mime::add_content finished " << this;
}

void mime::add_buffer(std::string_view key, std::string_view name, const std::shared_ptr<std::string>& buffer) {
    LOG_TRACE() << "mime::add_buffer start " << this;

    if(!mime_) {
        LOG_ERROR() << "mime failed!";
        return;
    }

    buffers_.push_back(buffer);

    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_data(part_, buffer->c_str(), buffer->length());

    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_data(part_, key.data(), key.length());
    native::curl_mime_name(part_, name.data());

    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_subparts(part_, mime_);
    native::curl_mime_type(part_, "multipart/from-data");

    LOG_TRACE() << "mime::add_buffer finished " << this;
}

void mime::add_buffer(std::string_view key, std::string_view name, std::string_view content_type, const std::shared_ptr<std::string>& buffer) {
    LOG_TRACE() << "mime::add_buffer start " << this;

    if(!mime_) {
        LOG_ERROR() << "mime failed!";
        return;
    }

    buffers_.push_back(buffer);

    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_data(part_, buffer->c_str(), buffer->length());
    native::curl_mime_type(part_, content_type.data());

    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_data(part_, key.data(), key.length());
    native::curl_mime_name(part_, name.data());

    part_ = native::curl_mime_addpart(mime_);
    native::curl_mime_subparts(part_, mime_);
    native::curl_mime_type(part_, "multipart/from-data");    

    LOG_TRACE() << "mime::add_buffer finished " << this;
}

} // curl namespace
USERVER_NAMESPACE_END