#pragma once
#include "timeline_types.hpp"
#include <QObject>
#include <QUndoStack>
#include <memory>

namespace Rina::UI {
class SelectionService;

class TimelineService : public QObject {
    Q_OBJECT
  public:
    explicit TimelineService(SelectionService *selection, QObject *parent = nullptr);

    // データアクセス
    const QList<ClipData> &clips() const;
    QList<ClipData> &clipsMutable();                 // シリアライザ用
    const QList<ClipData> &clips(int sceneId) const; // 特定シーンのクリップ取得

    // 追加: ネストを解決した「フレーム時点のアクティブクリップ」を返す
    QList<ClipData *> resolvedActiveClipsAt(int frame) const;

    const QList<SceneData> &getAllScenes() const { return m_scenes; }
    void setScenes(const QList<SceneData> &scenes);
    QUndoStack *undoStack() const { return m_undoStack; }

    // 操作 (公開API)
    void undo();
    void redo();
    void createClip(const QString &type, int startFrame, int layer);
    void deleteClip(int clipId);
    void updateClip(int id, int layer, int startFrame, int duration);
    void splitClip(int clipId, int frame);
    void selectClip(int id);

    // シーン管理
    QVariantList scenes() const;
    int currentSceneId() const { return m_currentSceneId; }
    void createScene(const QString &name);
    void removeScene(int sceneId);
    void switchScene(int sceneId);
    void updateSceneSettings(int sceneId, const QString &name, int width, int height, double fps, int totalFrames);

    // エフェクト
    void addEffect(int clipId, const QString &effectId);
    void removeEffect(int clipId, int effectIndex);
    void updateEffectParam(int clipId, int effectIndex, const QString &paramName, const QVariant &value);

    // クリップボード
    void copyClip(int clipId);
    void cutClip(int clipId);
    void pasteClip(int frame, int layer);

    // 内部用 (コマンドから呼び出される)
    void createClipInternal(int clipId, const QString &type, int startFrame, int layer);
    void updateClipInternal(int id, int layer, int startFrame, int duration);
    void addEffectInternal(int clipId, const QString &effectId);
    void addClipDirectInternal(const ClipData &clip);
    void restoreEffectInternal(int clipId, const QVariantMap &data);
    void removeEffectInternal(int clipId, int effectIndex);
    void updateEffectParamInternal(int clipId, int effectIndex, const QString &paramName, const QVariant &value);
    ClipData *findClipById(int clipId);
    const ClipData *findClipById(int clipId) const;

    // ヘルパー
    ClipData deepCopyClip(const ClipData &source);

    // 状態管理
    int nextClipId() const { return m_nextClipId; }
    void setNextClipId(int id) { m_nextClipId = id; }
    int nextSceneId() const { return m_nextSceneId; }
    void setNextSceneId(int id) { m_nextSceneId = id; }

  signals:
    void clipsChanged();
    void scenesChanged();
    void currentSceneIdChanged();
    void clipEffectsChanged(int clipId);
    void clipCreated(int id, int layer, int startFrame, int duration, const QString &type);

  private:
    QList<SceneData> m_scenes;
    int m_currentSceneId = 0;

    SceneData *currentScene();
    const SceneData *currentScene() const;

    int m_nextClipId = 1;
    int m_nextSceneId = 1;
    QUndoStack *m_undoStack;
    std::unique_ptr<ClipData> m_clipboard;
    SelectionService *m_selection;
};
} // namespace Rina::UI