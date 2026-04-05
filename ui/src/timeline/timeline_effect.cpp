#include "commands.hpp"
#include "effect_registry.hpp"
#include "selection_service.hpp"
#include "timeline_service.hpp"
#include <QDebug>

namespace Rina::UI {

void TimelineService::addEffect(int clipId, const QString &effectId) {
    auto meta = Rina::Core::EffectRegistry::instance().getEffect(effectId);
    if (meta.id.isEmpty()) {
        return;
    }

    m_undoStack->push(new AddEffectCommand(this, clipId, effectId, meta.name));
}

void TimelineService::addEffectInternal(int clipId, const QString &effectId) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            auto meta = Rina::Core::EffectRegistry::instance().getEffect(effectId);
            auto *model = new EffectModel(meta.id, meta.name, meta.category, meta.defaultParams, meta.qmlSource, meta.uiDefinition, this);
            model->syncTrackEndpoints(clip.durationFrames);
            connect(model, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
            clip.effects.append(model);
            emit clipsChanged();
            emit clipEffectsChanged(clipId);
            break;
        }
    }
}

void TimelineService::restoreEffectInternal(int clipId, const QVariantMap &data) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            auto meta = Rina::Core::EffectRegistry::instance().getEffect(data.value(QStringLiteral("id")).toString());
            auto *model = new EffectModel(data.value(QStringLiteral("id")).toString(), data.value(QStringLiteral("name")).toString(), meta.category, data.value(QStringLiteral("params")).toMap(), data.value(QStringLiteral("qmlSource")).toString(),
                                          data.value(QStringLiteral("uiDefinition")).toMap(), this);
            model->setEnabled(data.value(QStringLiteral("enabled")).toBool());
            model->setKeyframeTracks(data.value(QStringLiteral("keyframes")).toMap());
            connect(model, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
            clip.effects.append(model);
            emit clipsChanged();
            emit clipEffectsChanged(clipId);
            break;
        }
    }
}

void TimelineService::removeEffect(int clipId, int effectIndex) {
    QVariantMap removedData;
    const auto *clip = findClipById(clipId);
    if (clip == nullptr) {
        return;
    }

    int idx = (effectIndex == -1) ? static_cast<int>(clip->effects.size()) - 1 : effectIndex;
    if (idx >= 0 && idx < clip->effects.size()) {
        auto *eff = clip->effects.value(idx);
        removedData.insert(QStringLiteral("id"), eff->id());
        removedData.insert(QStringLiteral("name"), eff->name());
        removedData.insert(QStringLiteral("enabled"), eff->isEnabled());
        removedData.insert(QStringLiteral("params"), eff->params());
        removedData.insert(QStringLiteral("qmlSource"), eff->qmlSource());
        removedData.insert(QStringLiteral("uiDefinition"), eff->uiDefinition());
        removedData.insert(QStringLiteral("keyframes"), eff->keyframeTracks());

        auto *cmd = new RemoveEffectCommand(this, clipId, effectIndex, eff->name());
        cmd->setRemovedEffect(removedData);
        m_undoStack->push(cmd);
    }
}

void TimelineService::removeEffectInternal(int clipId, int effectIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            if (effectIndex == -1) {
                effectIndex = static_cast<int>(clip.effects.size()) - 1;
            }
            if (effectIndex >= 0 && effectIndex < clip.effects.size()) {
                // Transformエフェクトは削除不可
                if (effectIndex == 0 && clip.effects.value(0)->id() == QStringLiteral("transform")) {
                    return;
                }
                auto *eff = clip.effects.takeAt(effectIndex);
                eff->deleteLater();
                emit clipsChanged();
                emit clipEffectsChanged(clipId);
            }
            break;
        }
    }
}

void TimelineService::removeMultipleEffects(int clipId, const QList<int> &indices) {
    const auto *clip = findClipById(clipId);
    if (clip == nullptr)
        return;

    QList<int> sorted;
    for (int idx : indices) {
        if (idx >= 0 && idx < static_cast<int>(clip->effects.size()))
            sorted.append(idx);
    }
    if (sorted.isEmpty())
        return;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    auto *cmd = new RemoveMultipleEffectsCommand(this, clipId, sorted, QObject::tr("エフェクト削除 (%1件)").arg(sorted.size()));
    m_undoStack->push(cmd);
}

