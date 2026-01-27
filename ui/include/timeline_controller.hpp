#pragma once
#include <QObject>
#include <QDebug>
#include <QtMath>
#include <vector>
#include <QVariant>
#include <QTimer>
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUndoStack>
#include <QAbstractListModel>
#include "effect_model.hpp" // 念のため維持

namespace Rina::UI {
    // Value object for serialization and undo/redo
    struct EffectData {
        QString id;
        QString name;
        QString qmlSource;
        bool enabled = true;
        QVariantMap params;
    };

    struct Keyframe {
        int frame;
        float value;
        int interpolationType; // 0: Linear
    };

    // Moved out of TimelineController for ClipModel access
    struct ClipData {
        int id;
        QString type;
        int startFrame;
        int durationFrames;
        int layer;
        
        // エフェクトスタック
        QList<EffectModel*> effects;
    };

    class ClipModel : public QAbstractListModel {
        Q_OBJECT
    public:
        enum Roles {
            IdRole = Qt::UserRole + 1,
            TypeRole,
            StartFrameRole,
            DurationRole,
            LayerRole,
            ParamsRole, // Flattened params
            EffectsRole // QList<QObject*> for effect models
        };

        explicit ClipModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

        int rowCount(const QModelIndex& parent = QModelIndex()) const override {
            return m_activeClips.size();
        }

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;

        void updateClips(const QList<ClipData*>& newClips);

    private:
        QList<ClipData*> m_activeClips;
    };

    class TimelineController : public QObject {
        Q_OBJECT
        // プロジェクト設定
        Q_PROPERTY(int projectWidth READ projectWidth WRITE setProjectWidth NOTIFY projectWidthChanged)
        Q_PROPERTY(int projectHeight READ projectHeight WRITE setProjectHeight NOTIFY projectHeightChanged)
        Q_PROPERTY(double projectFps READ projectFps WRITE setProjectFps NOTIFY projectFpsChanged)
        Q_PROPERTY(int totalFrames READ totalFrames WRITE setTotalFrames NOTIFY totalFramesChanged)
        Q_PROPERTY(double timelineScale READ timelineScale WRITE setTimelineScale NOTIFY timelineScaleChanged)

        // Generic property access for the selected clip
        Q_PROPERTY(QVariantMap selectedClipData READ selectedClipData NOTIFY selectedClipDataChanged)

        // フレーム数ベースに変更
        Q_PROPERTY(int currentFrame READ currentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
        Q_PROPERTY(int clipStartFrame READ clipStartFrame WRITE setClipStartFrame NOTIFY clipStartFrameChanged)
        Q_PROPERTY(int clipDurationFrames READ clipDurationFrames WRITE setClipDurationFrames NOTIFY clipDurationFramesChanged)
        Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY layerChanged)
        Q_PROPERTY(bool isClipActive READ isClipActive NOTIFY isClipActiveChanged)
        Q_PROPERTY(QVariantList keyframeList READ keyframeList NOTIFY keyframeListChanged)
        Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
        Q_PROPERTY(QString activeObjectType READ activeObjectType NOTIFY activeObjectTypeChanged)
        Q_PROPERTY(QVariantList clips READ clips NOTIFY clipsChanged)
        Q_PROPERTY(Rina::UI::ClipModel* clipModel READ clipModel CONSTANT)
        Q_PROPERTY(int selectedClipId READ selectedClipId NOTIFY selectedClipIdChanged)

    public:
        explicit TimelineController(QObject* parent = nullptr);

        // Getter / Setter
        int projectWidth() const;
        void setProjectWidth(int w);
        
        int projectHeight() const;
        void setProjectHeight(int h);

        double projectFps() const;
        void setProjectFps(double fps);

        int totalFrames() const;
        void setTotalFrames(int frames);

        double timelineScale() const;
        void setTimelineScale(double scale);

        // Generic Property Operations
        Q_INVOKABLE void setClipProperty(const QString& name, const QVariant& value);
        Q_INVOKABLE QVariant getClipProperty(const QString& name) const;
        QVariantMap selectedClipData() const;

