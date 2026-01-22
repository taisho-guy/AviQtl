#pragma once
#include <QUndoCommand>
#include "timeline_controller.hpp"

namespace Rina::UI {

    class AddClipCommand : public QUndoCommand {
    public:
        AddClipCommand(TimelineController* controller, const QString& type, int startFrame, int layer);
        void undo() override;
        void redo() override;
    private:
        TimelineController* m_controller;
        QString m_type;
        int m_startFrame;
        int m_layer;
        int m_generatedId = -1;
    };

    class MoveClipCommand : public QUndoCommand {
    public:
        MoveClipCommand(TimelineController* controller, int clipId, 
                        int oldLayer, int oldStart, int oldDuration,
                        int newLayer, int newStart, int newDuration);
        void undo() override;
        void redo() override;
    private:
        TimelineController* m_controller;
        int m_clipId;
        int m_oldLayer, m_oldStart, m_oldDuration;
        int m_newLayer, m_newStart, m_newDuration;
    };

    class UpdateEffectParamCommand : public QUndoCommand {
    public:
        UpdateEffectParamCommand(TimelineController* controller, int clipId, int effectIndex, 
                                 const QString& paramName, const QVariant& newValue, const QVariant& oldValue);
        void undo() override;
        void redo() override;
        int id() const override;
        bool mergeWith(const QUndoCommand *other) override;
    private:
        TimelineController* m_controller;
        int m_clipId;
        int m_effectIndex;
        QString m_paramName;
        QVariant m_newValue;
        QVariant m_oldValue;
    };

    class AddEffectCommand : public QUndoCommand {
    public:
        AddEffectCommand(TimelineController* controller, int clipId, const QString& effectId);
        void undo() override;
        void redo() override;
    private:
        TimelineController* m_controller;
        int m_clipId;
        QString m_effectId;
    };

    class RemoveEffectCommand : public QUndoCommand {
    public:
        RemoveEffectCommand(TimelineController* controller, int clipId, int effectIndex);
        void undo() override;
        void redo() override;
        void setRemovedEffect(const EffectData& effect) { m_removedEffect = effect; }
    private:
        TimelineController* m_controller;
        int m_clipId;
        int m_effectIndex;
        EffectData m_removedEffect;
    };
}