void TimelineService::removeMultipleEffectsInternal(int clipId, const QList<int> &sortedDescIndices, QList<QVariantMap> *outData) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            if (outData)
                outData->clear();
            for (int idx : sortedDescIndices) {
                if (idx < 0 || idx >= static_cast<int>(clip.effects.size()))
                    continue;
                if (idx == 0 && clip.effects.value(0)->id() == QStringLiteral("transform"))
                    continue;
                auto *eff = clip.effects.takeAt(idx);
                if (outData) {
                    QVariantMap d;
                    d.insert(QStringLiteral("id"), eff->id());
                    d.insert(QStringLiteral("name"), eff->name());
                    d.insert(QStringLiteral("enabled"), eff->isEnabled());
                    d.insert(QStringLiteral("params"), eff->params());
                    d.insert(QStringLiteral("qmlSource"), eff->qmlSource());
                    d.insert(QStringLiteral("uiDefinition"), eff->uiDefinition());
                    d.insert(QStringLiteral("keyframes"), eff->keyframeTracks());
                    outData->prepend(d);
                }
                eff->deleteLater();
            }
            emit clipsChanged();
            emit clipEffectsChanged(clipId);
            break;
        }
    }
}

void TimelineService::restoreMultipleEffectsInternal(int clipId, const QList<QVariantMap> &ascData) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            for (const auto &d : ascData) {
                auto meta = Rina::Core::EffectRegistry::instance().getEffect(d.value(QStringLiteral("id")).toString());
                auto *model = new EffectModel(d.value(QStringLiteral("id")).toString(), d.value(QStringLiteral("name")).toString(), meta.category, d.value(QStringLiteral("params")).toMap(), d.value(QStringLiteral("qmlSource")).toString(),
                                              d.value(QStringLiteral("uiDefinition")).toMap(), this);
                model->setEnabled(d.value(QStringLiteral("enabled")).toBool());
                model->setKeyframeTracks(d.value(QStringLiteral("keyframes")).toMap());
                connect(model, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
                clip.effects.append(model);
            }
            emit clipsChanged();
            emit clipEffectsChanged(clipId);
            break;
        }
    }
}

void TimelineService::setEffectEnabled(int clipId, int effectIndex, bool enabled) { m_undoStack->push(new SetEffectEnabledCommand(this, clipId, effectIndex, enabled)); }

void TimelineService::setAudioPluginEnabled(int clipId, int index, bool enabled) { m_undoStack->push(new SetAudioPluginEnabledCommand(this, clipId, index, enabled)); }

void TimelineService::reorderEffects(int clipId, int oldIndex, int newIndex) {
    if (oldIndex == newIndex) {
        return;
    }
    m_undoStack->push(new ReorderEffectCommand(this, clipId, oldIndex, newIndex));
}

void TimelineService::reorderMultipleEffects(int clipId, const QVariantList &indicesList, int targetIndex) {
    const auto *clip = findClipById(clipId);
    if (clip == nullptr)
        return;

    const int n = static_cast<int>(clip->effects.size());

    // QVariantList を int に変換
    QList<int> indices;
    for (const QVariant &v : indicesList) {
        bool ok;
        int val = v.toInt(&ok);
        if (ok)
            indices.append(val);
    }

    // Transform (index 0) は移動不可として除外し、重複も除去して昇順ソート
    QList<int> valid;
    for (int idx : indices) {
        if (idx > 0 && idx < n)
            valid.append(idx);
    }
    if (valid.isEmpty())
        return;
    std::sort(valid.begin(), valid.end());
    valid.erase(std::unique(valid.begin(), valid.end()), valid.end());

    // 選択アイテムのセット（O(1) 検索用）
    const QSet<int> selectedSet(valid.begin(), valid.end());

    // 選択アイテムを昇順で収集
    QList<EffectModel *> selected;
    selected.reserve(valid.size());
    for (int idx : valid)
        selected.append(clip->effects.at(idx));

    // 非選択アイテムを収集（Transform を含む）
    QList<EffectModel *> remaining;
    remaining.reserve(n - valid.size());
    for (int i = 0; i < n; i++) {
        if (!selectedSet.contains(i))
            remaining.append(clip->effects.at(i));
    }

    // targetIndex より前にある選択アイテムの数を計算し、挿入位置を調整
    int countBefore = 0;
    for (int idx : valid) {
        if (idx < targetIndex)
            countBefore++;
    }
    // Transform より前への挿入を禁止（insertAt >= 1）
    const int insertAt = std::clamp(targetIndex - countBefore, 1, static_cast<int>(remaining.size()));

    // 新順序を構築
    QList<EffectModel *> newOrder;
    newOrder.reserve(n);
    for (int i = 0; i < insertAt; i++)
        newOrder.append(remaining.at(i));
    for (auto *eff : std::as_const(selected))
        newOrder.append(eff);
    for (int i = insertAt; i < static_cast<int>(remaining.size()); i++)
        newOrder.append(remaining.at(i));

    if (newOrder == clip->effects)
        return;

    const QList<EffectModel *> oldOrder = clip->effects;
    m_undoStack->push(new ReorderMultipleEffectsCommand(this, clipId, oldOrder, newOrder, QObject::tr("エフェクト順序変更 (%1件)").arg(valid.size())));
}

