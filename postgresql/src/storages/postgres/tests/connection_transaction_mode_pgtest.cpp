#include <storages/postgres/tests/util_pgtest.hpp>

#include <storages/postgres/detail/connection.hpp>
#include <userver/storages/postgres/exceptions.hpp>

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
    return settings;
}

void ZeroBackendStatementTimeout(const pg::detail::ConnectionPtr& conn) {
    conn->Execute("SELECT set_config('statement_timeout', '0', false)");
}

void ExpectBackendStatementTimeout(const pg::detail::ConnectionPtr& conn, std::string_view expected) {
    pg::ResultSet res{nullptr};
    UEXPECT_NO_THROW(res = conn->Execute("SELECT current_setting('statement_timeout')"));
    EXPECT_EQ(expected, res.AsSingleRow<std::string>());
}

class PostgreTransactionModeConnection : public PostgreSQLBase {
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

// TODO TAXICOMMON-11994: keep until autocommit command-control behavior is finalized for transaction pooler mode
UTEST_F(PostgreTransactionModeConnection, TransactionPoolerAutocommitDoesNotReapplyTimeout) {
    const auto conn = MakeConn();

    const DefaultCommandControlScope scope{kTransactionPoolerCmdCtl};

    UEXPECT_NO_THROW(ZeroBackendStatementTimeout(conn));

    UEXPECT_NO_THROW(conn->Execute("SELECT pg_sleep(0.5)"));
    EXPECT_EQ(pg::ConnectionState::kIdle, conn->GetState());
    EXPECT_FALSE(conn->IsBroken());
}

// TODO TAXICOMMON-11994: keep until autocommit command-control behavior is finalized for transaction pooler mode
UTEST_F(PostgreTransactionModeConnection, TransactionPoolerAutocommitWithCommandControl) {
    const auto conn = MakeConn();

    const DefaultCommandControlScope scope{kTransactionPoolerDefaultCmdCtl};

    UEXPECT_NO_THROW(ZeroBackendStatementTimeout(conn));

    UEXPECT_NO_THROW(conn->Execute(kTransactionPoolerCmdCtl, pg::Query{"SELECT pg_sleep(1.5)"}, pg::ParameterStore{}));
    UEXPECT_NO_THROW(conn->CancelAndCleanup(utest::kMaxTestWaitTime));
    EXPECT_FALSE(conn->IsBroken());
}

}  // namespace

USERVER_NAMESPACE_END
