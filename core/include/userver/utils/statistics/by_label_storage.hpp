#pragma once

/// @file userver/utils/statistics/by_label_storage.hpp
/// @brief @copybrief utils::statistics::MonotonicByLabelStorage

#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include <boost/container_hash/hash.hpp>
#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#include <boost/pfr/tuple_size.hpp>

#include <userver/concurrent/impl/monotonic_concurrent_set.hpp>
#include <userver/utils/assert.hpp>
#include <userver/utils/fixed_array.hpp>
#include <userver/utils/statistics/labels.hpp>
#include <userver/utils/statistics/writer.hpp>

USERVER_NAMESPACE_BEGIN

namespace utils::statistics {

namespace impl {

template <typename Metric>
concept DumpableMetric = kHasWriterSupport<Metric>;

template <typename Metric>
concept ResettableMetric = requires(Metric& m) { ResetMetric(m); };

template <typename Field>
concept StringViewCompatibleField =
    std::constructible_from<Field, std::string_view> && std::constructible_from<std::string_view, const Field&>;

template <typename Labels>
concept AllFieldsAreStringViewCompatible = []<std::size_t... Is>(std::index_sequence<Is...>) {
    return (StringViewCompatibleField<boost::pfr::tuple_element_t<Is, Labels>> && ...);
}(std::make_index_sequence<boost::pfr::tuple_size_v<Labels>>{});

template <typename Labels>
concept LabelsAggregate = std::is_aggregate_v<Labels> && AllFieldsAreStringViewCompatible<Labels>;

template <typename Labels>
auto LabelsStructToViewArray(const Labels& labels) {
    constexpr std::size_t kN = boost::pfr::tuple_size_v<Labels>;
    std::array<std::string_view, kN> result{};
    boost::pfr::for_each_field(labels, [&result](const auto& field, std::size_t i) {
        result[i] = std::string_view{field};
    });
    return result;
}

template <typename Labels>
constexpr auto GetLabelNames() noexcept {
    return boost::pfr::names_as_array<Labels>();
}

template <typename Labels, std::size_t... Is>
Labels LabelsArrayToStruct(const utils::FixedArray<std::string>& arr, std::index_sequence<Is...>) {
    UASSERT(arr.size() == sizeof...(Is));
    return Labels{std::string_view{arr[Is]}...};
}

template <typename Labels>
Labels LabelsArrayToStruct(const utils::FixedArray<std::string>& arr) {
    return LabelsArrayToStruct<Labels>(arr, std::make_index_sequence<boost::pfr::tuple_size_v<Labels>>{});
}

// Transparent hash for utils::FixedArray<std::string>.
struct FixedStringArrayHash {
    using is_transparent [[maybe_unused]] = void;

    template <typename StringViewRange>
    std::size_t operator()(const StringViewRange& arr) const noexcept {
        std::size_t hash = arr.size();
        for (const auto& s : arr) {
            boost::hash_combine(hash, std::hash<std::string_view>{}(s));
        }
        return hash;
    }
};

// Transparent equality for utils::FixedArray<std::string>.
struct FixedStringArrayEqual {
    using is_transparent [[maybe_unused]] = void;

    template <typename StringViewRange1, typename StringViewRange2>
    bool operator()(const StringViewRange1& a, const StringViewRange2& b) const noexcept {
        UASSERT(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (std::string_view{a[i]} != std::string_view{b[i]}) {
                return false;
            }
        }
        return true;
    }
};

// Entry stored in the set: label values + metric value.
template <typename Metric>
struct ByLabelEntry {
    utils::FixedArray<std::string> labels;
    Metric metric;

    template <std::size_t N, typename... Args>
    explicit ByLabelEntry(const std::array<std::string_view, N>& views, Args&&... args)
        : labels(views.begin(), views.end()),
          metric(std::forward<Args>(args)...)
    {}
};

// Hash for ByLabelEntry - hashes only the labels part.
template <typename Metric>
struct ByLabelEntryHash {
    using is_transparent [[maybe_unused]] = void;

    std::size_t operator()(const ByLabelEntry<Metric>& entry) const noexcept {
        return FixedStringArrayHash{}(entry.labels);
    }

    template <std::size_t N>
    std::size_t operator()(const std::array<std::string_view, N>& key) const noexcept {
        return FixedStringArrayHash{}(key);
    }
};

// Equality for ByLabelEntry - compares only the labels part.
template <typename Metric>
struct ByLabelEntryEqual {
    using is_transparent [[maybe_unused]] = void;

    bool operator()(const ByLabelEntry<Metric>& a, const ByLabelEntry<Metric>& b) const noexcept {
        return FixedStringArrayEqual{}(a.labels, b.labels);
    }

    template <std::size_t N>
    bool operator()(const ByLabelEntry<Metric>& a, const std::array<std::string_view, N>& b) const noexcept {
        return FixedStringArrayEqual{}(a.labels, b);
    }

