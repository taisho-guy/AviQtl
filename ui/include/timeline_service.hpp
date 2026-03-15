#pragma once
#include "timeline_types.hpp"
#include <QObject>
#include <QPoint>
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

    // 指定された条件で配置可能な最短の開始フレームを計算する（衝突回避）
    int findVacantFrame(int layer, int startFrame, int duration, int excludeClipId) const;

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
    Q_INVOKABLE int computeMagneticSnapPosition(int clipId, int targetLayer, int proposedStartFrame); // Note: This is for a different snap feature
    Q_INVOKABLE QPoint resolveDragPosition(int clipId, int targetLayer, int proposedStartFrame);
    void selectClip(int id);
    void selectSingleClip(int id);
    void toggleClipSelection(int id);
    void selectClipsInRange(int frameA, int frameB, int layerA, int layerB, bool additive = false);
    void applySelectionIds(const QVariantList &ids);

    // シーン管理
    QVariantList scenes() const;
    int currentSceneId() const { return m_currentSceneId; }
    void createScene(const QString &name);
    void removeScene(int sceneId);
    void switchScene(int sceneId);
    void updateSceneSettings(int sceneId, const QString &name, int width, int height, double fps, int totalFrames, const QString &gridMode, double gridBpm, double gridOffset, int gridInterval, int gridSubdivision, bool enableSnap, int magneticSnapRange);

    // エフェクト
    void addEffect(int clipId, const QString &effectId);
    void removeEffect(int clipId, int effectIndex);
    void updateEffectParam(int clipId, int effectIndex, const QString &paramName, const QVariant &value);
    void setKeyframe(int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options);
    void removeKeyframe(int clipId, int effectIndex, const QString &paramName, int frame);

    // クリップボード
    void copyClip(int clipId);
    void cutClip(int clipId);
    void pasteClip(int frame, int layer);
    void copySelectedClips();
    void cutSelectedClips();
    void deleteSelectedClips();

    // 内部用 (コマンドから呼び出される)
    void deleteClipInternal(int clipId);
    void createClipInternal(int clipId, const QString &type, int startFrame, int layer);
    void updateClipInternal(int id, int layer, int startFrame, int duration);
    void addEffectInternal(int clipId, const QString &effectId);
    void addClipDirectInternal(const ClipData &clip);
    void restoreEffectInternal(int clipId, const QVariantMap &data);
    void removeEffectInternal(int clipId, int effectIndex);
    void updateEffectParamInternal(int clipId, int effectIndex, const QString &paramName, const QVariant &value);
    void setClipboard(const ClipData &clip);
    void setClipboard(const QList<ClipData> &clips);
    void createSceneInternal(int sceneId, const QString &name);
    void removeSceneInternal(int sceneId);
    void restoreSceneInternal(const SceneData &scene);
    void applySceneSettingsInternal(int sceneId, const SceneData &data);
    void setKeyframeInternal(int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options);
    void removeKeyframeInternal(int clipId, int effectIndex, const QString &paramName, int frame);
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
    QList<ClipData> m_clipboard;
    SelectionService *m_selection;
};
} // namespace Rina::UI
