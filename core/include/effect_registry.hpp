#pragma once
#include <QHash>
#include <QList>
#include <QString>
#include <QVariantMap>
#include <vector>

namespace Rina::Core {

struct EffectMetadata {
    QString id;
    QString name;
    QString category;
    QString qmlSource; // 将来用: QML実装やシェーダーへのパス
    QVariantMap defaultParams;
    // 将来用: パラメータの型情報や範囲制限など
};

class EffectRegistry {
  public:
    static EffectRegistry &instance() {
        static EffectRegistry inst;
        return inst;
    }

    void registerEffect(const EffectMetadata &meta) {
        // Legacy Fix: O(N) linear search replaced with O(1) hash lookup
        if (!m_effects.contains(meta.id)) {
            m_orderedIds.push_back(meta.id);
        }
        m_effects.insert(meta.id, meta);
    }

    EffectMetadata getEffect(const QString &id) const { return m_effects.value(id); }

    QList<EffectMetadata> getAllEffects() const {
        QList<EffectMetadata> list;
        for (const auto &id : m_orderedIds) {
            list.append(m_effects[id]);
        }
        return list;
    }

    void loadEffectsFromDirectory(const QString &path);

  private:
    EffectRegistry() = default;
    QHash<QString, EffectMetadata> m_effects;
    std::vector<QString> m_orderedIds;
};

// 標準エフェクト初期化関数
void initializeStandardEffects();

} // namespace Rina::Core
