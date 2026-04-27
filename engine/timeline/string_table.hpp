#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace AviQtl::Engine::Timeline {

// 文字列インターンプール
// MetadataComponent の QString を排除するための ECS 外部管理テーブル
// SoA内にはIDのみ保持し、heap文字列をSoAから完全分離する
// 変更頻度: クリップ追加・削除時のみ（フレームレートと無関係）
class StringTable {
  public:
    // EMPTY_ID=0 は常に空文字列 "" に割り当て済み
    static constexpr uint32_t EMPTY_ID = 0;

    StringTable() {
        m_pool.push_back("");
        m_lookup.emplace("", 0u);
    }

    // 文字列をインターンしてIDを返す（重複なし）
    // 同一文字列は同一IDを保証する
    uint32_t intern(std::string_view s) {
        auto it = m_lookup.find(std::string(s));
        if (it != m_lookup.end()) {
            return it->second;
        }
        const uint32_t id = static_cast<uint32_t>(m_pool.size());
        m_pool.emplace_back(s);
        m_lookup.emplace(s, id);
        return id;
    }

    // IDから文字列を逆引き（O(1)）
    // id が範囲外の場合は空文字列ビューを返す
    std::string_view get(uint32_t id) const noexcept {
        if (id >= static_cast<uint32_t>(m_pool.size())) {
            return {};
        }
        return m_pool[static_cast<std::size_t>(id)];
    }

    std::size_t size() const noexcept { return m_pool.size(); }

  private:
    std::vector<std::string> m_pool;
    std::unordered_map<std::string, uint32_t> m_lookup;
};

} // namespace AviQtl::Engine::Timeline
