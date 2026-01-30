#pragma once
#include <QUndoCommand>
#include "timeline_service.hpp"

namespace Rina::UI {

    class AddClipCommand : public QUndoCommand {
    public:
        AddClipCommand(TimelineService* service, const QString& type, int startFrame, int layer);
        void undo() override;
        void redo() override;
    private:
        TimelineService* m_service;
        QString m_type;
        int m_startFrame;
        int m_layer;
        int m_generatedId = -1;
    };

    class MoveClipCommand : public QUndoCommand {
    public:
        MoveClipCommand(TimelineService* service, int clipId, 
                        int oldLayer, int oldStart, int oldDuration,
                        int newLayer, int newStart, int newDuration);
        void undo() override;
        void redo() override;
    private:
        TimelineService* m_service;
        int m_clipId;
        int m_oldLayer, m_oldStart, m_oldDuration;
        int m_newLayer, m_newStart, m_newDuration;
    };

    class UpdateEffectParamCommand : public QUndoCommand {
    public:
        UpdateEffectParamCommand(TimelineService* service, int clipId, int effectIndex, 
                                 const QString& paramName, const QVariant& newValue, const QVariant& oldValue);
        void undo() override;
        void redo() override;
        int id() const override;
        bool mergeWith(const QUndoCommand *other) override;
    private:
        TimelineService* m_service;
        int m_clipId;
        int m_effectIndex;
        QString m_paramName;
        QVariant m_newValue;
        QVariant m_oldValue;
    };

    class AddEffectCommand : public QUndoCommand {
    public:
        AddEffectCommand(TimelineService* service, int clipId, const QString& effectId);
        void undo() override;
        void redo() override;
    private:
        TimelineService* m_service;
        int m_clipId;
        QString m_effectId;
    };

    class RemoveEffectCommand : public QUndoCommand {
    public:
        RemoveEffectCommand(TimelineService* service, int clipId, int effectIndex);
        void undo() override;
        void redo() override;
        void setRemovedEffect(const EffectData& effect) { m_removedEffect = effect; }
    private:
        TimelineService* m_service;
        int m_clipId;
        int m_effectIndex;
        EffectData m_removedEffect;
    };
}