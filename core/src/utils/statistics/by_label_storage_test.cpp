#include <userver/utils/statistics/by_label_storage.hpp>

#include <atomic>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>

#include <userver/utest/utest.hpp>
#include <userver/utils/not_null.hpp>
#include <userver/utils/required.hpp>
#include <userver/utils/statistics/metric_tag.hpp>
#include <userver/utils/statistics/metrics_storage.hpp>
#include <userver/utils/statistics/rate_counter.hpp>
#include <userver/utils/statistics/storage.hpp>
#include <userver/utils/statistics/testing.hpp>

USERVER_NAMESPACE_BEGIN

namespace {

using Rate = utils::statistics::Rate;
using RateCounter = utils::statistics::RateCounter;

/// A metric with a RateCounter and an atomic<int>, but no ResetMetric.
struct CompositeMetric {
    RateCounter requests;
    std::atomic<int> errors{0};

    CompositeMetric() = default;

    explicit CompositeMetric(Rate initial_requests, int initial_errors)
        : requests(initial_requests),
          errors(initial_errors)
    {}

    // Non-copyable, non-movable (atomics).
    CompositeMetric(const CompositeMetric&) = delete;
    CompositeMetric& operator=(const CompositeMetric&) = delete;
};

[[maybe_unused]] void DumpMetric(utils::statistics::Writer& writer, const CompositeMetric& m) {
    writer["requests"] = m.requests;
    writer["errors"] = m.errors.load();
}

// No ResetMetric for CompositeMetric, used to verify the conditional friend is absent.

template <typename Labels, typename Metric>
utils::statistics::Snapshot MakeSnapshot(const utils::statistics::MonotonicByLabelStorage<Labels, Metric>& storage) {
    utils::statistics::Storage stat_storage;
    auto holder = stat_storage.RegisterWriter("metric", [&storage](utils::statistics::Writer& writer) {
        writer = storage;
    });
    return utils::statistics::Snapshot{stat_storage, "metric"};
}

}  // namespace

struct SingleLabel {
    std::string_view name;
};

struct TwoLabels {
    std::string_view host;
    std::string_view dc;
};

struct ThreeLabels {
    std::string_view region;
    std::string_view zone;
    std::string_view rack;
};

UTEST(MonotonicByLabelStorage, EmptyGetIfExistsReturnsNull) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    EXPECT_EQ(storage.GetIfExists({"foo"}), nullptr);
}

UTEST(MonotonicByLabelStorage, EmptyVisitAllVisitsNothing) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    int call_count = 0;
    storage.VisitAll([&call_count](const auto& /*labels*/, const auto& /*metric*/) { ++call_count; });
    EXPECT_EQ(call_count, 0);
}

UTEST(MonotonicByLabelStorage, EmptyDumpMetricProducesNoOutput) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    const auto snapshot = MakeSnapshot(storage);
    EXPECT_FALSE(snapshot.SingleMetricOptional("").has_value());
}

UTEST(MonotonicByLabelStorage, EmplaceCreatesEntry) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    auto& c = storage.Emplace({"foo"});
    ++c;
    EXPECT_EQ(c.Load(), Rate{1});
}

UTEST(MonotonicByLabelStorage, EmplaceWithArgsForwardsToConstructor) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, CompositeMetric> storage;
    CompositeMetric& m = storage.Emplace({"bar"}, Rate{5}, 42);
    EXPECT_EQ(m.requests.Load(), Rate{5});
    EXPECT_EQ(m.errors.load(), 42);
}

UTEST(MonotonicByLabelStorage, EmplaceIdempotentReturnsSameEntry) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    auto& first = storage.Emplace({"key"});
    first.Add(Rate{7});

    // Second Emplace with same labels must return the existing entry.
    auto& second = storage.Emplace({"key"});
    EXPECT_EQ(&second, &first);
    EXPECT_EQ(second.Load(), Rate{7});
}

