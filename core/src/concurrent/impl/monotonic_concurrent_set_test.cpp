#include <userver/concurrent/impl/monotonic_concurrent_set.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <userver/utest/utest.hpp>

USERVER_NAMESPACE_BEGIN

namespace {

std::atomic<std::ptrdiff_t> test_items_alive{0};

struct TestItem {
    int key;
    std::string value;

    TestItem(int k, std::string v)
        : key(k),
          value(std::move(v))
    {
        ++test_items_alive;
    }

    ~TestItem() { --test_items_alive; }

    // Items do not need to be movable, MonotonicConcurrentSet guarantees reference stability.
    TestItem(TestItem&&) = delete;
    TestItem& operator=(TestItem&&) = delete;

    bool operator==(const TestItem& other) const { return key == other.key; }
};

struct TestItemHash {
    std::size_t operator()(const TestItem& item) const { return std::hash<int>{}(item.key); }
    std::size_t operator()(int key) const { return std::hash<int>{}(key); }
};

struct TestItemEqual {
    bool operator()(const TestItem& a, const TestItem& b) const { return a.key == b.key; }
    bool operator()(const TestItem& a, int key) const { return a.key == key; }
    bool operator()(int key, const TestItem& a) const { return a.key == key; }
};

}  // namespace

UTEST(MonotonicConcurrentSet, Empty) {
    concurrent::impl::MonotonicConcurrentSet<TestItem, TestItemHash, TestItemEqual> set;
    EXPECT_FALSE(set.Find(1).has_value());
    EXPECT_EQ(test_items_alive.load(), 0);
}

UTEST(MonotonicConcurrentSet, BasicEmplace) {
    {
        concurrent::impl::MonotonicConcurrentSet<TestItem, TestItemHash, TestItemEqual> set;

        auto [item1, inserted1] = set.TryEmplace(1, 1, "one");
        EXPECT_TRUE(inserted1);
        EXPECT_EQ(item1.key, 1);
        EXPECT_EQ(item1.value, "one");
        EXPECT_EQ(test_items_alive.load(), 1);

        auto [item2, inserted2] = set.TryEmplace(1, 1, "duplicate");
        EXPECT_FALSE(inserted2);
        EXPECT_EQ(item2.key, 1);
        EXPECT_EQ(item2.value, "one");  // Should return the original
        EXPECT_EQ(test_items_alive.load(), 1);
    }
    EXPECT_EQ(test_items_alive.load(), 0);
}

UTEST(MonotonicConcurrentSet, BasicFind) {
    concurrent::impl::MonotonicConcurrentSet<TestItem, TestItemHash, TestItemEqual> set;

    set.TryEmplace(1, 1, "one");
    set.TryEmplace(2, 2, "two");

    auto found1 = set.Find(1);
    ASSERT_TRUE(found1.has_value());
    EXPECT_EQ(found1->key, 1);
    EXPECT_EQ(found1->value, "one");

    auto found2 = set.Find(2);
    ASSERT_TRUE(found2.has_value());
    EXPECT_EQ(found2->key, 2);
    EXPECT_EQ(found2->value, "two");

    auto not_found = set.Find(3);
    EXPECT_FALSE(not_found.has_value());
}

UTEST(MonotonicConcurrentSet, Rehashing) {
    {
        concurrent::impl::MonotonicConcurrentSet<TestItem, TestItemHash, TestItemEqual> set(4);

        // Insert more items than initial capacity to trigger rehashing.
        for (int i = 0; i < 20; ++i) {
            auto [item, inserted] = set.TryEmplace(i, i, "value_" + std::to_string(i));
            EXPECT_TRUE(inserted);
            EXPECT_EQ(item.key, i);
        }

        // Verify all items are still findable.
        for (int i = 0; i < 20; ++i) {
            auto found = set.Find(i);
            ASSERT_TRUE(found.has_value());
            EXPECT_EQ(found->key, i);
            EXPECT_EQ(found->value, "value_" + std::to_string(i));
        }

        EXPECT_EQ(test_items_alive.load(), 20);
    }
    EXPECT_EQ(test_items_alive.load(), 0);
}

UTEST(MonotonicConcurrentSet, ConstFind) {
    concurrent::impl::MonotonicConcurrentSet<TestItem, TestItemHash, TestItemEqual> set;
    set.TryEmplace(1, 1, "one");

    const auto& const_set = set;
    auto found = const_set.Find(1);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->key, 1);
    EXPECT_EQ(found->value, "one");
}

UTEST(MonotonicConcurrentSet, StressTest) {
    constexpr int kNumThreads = 4;
    constexpr int kItemsPerThread = 100;
    constexpr auto kTestDuration = std::chrono::milliseconds(500);

    const auto deadline = std::chrono::steady_clock::now() + kTestDuration;

    while (std::chrono::steady_clock::now() < deadline) {
        EXPECT_EQ(test_items_alive.load(), 0);
        concurrent::impl::MonotonicConcurrentSet<TestItem, TestItemHash, TestItemEqual> set(4);

        std::atomic<bool> start_flag{false};
        std::atomic<int> ready_count{0};
        std::vector<std::thread> threads;
        threads.reserve(kNumThreads);

        for (int thread_id = 0; thread_id < kNumThreads; ++thread_id) {
            threads.emplace_back([&set, &start_flag, &ready_count, thread_id]() {
                ready_count.fetch_add(1, std::memory_order_relaxed);
                while (!start_flag.load(std::memory_order_acquire)) {
                    // Spin wait.
                }

                for (int i = 0; i < kItemsPerThread; ++i) {
                    int key = (thread_id * kItemsPerThread) + i;
                    auto [item, inserted] = set.TryEmplace(key, key, "value_" + std::to_string(key));
                    EXPECT_TRUE(inserted);
                    EXPECT_EQ(item.key, key);
                }
            });
        }

        while (ready_count.load(std::memory_order_relaxed) < kNumThreads) {
            // Spin wait
        }

        // Start all threads simultaneously.
        start_flag.store(true, std::memory_order_release);

        for (auto& thread : threads) {
            thread.join();
        }

        // Verify all items are present.
        for (int thread_id = 0; thread_id < kNumThreads; ++thread_id) {
            for (int i = 0; i < kItemsPerThread; ++i) {
                int key = (thread_id * kItemsPerThread) + i;
                auto found = set.Find(key);
                ASSERT_TRUE(found.has_value()) << "Missing key: " << key;
                EXPECT_EQ(found->key, key);
                EXPECT_EQ(found->value, "value_" + std::to_string(key));
            }
        }

        EXPECT_EQ(test_items_alive.load(), kNumThreads * kItemsPerThread);
    }
}

USERVER_NAMESPACE_END
