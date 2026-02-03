#pragma once
#include "timeline_service.hpp"
#include <QUndoCommand>

namespace Rina::UI {

class AddClipCommand : public QUndoCommand {
  public:
    AddClipCommand(TimelineService *service, int clipId, const QString &type, int startFrame, int layer, const QString &clipName);
    void undo() override;
    void redo() override;

  private:
    TimelineService *m_service;
    int m_clipId;
    QString m_type;
    int m_startFrame;
    int m_layer;
    QString m_clipName;
};

class MoveClipCommand : public QUndoCommand {
  public:
    MoveClipCommand(TimelineService *service, int clipId, int oldLayer, int oldStart, int oldDuration, int newLayer, int newStart, int newDuration, const QString &clipName);
    void undo() override;
    void redo() override;

  private:
    TimelineService *m_service;
    int m_clipId;
    int m_oldLayer, m_oldStart, m_oldDuration;
    int m_newLayer, m_newStart, m_newDuration;
    QString m_clipName;
};

class UpdateEffectParamCommand : public QUndoCommand {
  public:
    UpdateEffectParamCommand(TimelineService *service, int clipId, int effectIndex, const QString &paramName, const QVariant &newValue, const QVariant &oldValue, const QString &effectName);
    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

  private:
    TimelineService *m_service;
    int m_clipId;
    int m_effectIndex;
    QString m_paramName;
    QVariant m_newValue;
    QVariant m_oldValue;
    QString m_effectName;
};

class AddEffectCommand : public QUndoCommand {
  public:
    AddEffectCommand(TimelineService *service, int clipId, const QString &effectId, const QString &effectName);
    void undo() override;
    void redo() override;

  private:
    TimelineService *m_service;
    int m_clipId;
    QString m_effectId;
    QString m_effectName;
};

class RemoveEffectCommand : public QUndoCommand {
  public:
    RemoveEffectCommand(TimelineService *service, int clipId, int effectIndex, const QString &effectName);
    void undo() override;
    void redo() override;
    void setRemovedEffect(const QVariantMap &effectData) { m_removedEffectData = effectData; }

  private:
    TimelineService *m_service;
    int m_clipId;
    int m_effectIndex;
    QVariantMap m_removedEffectData;
    QString m_effectName;
};

class SplitClipCommand : public QUndoCommand {
  public:
    SplitClipCommand(TimelineService *service, int clipId, int frame, const QString &clipName);
    void undo() override;
    void redo() override;

  private:
    TimelineService *m_service;
    int m_originalClipId;
    int m_newClipId;
    int m_splitFrame;
    int m_originalDuration;
    QString m_clipName;
};
} // namespace Rina::UI