UTEST(MonotonicByLabelStorage, EmplaceIdempotentIgnoresConstructorArgs) {
    // RateCounter(Rate) constructor: first Emplace initialises to 10.
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    auto& first = storage.Emplace({"k"}, Rate{10});
    EXPECT_EQ(first.Load(), Rate{10});

    // Constructor arg must be ignored: entry already exists.
    auto& second = storage.Emplace({"k"}, Rate{20});
    EXPECT_EQ(&second, &first);
    EXPECT_EQ(second.Load(), Rate{10});
}

UTEST(MonotonicByLabelStorage, EmplaceDifferentLabelsCreatesSeparateEntries) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    auto& a = storage.Emplace({"a"});
    auto& b = storage.Emplace({"b"});
    EXPECT_NE(&a, &b);
    a.Add(Rate{1});
    b.Add(Rate{2});
    EXPECT_EQ(a.Load(), Rate{1});
    EXPECT_EQ(b.Load(), Rate{2});
}

UTEST(MonotonicByLabelStorage, GetIfExistsFindsExistingEntry) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    auto& emplaced = storage.Emplace({"x"});
    emplaced.Add(Rate{55});

    auto* found = storage.GetIfExists({"x"});
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found, &emplaced);
    EXPECT_EQ(found->Load(), Rate{55});
}

UTEST(MonotonicByLabelStorage, GetIfExistsReturnsNullForMissingEntry) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    storage.Emplace({"present"});
    EXPECT_EQ(storage.GetIfExists({"absent"}), nullptr);
}

UTEST(MonotonicByLabelStorage, GetIfExistsDistinguishesDifferentLabels) {
    utils::statistics::MonotonicByLabelStorage<TwoLabels, RateCounter> storage;
    storage.Emplace({"host1", "dc1"}).Add(Rate{1});
    storage.Emplace({"host1", "dc2"}).Add(Rate{2});
    storage.Emplace({"host2", "dc1"}).Add(Rate{3});

    auto* p11 = storage.GetIfExists({"host1", "dc1"});
    auto* p12 = storage.GetIfExists({"host1", "dc2"});
    auto* p21 = storage.GetIfExists({"host2", "dc1"});
    auto* p22 = storage.GetIfExists({"host2", "dc2"});

    ASSERT_NE(p11, nullptr);
    ASSERT_NE(p12, nullptr);
    ASSERT_NE(p21, nullptr);
    EXPECT_EQ(p22, nullptr);

    EXPECT_EQ(p11->Load(), Rate{1});
    EXPECT_EQ(p12->Load(), Rate{2});
    EXPECT_EQ(p21->Load(), Rate{3});
}

UTEST(MonotonicByLabelStorage, VisitAllVisitsAllEntries) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    storage.Emplace({"a"}).Add(Rate{1});
    storage.Emplace({"b"}).Add(Rate{2});
    storage.Emplace({"c"}).Add(Rate{3});

    std::vector<Rate> visited_rates;
    storage.VisitAll([&visited_rates](const SingleLabel& /*labels*/, const auto& metric) {
        visited_rates.push_back(metric.Load());
    });
    EXPECT_THAT(visited_rates, testing::UnorderedElementsAreArray({Rate{1}, Rate{2}, Rate{3}}));
}

UTEST(MonotonicByLabelStorage, VisitAllProvidesCorrectLabelValues) {
    utils::statistics::MonotonicByLabelStorage<TwoLabels, RateCounter> storage;
    storage.Emplace({"h1", "d1"}).Add(Rate{10});
    storage.Emplace({"h2", "d2"}).Add(Rate{20});

    std::vector<std::pair<std::string, std::string>> all_labels;
    storage.VisitAll([&all_labels](const TwoLabels& labels, const auto& /*metric*/) {
        all_labels.emplace_back(labels.host, labels.dc);
    });

    ASSERT_EQ(all_labels.size(), 2u);
    EXPECT_THAT(all_labels, ::testing::UnorderedElementsAre(std::make_pair("h1", "d1"), std::make_pair("h2", "d2")));
}

UTEST(MonotonicByLabelStorage, DumpMetricSingleLabel) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    storage.Emplace({"myval"}).Add(Rate{7});

    // Field name is "name" (from pfr), value is "myval", metric value is 7.
    const auto snapshot = MakeSnapshot(storage);
    EXPECT_EQ(snapshot.SingleMetric("", {{"name", "myval"}}), Rate{7});
}

