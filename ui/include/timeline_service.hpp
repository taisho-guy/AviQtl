#pragma once
#include "timeline_types.hpp"
#include <QObject>
#include <QUndoStack>
#include <memory>

namespace Rina::UI {
class SelectionService;

struct SceneData {
    int id;
    QString name;
    int width;
    int height;
    QList<ClipData> clips;
    QUndoStack *undoStack; // シーンごとにUndo履歴を持つ
};

class TimelineService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int currentSceneId READ currentSceneId NOTIFY currentSceneIdChanged)
    Q_PROPERTY(QVariantList scenes READ scenes NOTIFY scenesChanged)

  public:
    explicit TimelineService(SelectionService *selection, QObject *parent = nullptr);

    // Data Access
    const QList<ClipData> &clips() const;
    QList<ClipData> &clipsMutable(); // For serializer (Current Scene)
    QUndoStack *undoStack() const;

    // Scene Management
    Q_INVOKABLE int createScene(const QString &name);
    Q_INVOKABLE void removeScene(int sceneId);
    Q_INVOKABLE void switchScene(int sceneId);
    
    int currentSceneId() const { return m_currentSceneId; }
    QVariantList scenes() const;
    SceneData *getCurrentScene() const;

    // Operations (Public API)
    void undo();
    void redo();
    void createClip(const QString &type, int startFrame, int layer);
    void deleteClip(int clipId);
    void updateClip(int id, int layer, int startFrame, int duration);
    void splitClip(int clipId, int frame);
    void selectClip(int id);

    // Effects
    void addEffect(int clipId, const QString &effectId);
    void removeEffect(int clipId, int effectIndex);
    void updateEffectParam(int clipId, int effectIndex, const QString &paramName, const QVariant &value);

    // Clipboard
    void copyClip(int clipId);
    void cutClip(int clipId);
    void pasteClip(int frame, int layer);

    // Internal (for Commands)
    void createClipInternal(int clipId, const QString &type, int startFrame, int layer);
    void updateClipInternal(int id, int layer, int startFrame, int duration);
    void addEffectInternal(int clipId, const QString &effectId);
    void addClipDirectInternal(const ClipData &clip);
    void restoreEffectInternal(int clipId, const QVariantMap &data);
    void removeEffectInternal(int clipId, int effectIndex);
    void updateEffectParamInternal(int clipId, int effectIndex, const QString &paramName, const QVariant &value);

    // Helpers
    ClipData deepCopyClip(const ClipData &source);

    // State Management
    int nextClipId() const { return m_nextClipId; }
    void setNextClipId(int id) { m_nextClipId = id; }

  signals:
    void clipsChanged();
    void scenesChanged();
    void currentSceneIdChanged();
    void clipEffectsChanged(int clipId);
    void clipCreated(int id, int layer, int startFrame, int duration, const QString &type);

  private:
    QList<std::shared_ptr<SceneData>> m_scenes;
    int m_currentSceneId = 0;
    
    int m_nextClipId = 1;
    std::unique_ptr<ClipData> m_clipboard;
    SelectionService *m_selection;
};
} // namespace Rina::UI