void TimelineService::applyEffectOrderInternal(int clipId, const QList<EffectModel *> &order) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            clip.effects = order;
            emit clipEffectsChanged(clipId);
            emit clipsChanged();
            break;
        }
    }
}

void TimelineService::reorderAudioPlugins(int clipId, int oldIndex, int newIndex) {
    if (oldIndex == newIndex) {
        return;
    }
    m_undoStack->push(new ReorderAudioPluginCommand(this, clipId, oldIndex, newIndex));
}

void TimelineService::reorderEffectsInternal(int clipId, int oldIndex, int newIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if ((clip == nullptr) || oldIndex < 0 || oldIndex >= static_cast<int>(clip->effects.size()) || newIndex < 0 || newIndex >= static_cast<int>(clip->effects.size())) {
        return;
    }

    clip->effects.move(oldIndex, newIndex);

    // UI更新通知
    emit clipEffectsChanged(clipId);
    emit clipsChanged();
}

void TimelineService::setEffectEnabledInternal(int clipId, int effectIndex, bool enabled) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if ((clip == nullptr) || effectIndex < 0 || effectIndex >= static_cast<int>(clip->effects.size())) {
        return;
    }

    clip->effects.value(effectIndex)->setEnabled(enabled);
    emit clipEffectsChanged(clipId);
}

void TimelineService::setAudioPluginEnabledInternal(int clipId, int index, bool enabled) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if ((clip == nullptr) || index < 0 || index >= static_cast<int>(clip->audioPlugins.size())) {
        return;
    }

    clip->audioPlugins[index].enabled = enabled;

    emit clipEffectsChanged(clipId);
    emit clipsChanged(); // エンジン側の同期を促す
}

void TimelineService::reorderAudioPluginsInternal(int clipId, int oldIndex, int newIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if ((clip == nullptr) || oldIndex < 0 || oldIndex >= static_cast<int>(clip->audioPlugins.size()) || newIndex < 0 || newIndex >= static_cast<int>(clip->audioPlugins.size())) {
        return;
    }

    clip->audioPlugins.move(oldIndex, newIndex);

    // UI更新通知
    emit clipEffectsChanged(clipId);
    emit clipsChanged();
}

void TimelineService::copyEffect(int clipId, int effectIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if ((clip != nullptr) && effectIndex >= 0 && effectIndex < static_cast<int>(clip->effects.size())) {
        m_effectClipboard.reset(clip->effects.value(effectIndex)->clone());
    }
}

void TimelineService::pasteEffect(int clipId, int targetIndex) {
    if (!m_effectClipboard) {
        return;
    }
    m_undoStack->push(new PasteEffectCommand(this, clipId, targetIndex, m_effectClipboard.get()));
}

void TimelineService::pasteEffectInternal(int clipId, int targetIndex, EffectModel *effect) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if (clip != nullptr) {
        int idx = std::clamp(targetIndex, 0, static_cast<int>(clip->effects.size()));
        clip->effects.insert(idx, effect->clone());
        emit clipEffectsChanged(clipId);
        emit clipsChanged();
    }
}

