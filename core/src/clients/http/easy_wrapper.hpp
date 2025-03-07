#pragma once

#include <memory>

//#include <curl-ev/easy.hpp>
#include <curl-ev/easy_mime.hpp>

USERVER_NAMESPACE_BEGIN

namespace clients::http {
class Client;
}  // namespace clients::http

namespace clients::http::impl {

class EasyWrapper final {
public:
    //EasyWrapper(std::shared_ptr<curl::easy>&& easy, Client& client);
    EasyWrapper(std::shared_ptr<curl::easy_mime>&& easy, Client& client);

    EasyWrapper(const EasyWrapper&) = delete;
    EasyWrapper(EasyWrapper&&) noexcept;
    EasyWrapper& operator=(const EasyWrapper&) = delete;
    EasyWrapper& operator=(EasyWrapper&&) = delete;

    ~EasyWrapper();

    //curl::easy& Easy();
    //const curl::easy& Easy() const;

    inline curl::easy_mime& Easy() { return *easy_; }
    inline const curl::easy_mime& Easy() const { return *easy_; };

private:
    //std::shared_ptr<curl::easy> easy_;
    std::shared_ptr<curl::easy_mime> easy_;
    Client& client_;
};

}  // namespace clients::http::impl

USERVER_NAMESPACE_END
