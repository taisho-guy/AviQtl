#pragma once
#include "../../engine/audio_mixer.hpp"
#include "clip_model.hpp"
#include "effect_model.hpp"
#include "project_service.hpp"
#include "selection_service.hpp"
#include "timeline_engine_synchronizer.hpp"
#include "timeline_export_manager.hpp"
#include "timeline_media_manager.hpp"
#include "timeline_service.hpp"
#include "timeline_types.hpp"
#include "transport_service.hpp"
#include <QAbstractListModel>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QVariant>
#include <QtMath>
#include <memory>
#include <vector>

class QUndoStack;

namespace Rina::Core {
class VideoFrameStore;
class VideoEncoder;
} // namespace Rina::Core

namespace Rina::UI { // 元のnamespaceに戻す
class TimelineController : public QObject {
    Q_OBJECT

    // === サービス (サブコントローラ) ===
    Q_PROPERTY(Rina::UI::ProjectService *project READ project CONSTANT)
    Q_PROPERTY(Rina::UI::TransportService *transport READ transport CONSTANT)
    Q_PROPERTY(Rina::UI::SelectionService *selection READ selection CONSTANT)

    // === レガシー / ファサードプロパティ ===
    Q_PROPERTY(double timelineScale READ timelineScale WRITE setTimelineScale NOTIFY timelineScaleChanged)
    Q_PROPERTY(int clipStartFrame READ clipStartFrame WRITE setClipStartFrame NOTIFY clipStartFrameChanged)
    Q_PROPERTY(int clipDurationFrames READ clipDurationFrames WRITE setClipDurationFrames NOTIFY clipDurationFramesChanged)
    Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY layerChanged)
    Q_PROPERTY(bool isClipActive READ isClipActive NOTIFY isClipActiveChanged)
    Q_PROPERTY(QString activeObjectType READ activeObjectType NOTIFY activeObjectTypeChanged)
    Q_PROPERTY(QVariantList clips READ clips NOTIFY clipsChanged)
    Q_PROPERTY(Rina::UI::ClipModel *clipModel READ clipModel CONSTANT)
    Q_PROPERTY(int selectedLayer READ selectedLayer WRITE setSelectedLayer NOTIFY selectedLayerChanged)
    Q_PROPERTY(QVariantList scenes READ scenes NOTIFY scenesChanged)
    Q_PROPERTY(int currentSceneId READ currentSceneId NOTIFY currentSceneIdChanged)

  public:
    explicit TimelineController(QObject *parent = nullptr);

    void setVideoFrameStore(Rina::Core::VideoFrameStore *store);

    Q_INVOKABLE void setCompositeView(QQuickItem *view) { m_compositeView = view; }
    QQuickItem *compositeView() const { return m_compositeView; }

    // サービスアクセサ
    ProjectService *project() const { return m_project; }
    TransportService *transport() const { return m_transport; }
    SelectionService *selection() const { return m_selection; }
    TimelineService *timeline() const { return m_timeline; }
    TimelineMediaManager *mediaManager() const { return m_mediaManager; }

    double timelineScale() const;
    void setTimelineScale(double scale);

