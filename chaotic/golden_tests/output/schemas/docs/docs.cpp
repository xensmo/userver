#include <userver/chaotic/type_bundle_cpp.hpp>

#include "docs.hpp"
#include "docs_parsers.ipp"
#include "docs_sax_parsers.hpp"

namespace ns {

Circle FromJsonString(std::string_view json, USERVER_NAMESPACE::formats::parse::To<Circle>) {
  return USERVER_NAMESPACE::formats::json::parser::ParseToType<
      Circle,
      USERVER_NAMESPACE::chaotic::sax::impl::RemoveUserTypeParser<USERVER_NAMESPACE::chaotic::sax::Parser<Circle>>>(
      json);
}

bool operator==(const Circle& lhs, const Circle& rhs) {
  return lhs.kind == rhs.kind && lhs.radius == rhs.radius && lhs.extra == rhs.extra &&

         true;
}

USERVER_NAMESPACE::logging::LogHelper& operator<<(USERVER_NAMESPACE::logging::LogHelper& lh, const Circle& value) {
  return lh << ToString(USERVER_NAMESPACE::formats::json::ValueBuilder(value).ExtractValue());
}

Circle Parse(USERVER_NAMESPACE::formats::json::Value json, USERVER_NAMESPACE::formats::parse::To<Circle> to) {
  return Parse<USERVER_NAMESPACE::formats::json::Value>(json, to);
}

Circle Parse(USERVER_NAMESPACE::formats::yaml::Value json, USERVER_NAMESPACE::formats::parse::To<Circle> to) {
  return Parse<USERVER_NAMESPACE::formats::yaml::Value>(json, to);
}

Circle Parse(USERVER_NAMESPACE::yaml_config::Value json, USERVER_NAMESPACE::formats::parse::To<Circle> to) {
  return Parse<USERVER_NAMESPACE::yaml_config::Value>(json, to);
}

USERVER_NAMESPACE::formats::json::Value Serialize(
    [[maybe_unused]] const Circle& value,
    USERVER_NAMESPACE::formats::serialize::To<USERVER_NAMESPACE::formats::json::Value>) {
  USERVER_NAMESPACE::formats::json::ValueBuilder vb = value.extra;

  if (value.kind) {
    vb["kind"] = USERVER_NAMESPACE::chaotic::Primitive<std::string>{*value.kind};
  }

  if (value.radius) {
    vb["radius"] = USERVER_NAMESPACE::chaotic::Primitive<double>{*value.radius};
  }

  return vb.ExtractValue();
}

Object FromJsonString(std::string_view json, USERVER_NAMESPACE::formats::parse::To<Object>) {
  return USERVER_NAMESPACE::formats::json::parser::ParseToType<
      Object,
      USERVER_NAMESPACE::chaotic::sax::impl::RemoveUserTypeParser<USERVER_NAMESPACE::chaotic::sax::Parser<Object>>>(
      json);
}

bool operator==(const Object& lhs, const Object& rhs) { return lhs.foo == rhs.foo && lhs.bar == rhs.bar && true; }

USERVER_NAMESPACE::logging::LogHelper& operator<<(USERVER_NAMESPACE::logging::LogHelper& lh, const Object& value) {
  return lh << ToString(USERVER_NAMESPACE::formats::json::ValueBuilder(value).ExtractValue());
}

Object Parse(USERVER_NAMESPACE::formats::json::Value json, USERVER_NAMESPACE::formats::parse::To<Object> to) {
  return Parse<USERVER_NAMESPACE::formats::json::Value>(json, to);
}

Object Parse(USERVER_NAMESPACE::formats::yaml::Value json, USERVER_NAMESPACE::formats::parse::To<Object> to) {
  return Parse<USERVER_NAMESPACE::formats::yaml::Value>(json, to);
}

Object Parse(USERVER_NAMESPACE::yaml_config::Value json, USERVER_NAMESPACE::formats::parse::To<Object> to) {
  return Parse<USERVER_NAMESPACE::yaml_config::Value>(json, to);
}

USERVER_NAMESPACE::formats::json::Value Serialize(
    [[maybe_unused]] const Object& value,
    USERVER_NAMESPACE::formats::serialize::To<USERVER_NAMESPACE::formats::json::Value>) {
  USERVER_NAMESPACE::formats::json::ValueBuilder vb = USERVER_NAMESPACE::formats::common::Type::kObject;

  vb["foo"] = USERVER_NAMESPACE::chaotic::Primitive<int>{value.foo};

  if (value.bar) {
    vb["bar"] = USERVER_NAMESPACE::chaotic::Primitive<std::string>{*value.bar};
  }

  return vb.ExtractValue();
}

ObjectCpp FromJsonString(std::string_view json, USERVER_NAMESPACE::formats::parse::To<ObjectCpp>) {
  return USERVER_NAMESPACE::formats::json::parser::ParseToType<
      ObjectCpp,
      USERVER_NAMESPACE::chaotic::sax::impl::RemoveUserTypeParser<USERVER_NAMESPACE::chaotic::sax::Parser<ObjectCpp>>>(
      json);
}

bool operator==(const ObjectCpp& lhs, const ObjectCpp& rhs) {
  return lhs.some_hyphenated_key == rhs.some_hyphenated_key && true;
}

USERVER_NAMESPACE::logging::LogHelper& operator<<(USERVER_NAMESPACE::logging::LogHelper& lh, const ObjectCpp& value) {
  return lh << ToString(USERVER_NAMESPACE::formats::json::ValueBuilder(value).ExtractValue());
}

ObjectCpp Parse(USERVER_NAMESPACE::formats::json::Value json, USERVER_NAMESPACE::formats::parse::To<ObjectCpp> to) {
  return Parse<USERVER_NAMESPACE::formats::json::Value>(json, to);
}

ObjectCpp Parse(USERVER_NAMESPACE::formats::yaml::Value json, USERVER_NAMESPACE::formats::parse::To<ObjectCpp> to) {
  return Parse<USERVER_NAMESPACE::formats::yaml::Value>(json, to);
}

ObjectCpp Parse(USERVER_NAMESPACE::yaml_config::Value json, USERVER_NAMESPACE::formats::parse::To<ObjectCpp> to) {
  return Parse<USERVER_NAMESPACE::yaml_config::Value>(json, to);
}

USERVER_NAMESPACE::formats::json::Value Serialize(
    [[maybe_unused]] const ObjectCpp& value,
    USERVER_NAMESPACE::formats::serialize::To<USERVER_NAMESPACE::formats::json::Value>) {
  USERVER_NAMESPACE::formats::json::ValueBuilder vb = USERVER_NAMESPACE::formats::common::Type::kObject;

  if (value.some_hyphenated_key) {
    vb["some-hyphenated-key"] = USERVER_NAMESPACE::chaotic::Primitive<std::string>{*value.some_hyphenated_key};
  }

  return vb.ExtractValue();
}

Rectangle FromJsonString(std::string_view json, USERVER_NAMESPACE::formats::parse::To<Rectangle>) {
  return USERVER_NAMESPACE::formats::json::parser::ParseToType<
      Rectangle,
      USERVER_NAMESPACE::chaotic::sax::impl::RemoveUserTypeParser<USERVER_NAMESPACE::chaotic::sax::Parser<Rectangle>>>(
      json);
}

bool operator==(const Rectangle& lhs, const Rectangle& rhs) {
  return lhs.kind == rhs.kind && lhs.width == rhs.width && lhs.height == rhs.height && lhs.extra == rhs.extra &&

         true;
}

USERVER_NAMESPACE::logging::LogHelper& operator<<(USERVER_NAMESPACE::logging::LogHelper& lh, const Rectangle& value) {
  return lh << ToString(USERVER_NAMESPACE::formats::json::ValueBuilder(value).ExtractValue());
}

Rectangle Parse(USERVER_NAMESPACE::formats::json::Value json, USERVER_NAMESPACE::formats::parse::To<Rectangle> to) {
  return Parse<USERVER_NAMESPACE::formats::json::Value>(json, to);
}

Rectangle Parse(USERVER_NAMESPACE::formats::yaml::Value json, USERVER_NAMESPACE::formats::parse::To<Rectangle> to) {
  return Parse<USERVER_NAMESPACE::formats::yaml::Value>(json, to);
}

Rectangle Parse(USERVER_NAMESPACE::yaml_config::Value json, USERVER_NAMESPACE::formats::parse::To<Rectangle> to) {
  return Parse<USERVER_NAMESPACE::yaml_config::Value>(json, to);
}

USERVER_NAMESPACE::formats::json::Value Serialize(
    [[maybe_unused]] const Rectangle& value,
    USERVER_NAMESPACE::formats::serialize::To<USERVER_NAMESPACE::formats::json::Value>) {
  USERVER_NAMESPACE::formats::json::ValueBuilder vb = value.extra;

  if (value.kind) {
    vb["kind"] = USERVER_NAMESPACE::chaotic::Primitive<std::string>{*value.kind};
  }

  if (value.width) {
    vb["width"] = USERVER_NAMESPACE::chaotic::Primitive<double>{*value.width};
  }

  if (value.height) {
    vb["height"] = USERVER_NAMESPACE::chaotic::Primitive<double>{*value.height};
  }

  return vb.ExtractValue();
}

/* Parse(USERVER_NAMESPACE::formats::yaml::Value, To<Shape>) was not generated: ::ns::Shape has JSON-specific field
 * "extra" */

/* Parse(USERVER_NAMESPACE::yaml_config::Value, To<Shape>) was not generated: ::ns::Shape has JSON-specific field
 * "extra" */

USERVER_NAMESPACE::logging::LogHelper& operator<<(USERVER_NAMESPACE::logging::LogHelper& lh, const Status& value) {
  return lh << ToString(value);
}

Status Parse(USERVER_NAMESPACE::formats::json::Value json, USERVER_NAMESPACE::formats::parse::To<Status> to) {
  return Parse<USERVER_NAMESPACE::formats::json::Value>(json, to);
}

Status Parse(USERVER_NAMESPACE::formats::yaml::Value json, USERVER_NAMESPACE::formats::parse::To<Status> to) {
  return Parse<USERVER_NAMESPACE::formats::yaml::Value>(json, to);
}

Status Parse(USERVER_NAMESPACE::yaml_config::Value json, USERVER_NAMESPACE::formats::parse::To<Status> to) {
  return Parse<USERVER_NAMESPACE::yaml_config::Value>(json, to);
}

Status Convert(std::string_view value, USERVER_NAMESPACE::chaotic::convert::To<Status>) {
  const auto result = k__ns__Status_Mapping.TryFindBySecond(value);
  if (result.has_value()) {
    return *result;
  }
  throw std::runtime_error(fmt::format("Invalid enum value ({}) for type ::ns::Status", value));
}

std::optional<Status> TryConvert(std::string_view value, USERVER_NAMESPACE::chaotic::convert::To<Status>) noexcept {
  return k__ns__Status_Mapping.TryFindBySecond(value);
}

Status Parse(std::string_view value, USERVER_NAMESPACE::formats::parse::To<Status>) {
  return Convert(value, USERVER_NAMESPACE::chaotic::convert::To<Status>{});
}

USERVER_NAMESPACE::formats::json::Value Serialize(
    const Status& value, USERVER_NAMESPACE::formats::serialize::To<USERVER_NAMESPACE::formats::json::Value>) {
  return USERVER_NAMESPACE::formats::json::ValueBuilder(ToString(value)).ExtractValue();
}

std::string ToString(Status value) {
  const auto result = k__ns__Status_Mapping.TryFindByFirst(value);
  if (result.has_value()) {
    return std::string{*result};
  }
  throw std::runtime_error(fmt::format("Invalid enum value: {}", static_cast<int>(value)));
}

TreeNode FromJsonString(std::string_view json, USERVER_NAMESPACE::formats::parse::To<TreeNode>) {
  return USERVER_NAMESPACE::formats::json::parser::ParseToType<
      TreeNode,
      USERVER_NAMESPACE::chaotic::sax::impl::RemoveUserTypeParser<USERVER_NAMESPACE::chaotic::sax::Parser<TreeNode>>>(
      json);
}

bool operator==(const TreeNode& lhs, const TreeNode& rhs) {
  return lhs.data == rhs.data && lhs.left == rhs.left && lhs.right == rhs.right && true;
}

USERVER_NAMESPACE::logging::LogHelper& operator<<(USERVER_NAMESPACE::logging::LogHelper& lh, const TreeNode& value) {
  return lh << ToString(USERVER_NAMESPACE::formats::json::ValueBuilder(value).ExtractValue());
}

TreeNode Parse(USERVER_NAMESPACE::formats::json::Value json, USERVER_NAMESPACE::formats::parse::To<TreeNode> to) {
  return Parse<USERVER_NAMESPACE::formats::json::Value>(json, to);
}

TreeNode Parse(USERVER_NAMESPACE::formats::yaml::Value json, USERVER_NAMESPACE::formats::parse::To<TreeNode> to) {
  return Parse<USERVER_NAMESPACE::formats::yaml::Value>(json, to);
}

TreeNode Parse(USERVER_NAMESPACE::yaml_config::Value json, USERVER_NAMESPACE::formats::parse::To<TreeNode> to) {
  return Parse<USERVER_NAMESPACE::yaml_config::Value>(json, to);
}

USERVER_NAMESPACE::formats::json::Value Serialize(
    [[maybe_unused]] const TreeNode& value,
    USERVER_NAMESPACE::formats::serialize::To<USERVER_NAMESPACE::formats::json::Value>) {
  USERVER_NAMESPACE::formats::json::ValueBuilder vb = USERVER_NAMESPACE::formats::common::Type::kObject;

  if (value.data) {
    vb["data"] = USERVER_NAMESPACE::chaotic::Primitive<std::string>{*value.data};
  }

  if (value.left) {
    vb["left"] = USERVER_NAMESPACE::chaotic::Ref<USERVER_NAMESPACE::chaotic::Primitive<::ns::TreeNode>>{*value.left};
  }

  if (value.right) {
    vb["right"] = USERVER_NAMESPACE::chaotic::Ref<USERVER_NAMESPACE::chaotic::Primitive<::ns::TreeNode>>{*value.right};
  }

  return vb.ExtractValue();
}

}  // namespace ns

fmt::format_context::iterator fmt::formatter<ns::Status>::format(const ns::Status& value,
                                                                 fmt::format_context& ctx) const {
  return fmt::format_to(ctx.out(), "{}", ToString(value));
}

