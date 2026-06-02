#pragma once

#include <userver/storages/redis/command_control.hpp>

#include <storages/redis/util_redistest.hpp>

USERVER_NAMESPACE_BEGIN

/// Use this CommandControl for read commands that follow a write in tests,
/// to avoid replication lag causing flaky failures.
inline const storages::redis::CommandControl kMasterCC = [] {
    storages::redis::CommandControl cc{};
    cc.force_request_to_master = true;
    return cc;
}();

class RedisClientTest : public BaseRedisClientTest {
public:
    void SetUp() override {
        BaseRedisClientTest::SetUp();

        for (size_t shard = 0; shard < GetSentinel()->ShardsCount(); shard++) {
            GetSentinel()->MakeRequest({"flushdb"}, shard, true).Get();
        }
    }
};

USERVER_NAMESPACE_END
