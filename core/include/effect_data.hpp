#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Rina::UI {

// EffectModel の純粋データ表現。値セマンティクス（コピー可）。
// ECS::EffectStackComponent・Undoスナップショット・Clipboardの正本として使う。
// 計算キャッシュ（lastDuration / resolvedCache）は含めない。
struct EffectData {
    QString id;
    QString name;
    QString kind;
    QStringList categories;
    QString qmlSource;
    QVariantMap uiDefinition;
    bool enabled = true;
    QVariantMap params;
    QVariantMap keyframeTracks;
};

// EffectModel → EffectData 変換（effect_model.hpp をインクルードした .cpp 内でのみ使う）
class EffectModel;
EffectData effectDataFromModel(const EffectModel &model);

// EffectData → EffectModel 復元（UI層のみで使用）
// syncTrackEndpoints(durationFrames) は呼び出し元で別途呼ぶこと
EffectModel *effectModelFromData(const EffectData &data, QObject *parent = nullptr);

} // namespace Rina::UI