UTEST(MonotonicByLabelStorage, DumpMetricTwoLabels) {
    utils::statistics::MonotonicByLabelStorage<TwoLabels, RateCounter> storage;
    storage.Emplace({"srv01", "eu-west"}).Add(Rate{42});

    const auto snapshot = MakeSnapshot(storage);
    EXPECT_EQ(snapshot.SingleMetric("", {{"host", "srv01"}, {"dc", "eu-west"}}), Rate{42});
}

UTEST(MonotonicByLabelStorage, DumpMetricThreeLabels) {
    utils::statistics::MonotonicByLabelStorage<ThreeLabels, RateCounter> storage;
    storage.Emplace({"us", "us-east-1a", "rack3"}).Add(Rate{5});

    const auto snapshot = MakeSnapshot(storage);
    EXPECT_EQ(snapshot.SingleMetric("", {{"region", "us"}, {"zone", "us-east-1a"}, {"rack", "rack3"}}), Rate{5});
}

UTEST(MonotonicByLabelStorage, DumpMetricMultipleEntries) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    storage.Emplace({"alpha"}).Add(Rate{1});
    storage.Emplace({"beta"}).Add(Rate{2});

    const auto snapshot = MakeSnapshot(storage);
    EXPECT_EQ(snapshot.SingleMetric("", {{"name", "alpha"}}), Rate{1});
    EXPECT_EQ(snapshot.SingleMetric("", {{"name", "beta"}}), Rate{2});
}

UTEST(MonotonicByLabelStorage, DumpMetricEmptyStorageProducesNoMetrics) {
    utils::statistics::MonotonicByLabelStorage<TwoLabels, RateCounter> storage;
    const auto snapshot = MakeSnapshot(storage);
    EXPECT_FALSE(snapshot.SingleMetricOptional("").has_value());
}

UTEST(MonotonicByLabelStorage, CompositeMetricEmplaceAndDumpMetric) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, CompositeMetric> storage;
    auto& m = storage.Emplace({"svc"});
    m.requests.Add(Rate{3});
    m.errors.store(1);

    const auto snapshot = MakeSnapshot(storage);
    EXPECT_EQ(snapshot.SingleMetric("requests", {{"name", "svc"}}), Rate{3});
    EXPECT_EQ(snapshot.SingleMetric("errors", {{"name", "svc"}}), std::int64_t{1});
}

UTEST(MonotonicByLabelStorage, ResetMetricResetsAllEntries) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    storage.Emplace({"a"}).Add(Rate{10});
    storage.Emplace({"b"}).Add(Rate{20});
    storage.Emplace({"c"}).Add(Rate{30});

    ResetMetric(storage);

    auto* pa = storage.GetIfExists({"a"});
    auto* pb = storage.GetIfExists({"b"});
    auto* pc = storage.GetIfExists({"c"});
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);
    ASSERT_NE(pc, nullptr);
    EXPECT_EQ(pa->Load(), Rate{0});
    EXPECT_EQ(pb->Load(), Rate{0});
    EXPECT_EQ(pc->Load(), Rate{0});
}

UTEST(MonotonicByLabelStorage, ResetMetricDoesNotRemoveEntries) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    storage.Emplace({"x"}).Add(Rate{99});

    ResetMetric(storage);

    // Entry must still be findable after reset.
    auto* found = storage.GetIfExists({"x"});
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->Load(), Rate{0});
}

// CompositeMetric has no ResetMetric, so MonotonicByLabelStorage<X, CompositeMetric> must not have ResetMetric either.
static_assert(!utils::statistics::impl::ResettableMetric<CompositeMetric>);
static_assert(!utils::statistics::impl::ResettableMetric<
              utils::statistics::MonotonicByLabelStorage<SingleLabel, CompositeMetric>>);

