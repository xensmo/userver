#pragma once

#include <userver/chaotic/array.hpp>
#include <userver/chaotic/object.hpp>
#include <userver/chaotic/primitive.hpp>
#include <userver/chaotic/ref.hpp>
#include <userver/chaotic/sax_parser/enum.hpp>
#include <userver/chaotic/sax_parser/object.hpp>
#include <userver/chaotic/sax_parser/primitive.hpp>
#include <userver/chaotic/with_type.hpp>
#include <userver/formats/json/parser/dummy_parser.hpp>
#include <userver/formats/json/parser/parser_json.hpp>
#include <userver/utils/box.hpp>
#include <userver/utils/constexpr_indices.hpp>
#include <userver/utils/overloaded.hpp>
#include <userver/utils/trivial_map.hpp>

USERVER_NAMESPACE_BEGIN

namespace chaotic::sax::impl {

template <typename RawParser, typename UserType>
class WithType final : private formats::json::parser::Subscriber<typename RawParser::ResultType> {
public:
    using ResultType = UserType;

    WithType() { parser_.Subscribe(*this); }

    void Reset() { parser_.Reset(); }

    void Subscribe(formats::json::parser::Subscriber<ResultType>& subscriber) { subscriber_ = &subscriber; }

    formats::json::parser::BaseParser& GetParser() { return parser_; }

private:
    void OnSend(typename RawParser::ResultType&& value) override {
        auto user_value = [this, &value] {
            try {
                return Convert(value, convert::To<UserType>{});
            } catch (const std::exception& e) {
                formats::json::parser::BaseParser& base = parser_;
                chaotic::ThrowForPath<formats::json::Value>(e.what(), base.GetCurrentPath());
            }
        }();
        subscriber_->OnSend(std::move(user_value));
    }

    RawParser parser_;
    formats::json::parser::Subscriber<ResultType>* subscriber_{nullptr};
};

template <typename Parser, typename... Validator>
class WithValidators final : private formats::json::parser::Subscriber<typename Parser::ResultType> {
public:
    using ResultType = typename Parser::ResultType;

    WithValidators() { parser_.Subscribe(*this); }

    void Reset() { parser_.Reset(); }

    void Subscribe(formats::json::parser::Subscriber<ResultType>& subscriber) { subscriber_ = &subscriber; }

    formats::json::parser::BaseParser& GetParser() { return parser_; }

private:
    void OnSend(ResultType&& value) override {
        try {
            (Validator::Validate(value), ...);
        } catch (const std::exception& e) {
            formats::json::parser::BaseParser& base = parser_;
            chaotic::ThrowForPath<formats::json::Value>(e.what(), base.GetCurrentPath());
        }
        subscriber_->OnSend(std::move(value));
    }

    Parser parser_;
    formats::json::parser::Subscriber<ResultType>* subscriber_{nullptr};
};

template <typename T, typename ResultType = TypeOfDescriptor<T>>
class RefParser final : private formats::json::parser::Subscriber<ResultType> {
public:
    RefParser()
        : parser_(std::make_unique<chaotic::sax::Parser<T>>())
    {
        parser_->Subscribe(*this);
    }

    void Reset() { parser_->Reset(); }

    void Subscribe(formats::json::parser::Subscriber<utils::Box<ResultType>>& subscriber) { subscriber_ = &subscriber; }

    formats::json::parser::BaseParser& GetParser() { return *parser_; }

private:
    void OnSend(ResultType&& value) { subscriber_->OnSend(utils::Box<ResultType>(std::move(value))); }

    std::unique_ptr<formats::json::parser::TypedParser<ResultType>> parser_;
    formats::json::parser::Subscriber<utils::Box<ResultType>>* subscriber_{nullptr};
};

}  // namespace chaotic::sax::impl

namespace chaotic {

template <typename T>
auto ParserOf(Primitive<T>&)
{
    return sax::Parser<T>();
}

template <typename RawType, typename UserType>
auto ParserOf(WithType<RawType, UserType>&)
{
    using RawParser = sax::Parser<RawType>;
    return sax::impl::WithType<RawParser, UserType>{};
}

template <typename T>
auto ParserOf(Ref<T>&)
{
    return sax::impl::RefParser<T>{};
}

template <typename T, typename Validator0, typename... Validator>
auto ParserOf(Primitive<T, Validator0, Validator...>&)
{
    using Subparser = sax::Parser<T>;
    return sax::impl::WithValidators<Subparser, Validator0, Validator...>{};
}

template <typename ItemDescriptor, typename ArrayType>
auto ParserOf(chaotic::Array<ItemDescriptor, ArrayType>&)
{
    using ItemParser = sax::Parser<ItemDescriptor>;
    using ArrayParser = formats::json::parser::ArrayParser<typename ItemParser::ResultType, ItemParser, ArrayType>;
    return sax::JoinedParser<ArrayParser, ItemParser>{};
}

template <typename ItemDescriptor, typename ArrayType, typename Validator0, typename... Validator>
auto ParserOf(chaotic::Array<ItemDescriptor, ArrayType, Validator0, Validator...>&)
{
    using Array = chaotic::Array<ItemDescriptor, ArrayType>;
    using Subparser = sax::Parser<Array>;
    return sax::impl::WithValidators<Subparser, Validator0, Validator...>{};
}

template <typename StructType, typename Unknown, typename... Fields>
auto ParserOf(Object<StructType, Unknown, Fields...>&)
{
    return sax::impl::ObjectParser<StructType, Unknown, Fields...>{};
}

}  // namespace chaotic

USERVER_NAMESPACE_END