        int currentFrame() const;
        void setCurrentFrame(int frame);

        int clipStartFrame() const;
        void setClipStartFrame(int frame);

        int clipDurationFrames() const;
        void setClipDurationFrames(int frames);

        int layer() const;
        void setLayer(int layer);

        bool isClipActive() const;

        Q_INVOKABLE void addKeyframe(int frame, float value);
        QVariantList keyframeList() const;

        Q_INVOKABLE void createObject(const QString& type, int startFrame, int layer);
        QString activeObjectType() const;

        Q_INVOKABLE void togglePlay();
        bool isPlaying() const;

        Q_INVOKABLE void log(const QString& msg);
        QVariantList clips() const;
        ClipModel* clipModel() const { return m_clipModel; }

        // クリップの配置・長さを更新（ID指定）
        Q_INVOKABLE void updateClip(int id, int layer, int startFrame, int duration);

        // エフェクト操作
        Q_INVOKABLE QList<QObject*> getClipEffectsModel(int clipId) const;
        Q_INVOKABLE void updateClipEffectParam(int clipId, int effectIndex, const QString& paramName, const QVariant& value);
        
        // エフェクト追加・削除
        Q_INVOKABLE QVariantList getAvailableEffects() const;
        Q_INVOKABLE QVariantList getAvailableObjects() const;
        Q_INVOKABLE void addEffect(int clipId, const QString& effectId);
        Q_INVOKABLE void removeEffect(int clipId, int effectIndex);

        // プロジェクト保存・読み込み
        Q_INVOKABLE bool saveProject(const QString& fileUrl);
        Q_INVOKABLE bool loadProject(const QString& fileUrl);

        int selectedClipId() const;
        Q_INVOKABLE void selectClip(int id);

        Q_INVOKABLE void undo();
        Q_INVOKABLE void redo();

        // Internal methods called by Commands
        void updateClipInternal(int id, int layer, int startFrame, int duration);
        void updateClipEffectParamInternal(int clipId, int effectIndex, const QString& paramName, const QVariant& value);
        void createObjectInternal(const QString& type, int startFrame, int layer);
        void addEffectInternal(int clipId, const EffectData& effectData);
        void removeEffectInternal(int clipId, int effectIndex);
        EffectData createEffectData(const QString& id);
        void updateActiveClipsList();

    signals:
        void projectWidthChanged();
        void projectHeightChanged();
        void projectFpsChanged();
        void totalFramesChanged();
        void timelineScaleChanged();
        void selectedClipDataChanged();
        void currentFrameChanged();
        void clipStartFrameChanged();
        void clipDurationFramesChanged();
        void layerChanged();
        void isClipActiveChanged();
        void keyframeListChanged();
        void isPlayingChanged();
        void activeObjectTypeChanged();
        void clipsChanged(); // 追加
        void selectedClipIdChanged();

    private:
    void onPlaybackStep();
        void updateClipActiveState();
        void updateObjectX();
        void updateTimerInterval();
    float calculateInterpolatedValue(int frame);

        QList<ClipData> m_clips;
        ClipModel* m_clipModel;
        int m_nextClipId = 1;

        QUndoStack* m_undoStack;
        int m_projectWidth = 1920;
        int m_projectHeight = 1080;
        double m_projectFps = 60.0;
        int m_totalFrames = 3600;
        double m_timelineScale = 1.0; // 1 frame = 1 pixel (default)
        int m_selectedClipId = -1;
        QVariantMap m_selectedClipCache; // Cache for UI binding

        int m_currentFrame = 0;
        int m_clipStartFrame = 100;
        int m_clipDurationFrames = 200;
        int m_layer = 0;
        bool m_isClipActive = false;
        std::vector<Keyframe> m_keyframesX;
        QTimer* m_playbackTimer;
        bool m_isPlaying = false;
        QString m_activeObjectType = "rect"; // デフォルトは図形

        // Prototype Definitions (Type -> Default Properties)
        QMap<QString, QVariantMap> m_prototypes;
    };
}