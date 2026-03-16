#include <userver/utest/assert_macros.hpp>

#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/value_builder.hpp>

#include <schemas/array_of_xcpptype.hpp>
#include <schemas/custom_cpp_type.hpp>

USERVER_NAMESPACE_BEGIN

TEST(Custom, Int) {
    auto json = formats::json::MakeObject("integer", 12);
    auto custom = json.As<ns::ObjWithCustom>();
    EXPECT_EQ(custom.integer, std::chrono::milliseconds(12));

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json);

    const auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjWithCustom>{});
    EXPECT_EQ(custom2, custom);
}

TEST(Custom, String) {
    auto json = formats::json::MakeObject("string", "make love");
    auto custom = json.As<ns::ObjWithCustom>();
    EXPECT_EQ(custom.string, my::CustomString{"make love"});

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json);

    const auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjWithCustom>{});
    EXPECT_EQ(custom2, custom);
}

TEST(Custom, Decimal) {
    auto json = formats::json::MakeObject("decimal", "12.3456789");
    auto custom = json.As<ns::ObjWithCustom>();
    EXPECT_EQ(custom.decimal, decimal64::Decimal<10>::FromBiased(1234567890, 8));

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json);

    const auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjWithCustom>{});
    EXPECT_EQ(custom2, custom);
}

TEST(Custom, Boolean) {
    auto json = formats::json::MakeObject("boolean", true);
    auto custom = json.As<ns::ObjWithCustom>();
    EXPECT_EQ(custom.boolean, my::CustomBoolean{true});

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json);

    const auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjWithCustom>{});
    EXPECT_EQ(custom2, custom);
}

TEST(Custom, Number) {
    auto json = formats::json::MakeObject("number", 1.23);
    auto custom = json.As<ns::ObjWithCustom>();
    EXPECT_EQ(custom.number, my::CustomNumber{1.23});

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json);

    const auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjWithCustom>{});
    EXPECT_EQ(custom2, custom);
}

TEST(Custom, Object) {
    auto json = formats::json::MakeObject("object", formats::json::MakeObject("foo", "bar"));
    auto custom = json.As<ns::ObjWithCustom>();
    EXPECT_EQ(custom.object, my::CustomObject{"bar"});

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json);

    const auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjWithCustom>{});
    EXPECT_EQ(custom2, custom);
}

TEST(Custom, XCppContainer) {
    auto json = formats::json::MakeObject("std_array", formats::json::MakeArray("bar", "foo"));
    auto custom = json.As<ns::ObjWithCustom>();
    EXPECT_EQ(custom.std_array, (std::set<std::string>{"foo", "bar"}));

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json);

    const auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjWithCustom>{});
    EXPECT_EQ(custom2, custom);
}

TEST(Custom, XCppType) {
    auto json = formats::json::MakeObject("custom_array", formats::json::MakeArray("bar", "foo"));
    auto custom = json.As<ns::ObjWithCustom>();
    EXPECT_EQ(custom.custom_array, (my::CustomArray<std::string>{std::set<std::string>{"foo", "bar"}}));

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json);

    const auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjWithCustom>{});
    EXPECT_EQ(custom2, custom);
}

TEST(Custom, OneOf) {
    auto json = formats::json::MakeObject("oneOf", 5);
    auto custom = json.As<ns::ObjWithCustom>();
    EXPECT_EQ(*custom.oneOf, my::CustomOneOf(5));

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json) << ToString(json_back) << " " << ToString(json);

    const auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjWithCustom>{});
    EXPECT_EQ(custom2, custom);
}

TEST(Custom, OneOfWithDiscriminator) {
    auto json = formats::json::MakeObject(
        "oneOfWithDiscriminator",
        formats::json::MakeObject("type", "CustomStruct1", "field1", 3)
    );
    auto custom = json.As<ns::ObjWithCustom>();
    EXPECT_EQ(custom.oneOfWithDiscriminator, my::CustomOneOfWithDiscriminator(3));

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json);

    const auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjWithCustom>{});
    EXPECT_EQ(custom2, custom);
}

TEST(Custom, AllOf) {
    auto json = formats::json::MakeObject("allOf", formats::json::MakeObject("field1", "foo", "field2", "bar"));
    auto custom = json.As<ns::ObjWithCustom>();
    EXPECT_EQ(custom.allOf, (my::CustomAllOf{"foo", "bar"}));

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json);

    const auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjWithCustom>{});
    EXPECT_EQ(custom2, custom);
}

TEST(Custom, ArrayOfXCppType) {
    auto json = formats::json::MakeObject(
        "foo",
        formats::json::MakeArray(formats::json::MakeArray(1, 2)),
        "bar",
        formats::json::MakeArray(-1, -2),
        "baz",
        formats::json::MakeArray(-3, 100, "test"),
        "additional1",
        formats::json::MakeArray(formats::json::MakeObject("lat", 4, "lon", 3)),
        "additional2",
        formats::json::MakeArray(
            formats::json::MakeObject("lon", 5, "lat", 6),
            formats::json::MakeObject("lon", 7, "lat", 8)
        ),
        "additional3",
        formats::json::MakeArray()
    );

    auto custom = json.As<ns::ObjectArrayOfXCppType>();
    ns::ObjectArrayOfXCppType ethalon;
    ethalon.foo = std::vector<my::Point>{
        my::Point{1, 2},
    };
    ethalon.bar.emplace();
    ethalon.bar->push_back(my::CustomNumber{-1});
    ethalon.bar->push_back(my::CustomNumber{-2});

    ethalon.baz.emplace();
    ethalon.baz->push_back(my::CustomNumber{-3});
    ethalon.baz->push_back(my::CustomNumber{100});
    ethalon.baz->push_back(my::CustomString{"test"});

    ethalon.extra = {
        {"additional1", std::vector<my::Point>{my::Point{3, 4}}},
        {"additional2", std::vector<my::Point>{my::Point{5, 6}, my::Point{7, 8}}},
        {"additional3", std::vector<my::Point>{}},
    };
    EXPECT_EQ(custom, ethalon);

    auto json_back = formats::json::ValueBuilder{custom}.ExtractValue();
    EXPECT_EQ(json_back, json);

    auto custom2 = FromJsonString(ToString(json), formats::parse::To<ns::ObjectArrayOfXCppType>{});
    EXPECT_EQ(custom2, ethalon);

    auto changed_ethalon = ethalon;
    changed_ethalon.extra["additional2"][1].lat = 42;
    EXPECT_FALSE(changed_ethalon == ethalon);

    changed_ethalon = ethalon;
    changed_ethalon.extra.erase("additional3");
    EXPECT_FALSE(changed_ethalon == ethalon);
}

USERVER_NAMESPACE_END
