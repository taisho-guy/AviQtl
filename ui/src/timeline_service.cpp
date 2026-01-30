#include "timeline_service.hpp"
#include "commands.hpp"
#include "effect_registry.hpp"
#include "selection_service.hpp"
#include <QDebug>

namespace Rina::UI {

    // === Commands Implementation ===
    AddClipCommand::AddClipCommand(TimelineService* service, const QString& type, int startFrame, int layer)
        : m_service(service), m_type(type), m_startFrame(startFrame), m_layer(layer) { setText("Add Clip"); }
    void AddClipCommand::undo() { /* TODO: Implement remove */ }
    void AddClipCommand::redo() { m_service->createClipInternal(m_type, m_startFrame, m_layer); }

    MoveClipCommand::MoveClipCommand(TimelineService* service, int clipId, int oldLayer, int oldStart, int oldDuration, int newLayer, int newStart, int newDuration)
        : m_service(service), m_clipId(clipId), m_oldLayer(oldLayer), m_oldStart(oldStart), m_oldDuration(oldDuration), m_newLayer(newLayer), m_newStart(newStart), m_newDuration(newDuration) { setText("Move Clip"); }
    void MoveClipCommand::undo() { m_service->updateClipInternal(m_clipId, m_oldLayer, m_oldStart, m_oldDuration); }
    void MoveClipCommand::redo() { m_service->updateClipInternal(m_clipId, m_newLayer, m_newStart, m_newDuration); }

    UpdateEffectParamCommand::UpdateEffectParamCommand(TimelineService* service, int clipId, int effectIndex, const QString& paramName, const QVariant& newValue, const QVariant& oldValue)
        : m_service(service), m_clipId(clipId), m_effectIndex(effectIndex), m_paramName(paramName), m_newValue(newValue), m_oldValue(oldValue) { setText("Update Param: " + paramName); }
    void UpdateEffectParamCommand::undo() { m_service->updateEffectParamInternal(m_clipId, m_effectIndex, m_paramName, m_oldValue); }
    void UpdateEffectParamCommand::redo() { m_service->updateEffectParamInternal(m_clipId, m_effectIndex, m_paramName, m_newValue); }
    int UpdateEffectParamCommand::id() const { return 1001; }
    bool UpdateEffectParamCommand::mergeWith(const QUndoCommand *other) {
        if (other->id() != id()) return false;
        const auto* cmd = static_cast<const UpdateEffectParamCommand*>(other);
        if (cmd->m_clipId != m_clipId || cmd->m_effectIndex != m_effectIndex || cmd->m_paramName != m_paramName) return false;
        m_newValue = cmd->m_newValue;
        return true;
    }

    AddEffectCommand::AddEffectCommand(TimelineService* service, int clipId, const QString& effectId)
        : m_service(service), m_clipId(clipId), m_effectId(effectId) { setText("Add Effect"); }
    void AddEffectCommand::undo() { m_service->removeEffectInternal(m_clipId, -1); }
    void AddEffectCommand::redo() { m_service->addEffectInternal(m_clipId, m_service->createEffectData(m_effectId)); }

    RemoveEffectCommand::RemoveEffectCommand(TimelineService* service, int clipId, int effectIndex)
        : m_service(service), m_clipId(clipId), m_effectIndex(effectIndex) { setText("Remove Effect"); }
    void RemoveEffectCommand::redo() { m_service->removeEffectInternal(m_clipId, m_effectIndex); }
    void RemoveEffectCommand::undo() { m_service->addEffectInternal(m_clipId, m_removedEffect); }

    // === TimelineService Implementation ===

    TimelineService::TimelineService(SelectionService* selection, QObject* parent)
        : QObject(parent), m_undoStack(new QUndoStack(this)), m_selection(selection) {}

    void TimelineService::undo() { m_undoStack->undo(); }
    void TimelineService::redo() { m_undoStack->redo(); }

    void TimelineService::createClip(const QString& type, int startFrame, int layer) {
        m_undoStack->push(new AddClipCommand(this, type, startFrame, layer));
    }