    template <std::size_t N>
    bool operator()(const std::array<std::string_view, N>& a, const ByLabelEntry<Metric>& b) const noexcept {
        return FixedStringArrayEqual{}(a, b.labels);
    }
};

}  // namespace impl

/// @ingroup userver_universal
///
/// @brief Thread-safe monotonic storage of metrics indexed by label values.
///
/// `Labels` must be an aggregate type where all fields are interconvertible
/// with `std::string_view`, i.e. constructible from `std::string_view` and
/// convertible to `std::string_view`. This includes `std::string_view` itself,
/// `utils::Required<std::string_view>`, `std::string`, @ref utils::StrongTypedef, etc.
///
/// Label names are taken from `Labels` field names.
///
/// Items can only be added, never removed.
///
/// @warning Avoid storing high-cardinality values as labels, especially when the client input can directly
/// customize label values. This can lead to security issues where clients can cause metrics quota exhaustion
/// or crashes due to OOM errors.
///
/// ## Usage of MonotonicByLabelStorage
///
/// Define a labels struct with `std::string_view` fields:
/// @snippet core/src/utils/statistics/by_label_storage_test.cpp  by_label_storage labels struct
///
/// Use @ref utils::Required for fields without adequate defaults to make sure they are always provided.
///
/// Declare a @ref utils::statistics::MetricTag for the storage:
/// @snippet core/src/utils/statistics/by_label_storage_test.cpp  by_label_storage metric tag
///
/// Write to the storage via `Emplace`:
/// @snippet core/src/utils/statistics/by_label_storage_test.cpp  by_label_storage emplace
///
/// ## Advanced usage of MonotonicByLabelStorage
///
/// Any dumpable type is supported as `Metric`. When multiple metrics have the same labels,
/// declare a struct with metrics and create a storage of structs instead of creating multiple storages.
/// See @ref scripts/docs/en/userver/metrics.md .
///
/// `MonotonicByLabelStorage` is also composable the other way around, it can be included in larger metric structures
/// as a field.
///
/// @tparam Labels An aggregate type with `std::string_view` fields.
/// @tparam Metric The metric type. Must support `DumpMetric(Writer&, const Metric&)`.
template <typename Labels, typename Metric>
requires impl::LabelsAggregate<Labels> && impl::DumpableMetric<Metric>
class MonotonicByLabelStorage final {
public:
    /// @brief Create an empty storage.
    MonotonicByLabelStorage() = default;

    MonotonicByLabelStorage(MonotonicByLabelStorage&&) = delete;
    MonotonicByLabelStorage& operator=(MonotonicByLabelStorage&&) = delete;

    /// @brief Get or create a metric for the given label values.
    /// @param labels Label values (all fields must be `std::string_view`).
    /// @param args Arguments forwarded to `Metric` constructor on first insertion.
    /// @return Reference to the metric.
    template <typename... Args>
    requires std::constructible_from<Metric, Args...>
    Metric& Emplace(const Labels& labels, Args&&... args) {
        const auto view_array = impl::LabelsStructToViewArray(labels);
        auto [entry, inserted] = set_.TryEmplace(view_array, view_array, std::forward<Args>(args)...);
        (void)inserted;
        return entry.metric;
    }

    /// @brief Find a metric by label values without creating it.
    /// @return Pointer to the metric, or nullptr if not found.
    Metric* GetIfExists(const Labels& labels) {
        const auto view_array = impl::LabelsStructToViewArray(labels);
        auto ref = set_.Find(view_array);
        if (ref) {
            return &ref->metric;
        }
        return nullptr;
    }

    /// @brief Visit all stored metrics.
    /// @param func Callable accepting `(const Labels&, const Metric&)`.
    void VisitAll(std::invocable<const Labels&, const Metric&> auto func) const {
        set_.Visit([&func](const Entry& entry) { func(impl::LabelsArrayToStruct<Labels>(entry.labels), entry.metric); }
        );
    }

    /// @brief Dump all metrics to a Writer, using label names from `Labels` fields.
    friend void DumpMetric(Writer& writer, const MonotonicByLabelStorage& storage) {
        static constexpr auto kLabelNames = impl::GetLabelNames<Labels>();
        static constexpr std::size_t kFieldCount = boost::pfr::tuple_size_v<Labels>;

        std::vector<LabelView> label_views;
        label_views.reserve(kFieldCount);

        storage.set_.Visit([&writer, &label_views](const Entry& entry) {
            label_views.clear();
            for (std::size_t i = 0; i < kFieldCount; ++i) {
                label_views.emplace_back(kLabelNames[i], entry.labels[i]);
            }
            writer.ValueWithLabels(entry.metric, label_views);
        });
    }

    /// @brief Reset all metrics (only available if `Metric` has ADL-found `ResetMetric`).
    friend void ResetMetric(MonotonicByLabelStorage& storage)
    requires impl::ResettableMetric<Metric>
    {
        storage.set_.Visit([](Entry& entry) { ResetMetric(entry.metric); });
    }

private:
    using Entry = impl::ByLabelEntry<Metric>;
    using Set = concurrent::impl::MonotonicConcurrentSet<
        Entry,
        impl::ByLabelEntryHash<Metric>,
        impl::ByLabelEntryEqual<Metric>>;

    Set set_;
};

}  // namespace utils::statistics

USERVER_NAMESPACE_END
