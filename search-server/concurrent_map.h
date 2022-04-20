#pragma once
#include <mutex>
#include <vector>
#include <map>

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> lg;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count)
        : bucket_count_(bucket_count)
        , buckets_(bucket_count)
        , muts_(bucket_count)
    {}

    Access operator[](const Key& key) {
        size_t index = abs(static_cast<int>(key)) % bucket_count_;
        std::map<Key, Value>& bucket = buckets_[index];
        return { std::lock_guard(muts_[index]), bucket[key] };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> res;
        for (size_t i = 0; i < bucket_count_; i++) {
            std::lock_guard lg(muts_[i]);
            res.insert(buckets_[i].begin(), buckets_[i].end());
        }
        return res;
    }

private:
    size_t bucket_count_;
    std::vector<std::map<Key, Value>> buckets_;
    std::vector<std::mutex> muts_;
};