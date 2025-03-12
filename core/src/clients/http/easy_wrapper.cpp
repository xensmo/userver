#include <clients/http/easy_wrapper.hpp>

#include <userver/clients/http/client.hpp>
#include <userver/clients/http/response_future.hpp>
#include <userver/utils/assert.hpp>

USERVER_NAMESPACE_BEGIN

namespace clients::http::impl {

EasyWrapper::EasyWrapper(std::shared_ptr<curl::easy>&& easy, Client& client) : easy_(std::move(easy)), client_(client) {
    client_.IncPending();
}

EasyWrapper::EasyWrapper(EasyWrapper&&) noexcept = default;

EasyWrapper::~EasyWrapper() {
    if (easy_) {
        client_.PushIdleEasy(std::move(easy_));
    }
}

}  // namespace clients::http::impl

USERVER_NAMESPACE_END