    void TimelineService::createClipInternal(const QString& type, int startFrame, int layer) {
        ClipData newClip;
        newClip.id = m_nextClipId++;
        newClip.type = type;
        newClip.startFrame = startFrame;
        newClip.durationFrames = 100;
        newClip.layer = layer;

        // Transform
        EffectData transform;
        transform.id = "transform"; transform.name = "座標";
        transform.params = {{"x",0},{"y",0},{"z",0},{"scale",100.0},{"aspect",0.0},{"rotationX",0.0},{"rotationY",0.0},{"rotationZ",0.0},{"opacity",1.0}};
        auto* tm = new EffectModel(transform.id, transform.name, transform.params, "", this);
        connect(tm, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
        newClip.effects.append(tm);

        // Object
        auto meta = Rina::Core::EffectRegistry::instance().getEffect(type);
        EffectData content;
        content.id = meta.id.isEmpty() ? "unknown" : meta.id;
        content.name = meta.name.isEmpty() ? "Unknown" : meta.name;
        content.params = meta.defaultParams;
        content.qmlSource = meta.qmlSource;
        auto* cm = new EffectModel(content.id, content.name, content.params, content.qmlSource, this);
        connect(cm, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
        newClip.effects.append(cm);

        m_clips.append(newClip);
        emit clipsChanged();
        emit clipCreated(newClip.id, newClip.layer, newClip.startFrame, newClip.durationFrames, newClip.type);
    }

    void TimelineService::updateClip(int id, int layer, int startFrame, int duration) {
        int oldLayer=0, oldStart=0, oldDur=0;
        for(const auto& c : m_clips) if(c.id == id) { oldLayer=c.layer; oldStart=c.startFrame; oldDur=c.durationFrames; break; }
        m_undoStack->push(new MoveClipCommand(this, id, oldLayer, oldStart, oldDur, layer, startFrame, duration));
    }

    void TimelineService::updateClipInternal(int id, int layer, int startFrame, int duration) {
        if (startFrame < 0) startFrame = 0;
        if (duration < 1) duration = 1;
        if (layer < 0) layer = 0;
        for (auto& clip : m_clips) {
            if (clip.id == id) {
                if (clip.layer != layer || clip.startFrame != startFrame || clip.durationFrames != duration) {
                    clip.layer = layer; clip.startFrame = startFrame; clip.durationFrames = duration;
                    emit clipsChanged();
                    // 選択中の場合、SelectionServiceの更新はController側でシグナルを受けて行うか、
                    // ここでSelectionServiceを叩く。今回はServiceにSelectionServiceを持たせているので更新する。
                    if (m_selection->selectedClipId() == id) {
                        QVariantMap data = m_selection->selectedClipData();
                        data["layer"] = layer;
                        data["startFrame"] = startFrame;
                        data["durationFrames"] = duration;
                        m_selection->select(id, data);
                    }
                }
                break;
            }
        }
    }

    void TimelineService::selectClip(int id) {
        if (m_selection->selectedClipId() == id) return;

        for (const auto& clip : m_clips) {
            if (clip.id == id) {
                QVariantMap cache;
                for(auto* eff : clip.effects) {
                    QVariantMap params = eff->params();
                    for(auto it = params.begin(); it != params.end(); ++it) 
                        cache.insert(it.key(), it.value());
                }
                
                cache["startFrame"] = clip.startFrame;
                cache["durationFrames"] = clip.durationFrames;
                cache["layer"] = clip.layer;
                cache["type"] = clip.type;
                
                m_selection->select(id, cache);
                return;
            }
        }
        m_selection->select(-1, {});
    }

    void TimelineService::deleteClip(int clipId) {
        auto it = std::find_if(m_clips.begin(), m_clips.end(), [clipId](const ClipData& c){ return c.id == clipId; });
        if (it != m_clips.end()) {
            for (auto* eff : it->effects) eff->deleteLater();
            m_clips.erase(it);
            emit clipsChanged();
        }
    }

    void TimelineService::addEffect(int clipId, const QString& effectId) {
        m_undoStack->push(new AddEffectCommand(this, clipId, effectId));
    }

    void TimelineService::addEffectInternal(int clipId, const EffectData& effectData) {
        for (auto& clip : m_clips) {
            if (clip.id == clipId) {
                auto* model = new EffectModel(effectData.id, effectData.name, effectData.params, effectData.qmlSource, this);
                model->setEnabled(effectData.enabled);
                connect(model, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
                clip.effects.append(model);
                emit clipsChanged();
                emit clipEffectsChanged(clipId);
                break;
            }
        }
    }

    void TimelineService::removeEffect(int clipId, int effectIndex) {
        EffectData removed;
        bool found = false;
        for(const auto& c : m_clips) {
            if(c.id == clipId) {
                int idx = (effectIndex == -1) ? c.effects.size() - 1 : effectIndex;
                if(idx >= 0 && idx < c.effects.size()) {
                    removed.id = c.effects[idx]->id();
                    removed.name = c.effects[idx]->name();
                    removed.enabled = c.effects[idx]->isEnabled();
                    removed.params = c.effects[idx]->params();
                    found = true;
                    break;
                }
            }
        }
        if(found) {
            auto* cmd = new RemoveEffectCommand(this, clipId, effectIndex);
            cmd->setRemovedEffect(removed);
            m_undoStack->push(cmd);
        }
    }

    void TimelineService::removeEffectInternal(int clipId, int effectIndex) {
        for (auto& clip : m_clips) {
            if (clip.id == clipId) {
                if (effectIndex == -1) effectIndex = clip.effects.size() - 1;
                if (effectIndex >= 0 && effectIndex < clip.effects.size()) {
                    if (effectIndex == 0 && clip.effects[0]->id() == "transform") return;
                    EffectModel* eff = clip.effects.takeAt(effectIndex);
                    eff->deleteLater();
                    emit clipsChanged();
                    emit clipEffectsChanged(clipId);
                }
                break;
            }
        }
    }

    void TimelineService::updateEffectParam(int clipId, int effectIndex, const QString& paramName, const QVariant& value) {
        QVariant oldValue;
        for(const auto& c : m_clips) {
            if(c.id == clipId && effectIndex < c.effects.size()) {
                oldValue = c.effects[effectIndex]->params().value(paramName);
                break;
            }
        }
        m_undoStack->push(new UpdateEffectParamCommand(this, clipId, effectIndex, paramName, value, oldValue));
    }

    void TimelineService::updateEffectParamInternal(int clipId, int effectIndex, const QString& paramName, const QVariant& value) {
        for (auto& clip : m_clips) {
            if (clip.id == clipId) {
                if (effectIndex >= 0 && effectIndex < clip.effects.size()) {
                    clip.effects[effectIndex]->setParam(paramName, value);
                    emit clipsChanged();
                    if (m_selection->selectedClipId() == clipId) {
                        QVariantMap data = m_selection->selectedClipData();
                        data[paramName] = value;
                        m_selection->select(clipId, data);
                    }
                }
                break;
            }
        }
    }

    EffectData TimelineService::createEffectData(const QString& id) {
        EffectData eff;
        eff.id = id;
        eff.enabled = true;
        const auto meta = Rina::Core::EffectRegistry::instance().getEffect(id);
        if (!meta.id.isEmpty()) {
            eff.name = meta.name;
            eff.qmlSource = meta.qmlSource;
            eff.params = meta.defaultParams;
        } else { eff.name = id; }
        return eff;
    }

    ClipData TimelineService::deepCopyClip(const ClipData& source) {
        ClipData newClip;
        newClip.id = -1;
        newClip.type = source.type;
        newClip.startFrame = source.startFrame;
        newClip.durationFrames = source.durationFrames;
        newClip.layer = source.layer;
        for (const auto* oldEffect : source.effects) {
            auto* newEffect = new EffectModel(oldEffect->id(), oldEffect->name(), oldEffect->params(), oldEffect->qmlSource(), this);
            newEffect->setEnabled(oldEffect->isEnabled());
            newEffect->setKeyframeTracks(oldEffect->keyframeTracks());
            connect(newEffect, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
            newClip.effects.append(newEffect);
        }
        return newClip;
    }

    void TimelineService::copyClip(int clipId) {
        auto it = std::find_if(m_clips.begin(), m_clips.end(), [clipId](const ClipData& c){ return c.id == clipId; });
        if (it != m_clips.end()) m_clipboard = std::make_unique<ClipData>(deepCopyClip(*it));
    }

    void TimelineService::cutClip(int clipId) {
        copyClip(clipId);
        deleteClip(clipId);
    }

    void TimelineService::pasteClip(int frame, int layer) {
        if (!m_clipboard) return;
        ClipData newClip = deepCopyClip(*m_clipboard);
        newClip.id = m_nextClipId++;
        newClip.startFrame = frame;
        newClip.layer = layer;
        m_clips.append(newClip);
        emit clipsChanged();
        emit clipCreated(newClip.id, newClip.layer, newClip.startFrame, newClip.durationFrames, newClip.type);
    }

    void TimelineService::splitClip(int clipId, int frame) {
        // (省略: ロジックはTimelineControllerから移動)
        // 実際にはTimelineControllerの実装をここにコピーする
    }
}