#include <userver/utest/assert_macros.hpp>

#include <userver/chaotic/exception.hpp>
#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/parser/exception.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>

#include <schemas/const.hpp>

USERVER_NAMESPACE_BEGIN

// ---- StringConst ----

TEST(Const, StringConstValid) {
    auto json = formats::json::FromString(R"("active")");
    auto val = json.As<ns::StringConst>();
    EXPECT_EQ(val, ns::StringConst{});
}

TEST(Const, StringConstInvalid) {
    auto json = formats::json::FromString(R"("inactive")");
    UEXPECT_THROW_MSG(
        json.As<ns::StringConst>(),
        chaotic::Error<formats::json::Value>,
        "expected='active', actual='inactive'"
    );
}

TEST(Const, StringConstSerialize) {
    const ns::StringConst val{};
    auto json = formats::json::ValueBuilder{val}.ExtractValue();
    EXPECT_EQ(json.As<std::string>(), "active");
}

// ---- IntConst ----

TEST(Const, IntConstValid) {
    auto json = formats::json::FromString("42");
    auto val = json.As<ns::IntConst>();
    EXPECT_EQ(val, ns::IntConst{});
}

TEST(Const, IntConstInvalid) {
    auto json = formats::json::FromString("99");
    UEXPECT_THROW_MSG(json.As<ns::IntConst>(), chaotic::Error<formats::json::Value>, "expected='42', actual='99'");
}

TEST(Const, IntConstSerialize) {
    const ns::IntConst val{};
    auto json = formats::json::ValueBuilder{val}.ExtractValue();
    EXPECT_EQ(json.As<int>(), 42);
}

// ---- Int64Const ----

TEST(Const, Int64ConstValid) {
    auto json = formats::json::FromString("9000000000");
    auto val = json.As<ns::Int64Const>();
    EXPECT_EQ(val, ns::Int64Const{});
}

TEST(Const, Int64ConstInvalid) {
    auto json = formats::json::FromString("1");
    UEXPECT_THROW_MSG(
        json.As<ns::Int64Const>(),
        chaotic::Error<formats::json::Value>,
        "expected='9000000000', actual='1'"
    );
}

// ---- BoolConst ----

TEST(Const, BoolConstValid) {
    auto json = formats::json::FromString("true");
    auto val = json.As<ns::BoolConst>();
    EXPECT_EQ(val, ns::BoolConst{});
}

TEST(Const, BoolConstInvalid) {
    auto json = formats::json::FromString("false");
    UEXPECT_THROW_MSG(
        json.As<ns::BoolConst>(),
        chaotic::Error<formats::json::Value>,
        "expected='true', actual='false'"
    );
}

TEST(Const, BoolConstSerialize) {
    const ns::BoolConst val{};
    auto json = formats::json::ValueBuilder{val}.ExtractValue();
    EXPECT_EQ(json.As<bool>(), true);
}

// ---- StandaloneStringConst (inferred type) ----

TEST(Const, StandaloneStringConstValid) {
    auto json = formats::json::FromString(R"("standalone")");
    auto val = json.As<ns::StandaloneStringConst>();
    EXPECT_EQ(val, ns::StandaloneStringConst{});
}

TEST(Const, StandaloneStringConstInvalid) {
    auto json = formats::json::FromString(R"("other")");
    UEXPECT_THROW_MSG(
        json.As<ns::StandaloneStringConst>(),
        chaotic::Error<formats::json::Value>,
        "expected='standalone', actual='other'"
    );
}

// ---- ObjectWithConst ----

TEST(Const, ObjectWithConstValid) {
    auto json = formats::json::MakeObject("status", "active", "code", 42, "enabled", true);
    auto obj = json.As<ns::ObjectWithConst>();
    EXPECT_EQ(obj.status, ns::StringConst{});
    EXPECT_EQ(obj.code, ns::IntConst{});
    EXPECT_EQ(obj.enabled, ns::BoolConst{});
}

TEST(Const, ObjectWithConstInvalidStatus) {
    auto json = formats::json::MakeObject("status", "inactive", "code", 42, "enabled", true);
    UEXPECT_THROW_MSG(
        json.As<ns::ObjectWithConst>(),
        chaotic::Error<formats::json::Value>,
        "expected='active', actual='inactive'"
    );
}

TEST(Const, ObjectWithConstFromJsonString) {
    auto obj =
        FromJsonString(R"({"status":"active","code":42,"enabled":true})", formats::parse::To<ns::ObjectWithConst>());
    EXPECT_EQ(obj.status, ns::StringConst{});
}

TEST(Const, ObjectWithConstSerialize) {
    const ns::ObjectWithConst obj{
        ns::StringConst{},
        ns::IntConst{},
        ns::BoolConst{},
    };
    auto json = Serialize(obj, formats::serialize::To<formats::json::Value>());
    EXPECT_EQ(json["status"].As<std::string>(), "active");
    EXPECT_EQ(json["code"].As<int>(), 42);
    EXPECT_EQ(json["enabled"].As<bool>(), true);
}

TEST(Const, ObjectWithConstOperatorEq) {
    ns::ObjectWithConst a{ns::StringConst{}, ns::IntConst{}, ns::BoolConst{}};
    ns::ObjectWithConst b{ns::StringConst{}, ns::IntConst{}, ns::BoolConst{}};
    EXPECT_EQ(a, b);
}

USERVER_NAMESPACE_END
