#include <gmock/gmock.h>

#include <storages/postgres/tests/util_pgtest.hpp>

#include <storages/postgres/detail/connection.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/tracing/tags.hpp>
#include <userver/utest/log_capture_fixture.hpp>

USERVER_NAMESPACE_BEGIN

namespace pg = storages::postgres;

namespace {

constexpr std::chrono::seconds kTransactionPoolerNetworkTimeout{2};
constexpr std::chrono::milliseconds kTransactionPoolerStatementTimeout{200};

constexpr pg::CommandControl kTransactionPoolerCmdCtl{
    kTransactionPoolerNetworkTimeout,
    kTransactionPoolerStatementTimeout,
};
constexpr pg::CommandControl kTransactionPoolerDefaultCmdCtl{
    std::chrono::seconds{10},
    std::chrono::seconds{10},
};

pg::ConnectionSettings MakeTransactionPoolerSettings(const pg::ConnectionSettings& base = kCachePreparedStatements) {
    auto settings = base;
    settings.pooler_mode = pg::PoolerMode::kTransaction;
    settings.statement_log_mode = pg::ConnectionSettings::StatementLogMode::kLog;
    return settings;
}

constexpr std::string_view kSetConfigStatementName{"set_config"};

void ZeroBackendStatementTimeout(const pg::detail::ConnectionPtr& conn) {
    conn->Execute("SELECT set_config('statement_timeout', '0', false)");
}

void ExpectBackendStatementTimeout(const pg::detail::ConnectionPtr& conn, std::string_view expected) {
    pg::ResultSet res{nullptr};
    UEXPECT_NO_THROW(res = conn->Execute("SELECT current_setting('statement_timeout')"));
    EXPECT_EQ(expected, res.AsSingleRow<std::string>());
}

class PostgreTransactionModeConnection : public utest::LogCaptureFixture<PostgreSQLBase> {
protected:
    pg::detail::ConnectionPtr MakeConn() {
        pg::detail::ConnectionPtr conn{nullptr};

        UEXPECT_NO_THROW(conn = MakeConnection(GetDsnFromEnv(), GetTaskProcessor(), MakeTransactionPoolerSettings()));
        EXPECT_TRUE(conn);
        return conn;
    }
};

UTEST_F(PostgreTransactionModeConnection, TransactionPoolerSkipsStatementTimeoutOnConnect) {
    const auto conn = MakeConn();

    EXPECT_EQ(std::chrono::milliseconds{0}, conn->GetStatementTimeout());
}

UTEST_F(PostgreTransactionModeConnection, TransactionPoolerAppliesDefaultTimeoutAtBegin) {
    const auto conn = MakeConn();

    const DefaultCommandControlScope scope{kTransactionPoolerCmdCtl};

    UEXPECT_NO_THROW(ZeroBackendStatementTimeout(conn));
    UEXPECT_NO_THROW(conn->Begin({}, {}));

    EXPECT_EQ(kTransactionPoolerStatementTimeout, conn->GetStatementTimeout());
    UEXPECT_NO_THROW(ExpectBackendStatementTimeout(conn, "200ms"));

    UEXPECT_NO_THROW(conn->Rollback());
    UEXPECT_NO_THROW(conn->CancelAndCleanup(utest::kMaxTestWaitTime));
    EXPECT_FALSE(conn->IsBroken());
}

UTEST_F(PostgreTransactionModeConnection, TransactionPoolerBeginCommandControlOverridesDefaultTimeout) {
    const auto conn = MakeConn();

    const DefaultCommandControlScope scope{kTransactionPoolerDefaultCmdCtl};

    UEXPECT_NO_THROW(ZeroBackendStatementTimeout(conn));
    UEXPECT_NO_THROW(conn->Begin({}, {}, kTransactionPoolerCmdCtl));

    EXPECT_EQ(kTransactionPoolerStatementTimeout, conn->GetStatementTimeout());
    UEXPECT_NO_THROW(ExpectBackendStatementTimeout(conn, "200ms"));

    UEXPECT_NO_THROW(conn->Rollback());
    UEXPECT_NO_THROW(conn->CancelAndCleanup(utest::kMaxTestWaitTime));
    EXPECT_FALSE(conn->IsBroken());
}

UTEST_F(PostgreTransactionModeConnection, CancelledInTransactionPoolerByStatementTimeout) {
    const auto conn = MakeConn();

    const DefaultCommandControlScope scope{kTransactionPoolerCmdCtl};

    UEXPECT_NO_THROW(ZeroBackendStatementTimeout(conn));
    UEXPECT_NO_THROW(conn->Begin({}, {}));

    UEXPECT_THROW(conn->Execute("SELECT pg_sleep(1.5)"), pg::QueryCancelled);

    EXPECT_EQ(pg::ConnectionState::kTranError, conn->GetState());
    UEXPECT_NO_THROW(conn->Rollback());
    UEXPECT_NO_THROW(conn->CancelAndCleanup(utest::kMaxTestWaitTime));
    EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
    EXPECT_FALSE(conn->IsBroken());
}

UTEST_F(PostgreTransactionModeConnection, TransactionPoolerStatementCommandControlCancelsInTransaction) {
    const auto conn = MakeConn();

    const DefaultCommandControlScope scope{kTransactionPoolerDefaultCmdCtl};

    UEXPECT_NO_THROW(ZeroBackendStatementTimeout(conn));
    UEXPECT_NO_THROW(conn->Begin({}, {}));

    UEXPECT_THROW(
        conn->Execute(kTransactionPoolerCmdCtl, pg::Query{"SELECT pg_sleep(1.5)"}, pg::ParameterStore{}),
        pg::QueryCancelled
    );

    UEXPECT_NO_THROW(conn->Rollback());
    UEXPECT_NO_THROW(conn->CancelAndCleanup(utest::kMaxTestWaitTime));
    EXPECT_FALSE(conn->IsBroken());
}

UTEST_F(PostgreTransactionModeConnection, TransactionPoolerAutocommitAppliesDefaultTimeout) {
    const auto conn = MakeConn();

    const DefaultCommandControlScope scope{kTransactionPoolerCmdCtl};

    UEXPECT_NO_THROW(ZeroBackendStatementTimeout(conn));

    UEXPECT_THROW(conn->Execute("SELECT pg_sleep(1.5)"), pg::QueryCancelled);
    UEXPECT_NO_THROW(conn->CancelAndCleanup(utest::kMaxTestWaitTime));
    EXPECT_FALSE(conn->IsBroken());
}

UTEST_F(PostgreTransactionModeConnection, TransactionPoolerStatementCommandControlCancelsInAutocommit) {
    const auto conn = MakeConn();

    const DefaultCommandControlScope scope{kTransactionPoolerDefaultCmdCtl};

    UEXPECT_NO_THROW(ZeroBackendStatementTimeout(conn));

    UEXPECT_THROW(
        conn->Execute(kTransactionPoolerCmdCtl, pg::Query{"SELECT pg_sleep(1.5)"}, pg::ParameterStore{}),
        pg::QueryCancelled
    );
    UEXPECT_NO_THROW(conn->CancelAndCleanup(utest::kMaxTestWaitTime));
    EXPECT_FALSE(conn->IsBroken());
}

UTEST_F(PostgreTransactionModeConnection, TransactionPoolerSetsStatementTimeoutOncePerTransaction) {
    const auto conn = MakeConn();

    const DefaultCommandControlScope scope{kTransactionPoolerCmdCtl};

    UEXPECT_NO_THROW(ZeroBackendStatementTimeout(conn));
    GetLogCapture().Clear();

    UEXPECT_NO_THROW(conn->Begin({}, {}));
    EXPECT_EQ(kTransactionPoolerStatementTimeout, conn->GetStatementTimeout());
    UEXPECT_NO_THROW(conn->Execute("SELECT 1"));
    UEXPECT_NO_THROW(conn->Execute("SELECT 2"));
    UEXPECT_NO_THROW(conn->Execute("SELECT current_setting('statement_timeout')"));

    const auto set_config_logs = GetLogCapture().Filter([&](const utest::LogRecord& log) {
        return log.GetTagOptional(tracing::kDatabaseStatementName) == kSetConfigStatementName;
    });
    EXPECT_THAT(set_config_logs, ::testing::SizeIs(1));

    UEXPECT_NO_THROW(conn->Rollback());
    UEXPECT_NO_THROW(conn->CancelAndCleanup(utest::kMaxTestWaitTime));
    EXPECT_TRUE(conn->IsIdle());
}

UTEST_F(PostgreTransactionModeConnection, TransactionPoolerResendsSetConfigWhenCustomTimeoutAppliedLater) {
    const auto conn = MakeConn();

    const DefaultCommandControlScope scope{kTransactionPoolerCmdCtl};

    UEXPECT_NO_THROW(ZeroBackendStatementTimeout(conn));
    GetLogCapture().Clear();

    UEXPECT_NO_THROW(conn->Begin({}, {}));
    EXPECT_EQ(kTransactionPoolerStatementTimeout, conn->GetStatementTimeout());
    UEXPECT_NO_THROW(conn->Execute("SELECT 1"));
    UEXPECT_NO_THROW(conn->Execute(kTransactionPoolerDefaultCmdCtl, pg::Query{"SELECT 2"}, pg::ParameterStore{}));

    const auto set_config_logs = GetLogCapture().Filter([&](const utest::LogRecord& log) {
        return log.GetTagOptional(tracing::kDatabaseStatementName) == kSetConfigStatementName;
    });
    EXPECT_THAT(set_config_logs, ::testing::SizeIs(2));

    EXPECT_EQ(kTransactionPoolerDefaultCmdCtl.statement_timeout_ms, conn->GetStatementTimeout());

    pg::ResultSet backend_timeout{nullptr};
    UEXPECT_NO_THROW(
        backend_timeout = conn->Execute(
            kTransactionPoolerDefaultCmdCtl,
            pg::Query{"SELECT current_setting('statement_timeout')"},
            pg::ParameterStore{}
        )
    );
    EXPECT_EQ("10s", backend_timeout.AsSingleRow<std::string>());

    UEXPECT_NO_THROW(conn->Rollback());
    UEXPECT_NO_THROW(conn->CancelAndCleanup(utest::kMaxTestWaitTime));
    EXPECT_TRUE(conn->IsIdle());
}

}  // namespace

USERVER_NAMESPACE_END