UTEST(MonotonicByLabelStorage, ManyEntriesTriggersRehashing) {
    // Insert enough entries to force multiple rehash rounds.
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    constexpr int kCount = 200;

    std::vector<utils::NotNull<RateCounter*>> entry_references;
    entry_references.reserve(kCount);

    for (int i = 0; i < kCount; ++i) {
        const std::string s = std::to_string(i);
        RateCounter& entry = storage.Emplace({s});
        entry.Add(Rate{static_cast<std::uint64_t>(i)});
        entry_references.push_back(utils::NotNull<RateCounter*>{&entry});
    }

    // All entries must be findable after rehashing, and references must be stable.
    for (int i = 0; i < kCount; ++i) {
        const std::string s = std::to_string(i);
        auto* found = storage.GetIfExists({s});
        ASSERT_NE(found, nullptr) << "Missing entry for i=" << i;
        EXPECT_EQ(found, entry_references[i].GetBase()) << "Reference unstable for i=" << i;
        EXPECT_EQ(found->Load(), Rate{static_cast<std::uint64_t>(i)});
    }
}

UTEST(MonotonicByLabelStorage, LabelValuesAreOwnedAfterEmplace) {
    // Verify that label strings are copied into the entry (not dangling views).
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    {
        std::string ephemeral = "temporary_label_value";
        storage.Emplace({ephemeral}).Add(Rate{123});
        // ephemeral goes out of scope here.
    }
    // The storage must still find the entry by a fresh string_view.
    auto* found = storage.GetIfExists({"temporary_label_value"});
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->Load(), Rate{123});
}

UTEST(MonotonicByLabelStorage, EmptyStringLabelValue) {
    utils::statistics::MonotonicByLabelStorage<SingleLabel, RateCounter> storage;
    storage.Emplace({""}).Add(Rate{1});
    auto* found = storage.GetIfExists({""});
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->Load(), Rate{1});
    EXPECT_EQ(storage.GetIfExists({"nonempty"}), nullptr);
}

UTEST(MonotonicByLabelStorage, TwoLabelsEmptyVsNonEmpty) {
    utils::statistics::MonotonicByLabelStorage<TwoLabels, RateCounter> storage;
    storage.Emplace({"", "dc"}).Add(Rate{1});
    storage.Emplace({"host", ""}).Add(Rate{2});
    storage.Emplace({"", ""}).Add(Rate{3});

    EXPECT_NE(nullptr, storage.GetIfExists({"", "dc"}));
    EXPECT_NE(nullptr, storage.GetIfExists({"host", ""}));
    EXPECT_NE(nullptr, storage.GetIfExists({"", ""}));
    EXPECT_EQ(nullptr, storage.GetIfExists({"host", "dc"}));

    EXPECT_EQ(storage.GetIfExists({"", "dc"})->Load(), Rate{1});
    EXPECT_EQ(storage.GetIfExists({"host", ""})->Load(), Rate{2});
    EXPECT_EQ(storage.GetIfExists({"", ""})->Load(), Rate{3});
}

/// [by_label_storage labels struct]
struct RequestLabels {
    utils::Required<std::string_view> source;
    std::string_view status{"ok"};
};
/// [by_label_storage labels struct]

namespace {

/// [by_label_storage metric tag]
// Needs to be `inline` if defined in a header.
const utils::statistics::MetricTag<
    utils::statistics::MonotonicByLabelStorage<RequestLabels, utils::statistics::RateCounter>>
    kRequestsMetric{"my-service.requests"};
/// [by_label_storage metric tag]

}  // namespace

UTEST(MonotonicByLabelStorage, SnippetUsage) {
    utils::statistics::MetricsStorage metrics_storage;

    /// [by_label_storage emplace]
    auto& storage = metrics_storage.GetMetric(kRequestsMetric);
    storage.Emplace({{"frontend"}, "ok"}).Increment();
    storage.Emplace({{"backend"}, "error"}).Add(utils::statistics::Rate{3});
    /// [by_label_storage emplace]

    const auto snapshot = MakeSnapshot(storage);
    EXPECT_EQ(snapshot.SingleMetric("", {{"source", "frontend"}, {"status", "ok"}}), Rate{1});
    EXPECT_EQ(snapshot.SingleMetric("", {{"source", "backend"}, {"status", "error"}}), Rate{3});
}

USERVER_NAMESPACE_END
