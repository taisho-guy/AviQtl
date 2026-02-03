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
    const QList<ClipData> &clips() const { return m_clips; }
    QList<ClipData> &clipsMutable() { return m_clips; } // シリアライザ用
    QUndoStack *undoStack() const { return m_undoStack; }

    // 操作 (公開API)
    void undo();
    void redo();
    void createClip(const QString &type, int startFrame, int layer);
    void deleteClip(int clipId);
    void updateClip(int id, int layer, int startFrame, int duration);
    void splitClip(int clipId, int frame);
    void selectClip(int id);

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

  signals:
    void clipsChanged();
    void clipEffectsChanged(int clipId);
    void clipCreated(int id, int layer, int startFrame, int duration, const QString &type);

  private:
    QList<ClipData> m_clips;
    int m_nextClipId = 1;
    QUndoStack *m_undoStack;
    std::unique_ptr<ClipData> m_clipboard;
    SelectionService *m_selection;
};
} // namespace Rina::UI