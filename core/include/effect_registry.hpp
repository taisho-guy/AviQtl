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
    QString qmlSource; // QML実装へのパス
    QString color;     // ← 追加: JSON の "color" フィールド（省略可）
    QVariantMap defaultParams;
    QVariantMap uiDefinition; // UI定義（隠しパラメータやウィジェットタイプなど）
    // TODO: パラメータの型情報や範囲制限などをここに追加
};

class EffectRegistry {
  public:
    static EffectRegistry &instance() {
        static EffectRegistry inst;
        return inst;
    }

    void registerEffect(const EffectMetadata &meta) {
        // 登録順を維持するため、IDが新規の場合のみ順序リストに追加
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
