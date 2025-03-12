#pragma once

#include <memory>
#include <string>

USERVER_NAMESPACE_BEGIN

namespace curl {
#if LIBCURL_VERSION_NUM <= 0x074700
class form;
#endif 
}  // namespace curl

namespace clients::http {
#if LIBCURL_VERSION_NUM <= 0x074700
class Form final {
public:
    Form();
    ~Form();

    Form(const Form&) = delete;
    Form(Form&&) noexcept;
    Form& operator=(const Form&) = delete;
    Form& operator=(Form&&) noexcept;

    void AddContent(std::string_view key, std::string_view content);
    void AddContent(std::string_view key, std::string_view content, const std::string& content_type);

    void AddBuffer(const std::string& key, const std::string& file_name, const std::shared_ptr<std::string>& buffer);

    void AddBuffer(const std::string& key, const std::string& file_name, const std::shared_ptr<std::string>& buffer, const std::string& content_type);

    /// @cond
    // Call of this method will invalidate the form
    std::unique_ptr<curl::form> GetNative() &&;
    /// @endcond

private:
    std::unique_ptr<curl::form> impl_;
};
#endif // LIBCURL_VERSION_NUM
}  // namespace clients::http

USERVER_NAMESPACE_END