    // 汎用プロパティ操作
    Q_INVOKABLE void setClipProperty(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant getClipProperty(const QString &name) const;

    int clipStartFrame() const;
    void setClipStartFrame(int frame);

    int clipDurationFrames() const;
    void setClipDurationFrames(int frames);

    int layer() const;
    void setLayer(int layer);

    int selectedLayer() const { return m_selectedLayer; }
    void setSelectedLayer(int layer);

    bool isClipActive() const;

    Q_INVOKABLE void createObject(const QString &type, int startFrame, int layer);
    QString activeObjectType() const;

    Q_INVOKABLE void log(const QString &msg);
    QVariantList clips() const;
    ClipModel *clipModel() const { return m_engineSync->clipModel(); }

    // クリップの配置・長さを更新 (ID指定)
    Q_INVOKABLE void updateClip(int id, int layer, int startFrame, int duration);
    Q_INVOKABLE void moveSelectedClips(int deltaLayer, int deltaFrame);
    Q_INVOKABLE void applyClipBatchMove(const QVariantList &moves);
    Q_INVOKABLE void resizeSelectedClips(int deltaStartFrame, int deltaDuration);

    // 衝突回避ロジック付きでクリップを移動する（QMLのドラッグ操作から呼ぶ）
    Q_INVOKABLE void moveClipWithCollisionCheck(int clipId, int layer, int startFrame);

    // エフェクト操作
    Q_INVOKABLE QList<QObject *> getClipEffectsModel(int clipId) const;
    Q_INVOKABLE void updateClipEffectParam(int clipId, int effectIndex, const QString &paramName, const QVariant &value);

    // エフェクト・オブジェクトの利用可能リスト取得
    Q_INVOKABLE QVariantList getAvailableEffects() const;
    Q_INVOKABLE QVariantList getAvailableObjects() const;
    Q_INVOKABLE QString getClipTypeColor(const QString &type) const;
    Q_INVOKABLE QVariantList getAvailableObjects(const QString &category) const;
    Q_INVOKABLE void addEffect(int clipId, const QString &effectId);
    Q_INVOKABLE void removeEffect(int clipId, int effectIndex);

    // オーディオプラグイン操作
    Q_INVOKABLE QVariantList getAvailableAudioPlugins() const;
    Q_INVOKABLE void addAudioPlugin(int clipId, const QString &pluginId);
    Q_INVOKABLE void removeAudioPlugin(int clipId, int index);
    Q_INVOKABLE QVariantList getPluginCategories() const;
    Q_INVOKABLE QVariantList getPluginsByCategory(const QString &category) const;
    Q_INVOKABLE bool isAudioClip(int clipId) const;
    Q_INVOKABLE QVariantList getWaveformPeaks(int clipId, int pixelWidth, int displayDurationFrames) const;

    // パラメータ操作用
    Q_INVOKABLE QVariantList getClipEffectStack(int clipId) const;
    Q_INVOKABLE QVariantList getEffectParameters(int clipId, int effectIndex) const;
    Q_INVOKABLE void setEffectParameter(int clipId, int effectIndex, int paramIndex, float value);
    Q_INVOKABLE void setKeyframe(int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options);
    Q_INVOKABLE void removeKeyframe(int clipId, int effectIndex, const QString &paramName, int frame);

    // シーン操作 (未実装)
    QVariantList scenes() const;
    int currentSceneId() const;
    Q_INVOKABLE void createScene(const QString &name);
    Q_INVOKABLE void removeScene(int sceneId);
    Q_INVOKABLE void switchScene(int sceneId);
    Q_INVOKABLE void updateSceneSettings(int sceneId, const QString &name, int width, int height, double fps, int totalFrames, const QString &gridMode, double gridBpm, double gridOffset, int gridInterval, int gridSubdivision, bool enableSnap,
                                         int magneticSnapRange);
    Q_INVOKABLE QVariantList getSceneClips(int sceneId) const;
    Q_INVOKABLE QVariantMap getSceneInfo(int sceneId) const;
    Q_INVOKABLE int getSceneDuration(int sceneId) const;
    Q_INVOKABLE void requestVideoFrame(int clipId, int relFrame);

    // プロジェクトI/O
    Q_INVOKABLE bool saveProject(const QString &fileUrl);
    Q_INVOKABLE bool loadProject(const QString &fileUrl);
    Q_INVOKABLE QVariantMap getProjectInfo(const QString &fileUrl) const;
    // 旧インターフェース互換
    Q_INVOKABLE bool exportMedia(const QString &fileUrl, const QString &format, int quality);
    // 新非同期インターフェース
    Q_INVOKABLE void exportVideoAsync(const QVariantMap &config);
    Q_INVOKABLE void cancelExport();
    Q_PROPERTY(bool isExporting READ isExporting NOTIFY exportFinished)
    bool isExporting() const;

    Q_INVOKABLE void selectClip(int id);
    Q_INVOKABLE void selectClipsInRange(int frameA, int frameB, int layerA, int layerB, bool additive = false);
    Q_INVOKABLE void applySelectionIds(const QVariantList &ids);

    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // コンテキストメニュー用のクリップ操作コマンド
    Q_INVOKABLE void deleteClip(int clipId);
    Q_INVOKABLE void splitClip(int clipId, int frame);
    Q_INVOKABLE void copyClip(int clipId);
    Q_INVOKABLE void cutClip(int clipId);
    Q_INVOKABLE void pasteClip(int frame, int layer);
    Q_INVOKABLE void deleteSelectedClips();
    Q_INVOKABLE void copySelectedClips();
    Q_INVOKABLE void cutSelectedClips();

    void updateActiveClipsList();

    // 再生速度の同期 (QMLからTransportServiceの変更通知を受けて呼び出す)
    Q_INVOKABLE void syncPlaybackSpeed() { m_mediaManager->syncPlaybackSpeed(); }

    // サンプリングレートの更新 (ProjectServiceの変更通知を受けて呼び出す)
    Q_INVOKABLE void updateAudioSampleRate() { m_mediaManager->updateAudioSampleRate(); }

    // 動的に計算されたタイムラインの長さ（最後のクリップの末尾フレーム）
    int timelineDuration() const { return m_engineSync->timelineDuration(); }

  signals:
    void videoFrameRequested(int clipId, int relFrame);
    void timelineScaleChanged();
    void clipStartFrameChanged();
    void clipDurationFramesChanged();
    void layerChanged();
    void isClipActiveChanged();
    void activeObjectTypeChanged(); // 選択中クリップの種別 (text, rectなど)
    void clipsChanged();            // 追加
    void scenesChanged();
    void currentSceneIdChanged();
    void clipEffectsChanged(int clipId);
    void selectedLayerChanged();
    void errorOccurred(const QString &message);
    void exportStarted(int totalFrames);
    void exportProgressChanged(int progress, int currentFrame, int totalFrames);
    void exportFinished(bool success, const QString &message);

  private:
    // Initialization Helpers
    void initializeServices();
    void setupConnections();

    // Internal Slots
    void onPlayingChanged();
    void onCurrentFrameChanged();

    void updateClipActiveState();
    double m_timelineScale = 1.0; // タイムラインの表示倍率 (1.0 = 1フレームあたり1ピクセル)

    bool m_isClipActive = false;
    int m_selectedLayer = 0;

    // 各機能を担当するサービス群
    ProjectService *m_project;
    TransportService *m_transport;
    SelectionService *m_selection;
    TimelineService *m_timeline;

    TimelineMediaManager *m_mediaManager;
    TimelineEngineSynchronizer *m_engineSync;
    TimelineExportManager *m_exportManager;

    // デバッグ用: Luaスクリプト直接実行
    Q_INVOKABLE QString debugRunLua(const QString &script);

  private:
    QPointer<QQuickItem> m_compositeView; // CompositeViewへの参照
};
} // namespace Rina::UI
