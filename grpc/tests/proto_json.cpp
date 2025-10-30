#include <userver/utest/utest.hpp>

#include <google/protobuf/util/message_differencer.h>

#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/ugrpc/proto_json.hpp>
#include <userver/utest/literals.hpp>

#include <tests/messages.pb.h>

USERVER_NAMESPACE_BEGIN

namespace {

sample::ugrpc::GreetingResponse MakeFilledMessage() {
    sample::ugrpc::GreetingResponse filled_message;
    filled_message.set_name("userver");
    return filled_message;
}

const google::protobuf::util::JsonPrintOptions kEmptyOptions{};

}  // namespace

UTEST(ProtoJson, MessageToJsonStringDefault) {
    const auto json = ugrpc::ToJsonString(MakeFilledMessage());
    ASSERT_EQ(json, R"({"name":"userver","greeting":""})");
}

UTEST(ProtoJson, MessageToJsonStringEmptyOptions) {
    const auto json = ugrpc::ToJsonString(MakeFilledMessage(), kEmptyOptions);
    ASSERT_EQ(json, R"({"name":"userver"})");
}

UTEST(ProtoJson, MessageToJsonDefault) {
    const auto json = ugrpc::MessageToJson(MakeFilledMessage());
    ASSERT_TRUE(json.IsObject());
    EXPECT_EQ(json["greeting"], formats::json::ValueBuilder("").ExtractValue());
    EXPECT_EQ(json["name"], formats::json::ValueBuilder("userver").ExtractValue());
}

UTEST(ProtoJson, MessageToJsonEmptyOptions) {
    const auto json = ugrpc::MessageToJson(MakeFilledMessage(), kEmptyOptions);
    ASSERT_TRUE(json.IsObject());
    EXPECT_FALSE(json.HasMember("greeting"));
    EXPECT_EQ(json["name"], formats::json::ValueBuilder("userver").ExtractValue());
}

UTEST(ProtoJson, JsonToMessageDefault) {
    const auto json = formats::json::FromString(R"({"name":"userver","greeting":"hi"})");
    const auto message = ugrpc::JsonToMessage<sample::ugrpc::GreetingResponse>(json);
    EXPECT_EQ(message.name(), "userver");
    EXPECT_EQ(message.greeting(), "hi");
}

UTEST(ProtoJson, JsonToMessageOptions) {
    const auto json = formats::json::FromString(R"({"name":"userver","greeting":"hi", "garbage":"yay"})");
    google::protobuf::util::JsonParseOptions options;
    options.ignore_unknown_fields = true;
    const auto message = ugrpc::JsonToMessage<sample::ugrpc::GreetingResponse>(json, options);
    EXPECT_EQ(message.name(), "userver");
    EXPECT_EQ(message.greeting(), "hi");
}

UTEST(ProtoJson, FromJsonStringDefault) {
    const auto json_string = R"({"name":"userver","greeting":"hi"})";
    const auto message = ugrpc::FromJsonString<sample::ugrpc::GreetingResponse>(json_string);
    EXPECT_EQ(message.name(), "userver");
    EXPECT_EQ(message.greeting(), "hi");
}

UTEST(ProtoJson, FromJsonStringOptions) {
    const auto json_string = R"({"name":"userver","greeting":"hi", "garbage":"yay"})";
    google::protobuf::util::JsonParseOptions options;
    options.ignore_unknown_fields = true;
    const auto message = ugrpc::FromJsonString<sample::ugrpc::GreetingResponse>(json_string, options);
    EXPECT_EQ(message.name(), "userver");
    EXPECT_EQ(message.greeting(), "hi");
}

namespace {

google::protobuf::Struct MakeTestStruct() {
    google::protobuf::Value null_value;
    null_value.set_null_value(google::protobuf::NullValue{});

    google::protobuf::Value number_value;
    number_value.set_number_value(42);

    google::protobuf::Value string_value;
    string_value.set_string_value("string");

    google::protobuf::Value bool_value;
    bool_value.set_bool_value(true);

    google::protobuf::Value struct_value;
    google::protobuf::Value sub_struct_value;
    sub_struct_value.set_string_value("structString");
    struct_value.mutable_struct_value()->mutable_fields()->insert({"structKey", std::move(sub_struct_value)});

    google::protobuf::Value list_value;
    list_value.mutable_list_value()->add_values()->set_string_value("listString");

    google::protobuf::Struct message;
    message.mutable_fields()->insert({"nullValue", std::move(null_value)});
    message.mutable_fields()->insert({"numberValue", std::move(number_value)});
    message.mutable_fields()->insert({"stringValue", std::move(string_value)});
    message.mutable_fields()->insert({"boolValue", std::move(bool_value)});
    message.mutable_fields()->insert({"structValue", std::move(struct_value)});
    message.mutable_fields()->insert({"listValue", std::move(list_value)});
    return message;
}

formats::json::Value MakeTestJson() {
    return R"(
        {
            "nullValue": null,
            "numberValue": 42,
            "stringValue": "string",
            "boolValue": true,
            "structValue": {
                "structKey": "structString"
            },
            "listValue": [
                "listString"
            ]
        }
    )"_json;
}

}  // namespace

UTEST(ProtoJson, ProtobufStructToJson) {
    const auto protobuf_struct = MakeTestStruct();
    const auto protobuf_struct_as_json = formats::json::ValueBuilder{protobuf_struct}.ExtractValue();
    EXPECT_EQ(protobuf_struct_as_json, MakeTestJson());
}

UTEST(ProtoJson, JsonToProtobufStruct) {
    const auto json = MakeTestJson();
    const auto json_as_protobuf_struct = json.As<google::protobuf::Struct>();
    const auto gt_protobuf_struct = MakeTestStruct();
    EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(json_as_protobuf_struct, gt_protobuf_struct))
        << "Expected:\n"
        << gt_protobuf_struct.Utf8DebugString()  //
        << "\nActual:\n"
        << json_as_protobuf_struct.Utf8DebugString();
}

UTEST(ProtoJson, ProtobufValueToJson) {
    google::protobuf::Value protobuf_value;
    *protobuf_value.mutable_struct_value() = MakeTestStruct();
    const auto protobuf_value_as_json = formats::json::ValueBuilder{protobuf_value}.ExtractValue();
    EXPECT_EQ(protobuf_value_as_json, MakeTestJson());
}

UTEST(ProtoJson, JsonToProtobufValue) {
    const auto json = MakeTestJson();
    const auto json_as_protobuf_value = json.As<google::protobuf::Value>();
    google::protobuf::Value gt_protobuf_value;
    *gt_protobuf_value.mutable_struct_value() = MakeTestStruct();
    EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(json_as_protobuf_value, gt_protobuf_value))
        << "Expected:\n"
        << gt_protobuf_value.Utf8DebugString()  //
        << "\nActual:\n"
        << json_as_protobuf_value.Utf8DebugString();
}

USERVER_NAMESPACE_END
