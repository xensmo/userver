#include <storages/postgres/tests/util_pgtest.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/sleep.hpp>

USERVER_NAMESPACE_BEGIN

namespace pg = storages::postgres;

namespace {

UTEST_P(PostgreConnection, Cancel) {
    /*
     * IF THIS TEST FAILS DURING UPDATE OF libpq VERSION IN CONTRIB,
     * WRITE SOMEONE FROM TPS-60916
     * We verify the logic of request cancellation in PostgreSQL.
     * To do this, we start an asynchronous task that performs a long sleep on the PostgreSQL side (putting the
     * connection into the busy state). Then we cancel this task. The connection should remain busy. Next, we issue a
     * query cancellation request with a timeout shorter than the sleep duration on the PostgreSQL side (this is
     * important!). If the query is canceled successfully, the connection will transition to the idle state. If the
     * cancellation does not work, we will catch a timeout exception during DiscardInput. This happens specifically
     * because the cancel timeout is shorter than the duration passed to pg_sleep.
     */
    auto& conn = GetConn();
    CheckConnection(conn);

    // Setup query timeout for 1 minute
    const auto cmd_ctrl = pg::CommandControl{
        /*network_timeout*/ std::chrono::seconds{60},
        /*statement_timeout*/ std::chrono::seconds{60},
    };

    // sleep on postgres side for 1 minute
    auto task = engine::CriticalAsyncNoSpan([&conn, &cmd_ctrl]() {
        LOG_DEBUG() << "Enter pg_sleep";
        conn->Execute("select pg_sleep(60)", /*query_params*/ {}, cmd_ctrl);
        LOG_DEBUG() << "Return from pg_sleep";
    });

    // short sleep to ensure select in async task was started
    engine::SleepFor(std::chrono::milliseconds{10});
    ASSERT_FALSE(conn->IsIdle()) << "connection must be in busy state";

    // cancel task
    task.SyncCancel();
    ASSERT_FALSE(conn->IsIdle()) << "connection must remain in busy state";

    // try to cleanup with short timeout
    // if cancel did not work, here we will get timeout exception
    // because cancel timeout (5s) is less than pg_sleep(60s)
    ASSERT_NO_THROW(conn->CancelAndCleanup(std::chrono::seconds{5}));

    ASSERT_TRUE(conn->IsIdle()) << "connection must be in idle state";

    ASSERT_NO_THROW(conn->Execute("select 1", /*query_params*/ {}, cmd_ctrl)) << "connection must be usable";
}

}  // namespace

USERVER_NAMESPACE_END