void TimelineService::updateEffectParam(int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    QVariant oldValue;
    const auto *clip = findClipById(clipId);
    if ((clip == nullptr) || effectIndex >= static_cast<int>(clip->effects.size())) {
        return;
    }

    const auto *eff = clip->effects.value(effectIndex);
    oldValue = eff->params().value(paramName);

    m_undoStack->push(new UpdateEffectParamCommand(this, clipId, effectIndex, paramName, value, oldValue, eff->name()));
}

void TimelineService::updateEffectParamInternal(int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            if (effectIndex >= 0 && effectIndex < static_cast<int>(clip.effects.size())) {
                clip.effects.value(effectIndex)->setParam(paramName, value);

                // メディアの再読み込みが必要なパラメータ（ファイルパスや参照シーン）が変更された場合、
                // clipsChanged シグナルを発火させて MediaManager 等に通知する
                if (paramName == QLatin1String("path") || paramName == QLatin1String("source") || paramName == QStringLiteral("targetSceneId")) {
                    emit clipsChanged();
                }

                emit effectParamChanged(clipId, effectIndex, paramName, value);
                if (m_selection->selectedClipId() == clipId) {
                    QVariantMap data = m_selection->selectedClipData();
                    data.insert(paramName, value);
                    // select() は単一選択にリセットしてしまうため、
                    // 既存の複数選択リストを破壊しない refreshSelectionData を使用する。
                    // これにより UI からの意図しない書き戻しによる選択解除を防ぐ。
                    m_selection->refreshSelectionData(clipId, data);
                }
            }
            break;
        }
    }
}

void TimelineService::setKeyframe(int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) {
    const auto *clip = findClipById(clipId);
    if ((clip == nullptr) || effectIndex >= clip->effects.size()) {
        return;
    }
    const auto *eff = clip->effects.value(effectIndex);

    bool wasExisting = false;
    QVariant oldValue;
    QVariantMap oldOptions;
    const auto track = eff->keyframeTracks().value(paramName).toList();
    for (const auto &v : std::as_const(track)) {
        const auto m = v.toMap();
        if (m.value(QStringLiteral("frame")).toInt() == frame) {
            wasExisting = true;
            oldValue = m.value(QStringLiteral("value"));
            oldOptions = m;
            break;
        }
    }
    m_undoStack->push(new SetKeyframeCommand(this, clipId, effectIndex, paramName, frame, value, options, oldValue, oldOptions, wasExisting));
}

void TimelineService::removeKeyframe(int clipId, int effectIndex, const QString &paramName, int frame) {
    const auto *clip = findClipById(clipId);
    if ((clip == nullptr) || effectIndex >= clip->effects.size()) {
        return;
    }
    const auto *eff = clip->effects.value(effectIndex);

    QVariant savedValue;
    QVariantMap savedOptions;
    const auto track = eff->keyframeTracks().value(paramName).toList();
    for (const auto &v : std::as_const(track)) {
        const auto m = v.toMap();
        if (m.value(QStringLiteral("frame")).toInt() == frame) {
            savedValue = m.value(QStringLiteral("value"));
            savedOptions = m;
            break;
        }
    }
    m_undoStack->push(new RemoveKeyframeCommand(this, clipId, effectIndex, paramName, frame, savedValue, savedOptions));
}

void TimelineService::setKeyframeInternal(int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) { // NOLINT(bugprone-easily-swappable-parameters)
    const auto *clip = findClipById(clipId);
    if ((clip != nullptr) && effectIndex < clip->effects.size()) {
        clip->effects.value(effectIndex)->setKeyframe(paramName, frame, value, options);
    }
}

void TimelineService::removeKeyframeInternal(int clipId, int effectIndex, const QString &paramName, int frame) { // NOLINT(bugprone-easily-swappable-parameters)
    const auto *clip = findClipById(clipId);
    if ((clip != nullptr) && effectIndex < clip->effects.size()) {
        clip->effects.value(effectIndex)->removeKeyframe(paramName, frame);
    }
}

} // namespace Rina::UI