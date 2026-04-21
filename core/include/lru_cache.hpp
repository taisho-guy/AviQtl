#pragma once
#include <list>
#include <optional>
#include <unordered_map>

// LRUキャッシュ: QCache<K,V> の代替
// QCacheはコスト単位だが、ここではエントリ数上限のシンプル版
template <typename K, typename V> class LruCache {
  public:
    explicit LruCache(int maxSize) : m_maxSize(maxSize) {}

    void insert(const K &key, V value) {
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            m_list.erase(it->second.first);
            m_map.erase(it);
        }
        m_list.push_front(key);
        m_map[key] = {m_list.begin(), std::move(value)};
        while (static_cast<int>(m_map.size()) > m_maxSize) {
            m_map.erase(m_list.back());
            m_list.pop_back();
        }
    }

    std::optional<V> get(const K &key) const {
        auto it = m_map.find(key);
        if (it == m_map.end())
            return std::nullopt;
        return it->second.second;
    }

    bool contains(const K &key) const { return m_map.count(key) > 0; }
    void clear() {
        m_list.clear();
        m_map.clear();
    }
    void setMaxSize(int n) { m_maxSize = n; }
    int size() const { return static_cast<int>(m_map.size()); }

  private:
    int m_maxSize;
    std::list<K> m_list;
    struct Entry {
        typename std::list<K>::iterator first;
        V second;
    };
    std::unordered_map<K, Entry> m_map;
};
