#pragma once

/// @file userver/proto-structs/io/userver/utils/box_conv.hpp
/// @brief Provides read/write context class with the ability to handle `userver::utils::Box` conversion

#include <userver/proto-structs/io/userver/utils/box.hpp>

#include <userver/proto-structs/io/fwd.hpp>

USERVER_NAMESPACE_BEGIN

namespace proto_structs::io {

template <typename T, proto_structs::traits::ProtoMessage TMessage>
utils::Box<T> ReadProtoStruct(ReadContext& ctx, To<utils::Box<T>>, const TMessage& msg) {
    return {ReadProtoStruct(ctx, To<T>{}, msg)};
}

template <typename T, proto_structs::traits::ProtoMessage TMessage>
void WriteProtoStruct(WriteContext& ctx, const utils::Box<T>& obj, TMessage& msg) {
    WriteProtoStruct(ctx, *obj, msg);
}

template <typename T, proto_structs::traits::ProtoMessage TMessage>
void WriteProtoStruct(WriteContext& ctx, utils::Box<T>&& obj, TMessage& msg) {
    WriteProtoStruct(ctx, std::move(*obj), msg);
}

}  // namespace proto_structs::io

USERVER_NAMESPACE_END
