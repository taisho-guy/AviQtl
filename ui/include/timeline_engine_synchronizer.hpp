#pragma once
#include "clip_model.hpp"
#include "timeline_types.hpp"
#include <QFutureWatcher>
#include <QObject>
#include <QPointer>

namespace Rina::UI {
class TimelineController;
class ClipModel;

struct ClipEngineResult {
    int clipId;
    int layer;
    int relFrame;
    bool hasAudio = false;
    int startFrame = 0;
    int durationFrames = 0;
    float vol = 1.0f;
    float pan = 0.0f;
    bool mute = false;
    std::vector<float> propertyValues; // 数値化されたパラメータ配列
    QStringList propertyNames;         // 対応するパラメータ名リスト
    QVariantList variantValues;        // 文字列等の非数値パラメータ
    QStringList variantNames;          // 対応する名前リスト
};

class TimelineEngineSynchronizer : public QObject {
    Q_OBJECT
  public:
    explicit TimelineEngineSynchronizer(TimelineController *controller, QObject *parent = nullptr);

    ClipModel *clipModel() const { return m_clipModel; }
    ClipModel *renderModel() const { return m_renderModel; }
    int timelineDuration() const { return m_timelineDuration; }

    void updateActiveClipsList();
    void rebuildClipIndex();

    QVariantMap getCachedParams(int clipId) const { return m_frontParamCache.value(clipId); }

  private:
    void updateECSState(const QList<struct ArchetypeBatch> &batches, int currentFrame, double fps);

    void handleResultsReady();
    void finalizeCommit(const QHash<int, QVariantMap> &newCache);

    TimelineController *m_controller;
    ClipModel *m_clipModel;
    ClipModel *m_renderModel{};

    QList<ClipData *> m_sortedClips;
    int m_maxDuration = 0;
    int m_timelineDuration = 0;

    QFutureWatcher<QList<ClipEngineResult>> m_futureWatcher;
    QHash<int, QVariantMap> m_frontParamCache; // UIスレッドが参照するフロントバッファ
    std::atomic<bool> m_isCommitting{false};   // 二重書き込み防止
};
} // namespace Rina::UI