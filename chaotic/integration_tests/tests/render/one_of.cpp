#include <userver/formats/json/inline.hpp>
#include <userver/utest/assert_macros.hpp>

#include <userver/chaotic/exception.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>

#include <schemas/one_of.hpp>
#include <schemas/one_of_sax_parsers.hpp>

#include "helper.hpp"

USERVER_NAMESPACE_BEGIN

TEST(Simple, OneOf) {
    auto json = formats::json::MakeObject();
    auto obj = json.As<ns::OneOf>();
    EXPECT_EQ(TestWriteToStream(obj), json);

    EXPECT_EQ(std::get<ns::OneOf__O2>(obj), ns::OneOf__O2());

    EXPECT_EQ(TestDomSerializer(obj), json);
}

TEST(Simple, OneOfSax) {
    auto json = formats::json::MakeObject();
    auto obj = CallSaxParser<ns::OneOf>(ToString(json));
    EXPECT_EQ(TestWriteToStream(obj), json);

    EXPECT_EQ(std::get<ns::OneOf__O2>(obj), ns::OneOf__O2());

    EXPECT_EQ(TestDomSerializer(obj), json);
}

// TODO: Number $ref validators are not applied inside oneOf (Number is generated as plain double)
// TEST(Simple, OneOfRefValidatorMinimum) {
//     auto json = formats::json::ValueBuilder(0.5).ExtractValue();
//     UEXPECT_THROW_MSG(
//         json.As<ns::OneOf>(),
//         chaotic::Error<formats::json::Value>,
//         "Error at path '/': Invalid value, minimum=1, given=0"
//     );
// }
//
// TEST(Simple, OneOfRefValidatorMinimumSax) {
//     UEXPECT_THROW_MSG(
//         CallSaxParser<ns::OneOf>("0.5"),
//         formats::json::parser::ParseError,
//         "Invalid value, minimum=1, given=0"
//     );
// }

TEST(Simple, OneOfWithDiscriminatorRefValidatorMinimum) {
    auto json = formats::json::MakeObject("oneof", formats::json::MakeObject("type", "ObjectFoo", "foo", 0));
    UEXPECT_THROW_MSG(
        json.As<ns::ObjectOneOfWithDiscriminator>(),
        chaotic::Error<formats::json::Value>,
        "Error at path 'oneof.foo': Invalid value, minimum=1, given=0"
    );
}

TEST(Simple, OneOfWithDiscriminatorRefValidatorMinLength) {
    auto json = formats::json::MakeObject("oneof", formats::json::MakeObject("type", "ObjectBar", "bar", "ab"));
    UEXPECT_THROW_MSG(
        json.As<ns::ObjectOneOfWithDiscriminator>(),
        chaotic::Error<formats::json::Value>,
        "Error at path 'oneof.bar': Too short string, minimum length=3, given=2"
    );
}

TEST(Simple, OneOfWithDiscriminator) {
    auto json = formats::json::MakeObject("oneof", formats::json::MakeObject("type", "ObjectFoo", "foo", 42));
    auto obj = json.As<ns::ObjectOneOfWithDiscriminator>();
    EXPECT_EQ(std::get<0>(obj.oneof.value()).type, "ObjectFoo");
    EXPECT_EQ(std::get<0>(obj.oneof.value()).foo, 42);
    EXPECT_EQ(TestToJsonString(obj), json);
    EXPECT_EQ(TestWriteToStream(obj), json);
    EXPECT_NE(ToJsonString(obj).find("\"type\""), std::string::npos) << "No 'type'";
    EXPECT_EQ(ToJsonString(obj).find("\"type\""), ToJsonString(obj).rfind("\"type\"")) << "Multiple 'type's";

    EXPECT_EQ(TestDomSerializer(obj), json);

    std::get<0>(obj.oneof.value()).type = "incorrect";
    EXPECT_EQ(std::get<0>(obj.oneof.value()).type, "incorrect");
    EXPECT_EQ(TestDomSerializer(obj), json);
    EXPECT_EQ(TestToJsonString(obj), json);
    EXPECT_NE(ToJsonString(obj).find("\"type\""), std::string::npos) << "No 'type'";
    EXPECT_EQ(ToJsonString(obj).find("\"type\""), ToJsonString(obj).rfind("\"type\"")) << "Multiple 'type's";
}

TEST(Simple, OneOfWithDiscriminatorSax) {
    auto json = formats::json::MakeObject("oneof", formats::json::MakeObject("type", "ObjectFoo", "foo", 42));
    auto obj = CallSaxParser<ns::ObjectOneOfWithDiscriminator>(ToString(json));
    EXPECT_EQ(std::get<0>(obj.oneof.value()).type, "ObjectFoo");
    EXPECT_EQ(std::get<0>(obj.oneof.value()).foo, 42);
    EXPECT_EQ(TestToJsonString(obj), json);
    EXPECT_NE(ToJsonString(obj).find("\"type\""), std::string::npos) << "No 'type'";
    EXPECT_EQ(ToJsonString(obj).find("\"type\""), ToJsonString(obj).rfind("\"type\"")) << "Multiple 'type's";

    EXPECT_EQ(TestDomSerializer(obj), json);
}

USERVER_NAMESPACE_END
