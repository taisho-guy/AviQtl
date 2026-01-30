#pragma once
#include <QObject>
#include <QDebug>
#include <QtMath>
#include <vector>
#include <QVariant>
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <QAbstractListModel>
#include "effect_model.hpp" // 念のため維持
#include "timeline_types.hpp"
#include "project_service.hpp"
#include "transport_service.hpp"
#include "selection_service.hpp"

class QUndoStack;

namespace Rina::UI {
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
        
        // === Services (Sub-Controllers) ===
        Q_PROPERTY(Rina::UI::ProjectService* project READ project CONSTANT)
        Q_PROPERTY(Rina::UI::TransportService* transport READ transport CONSTANT)
        Q_PROPERTY(Rina::UI::SelectionService* selection READ selection CONSTANT)

        // === Legacy / Facade Properties ===
        Q_PROPERTY(double timelineScale READ timelineScale WRITE setTimelineScale NOTIFY timelineScaleChanged)
        Q_PROPERTY(int clipStartFrame READ clipStartFrame WRITE setClipStartFrame NOTIFY clipStartFrameChanged)
        Q_PROPERTY(int clipDurationFrames READ clipDurationFrames WRITE setClipDurationFrames NOTIFY clipDurationFramesChanged)
        Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY layerChanged)
        Q_PROPERTY(bool isClipActive READ isClipActive NOTIFY isClipActiveChanged)
        Q_PROPERTY(QVariantList keyframeList READ keyframeList NOTIFY keyframeListChanged)
        Q_PROPERTY(QString activeObjectType READ activeObjectType NOTIFY activeObjectTypeChanged)
        Q_PROPERTY(QVariantList clips READ clips NOTIFY clipsChanged)
        Q_PROPERTY(Rina::UI::ClipModel* clipModel READ clipModel CONSTANT)

    public:
        explicit TimelineController(QObject* parent = nullptr);

        // Service Accessors
        ProjectService* project() const { return m_project; }
        TransportService* transport() const { return m_transport; }
        SelectionService* selection() const { return m_selection; }

        double timelineScale() const;
        void setTimelineScale(double scale);

        // Generic Property Operations
        Q_INVOKABLE void setClipProperty(const QString& name, const QVariant& value);
        Q_INVOKABLE QVariant getClipProperty(const QString& name) const;

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

        Q_INVOKABLE void selectClip(int id);

        Q_INVOKABLE void undo();
        Q_INVOKABLE void redo();

        // Clip manipulation commands for context menu
        Q_INVOKABLE void deleteClip(int clipId);
        Q_INVOKABLE void splitClip(int clipId, int frame);
        Q_INVOKABLE void copyClip(int clipId);
        Q_INVOKABLE void cutClip(int clipId);
        Q_INVOKABLE void pasteClip(int frame, int layer);

        // Internal methods called by Commands
        void updateClipInternal(int id, int layer, int startFrame, int duration);
        void updateClipEffectParamInternal(int clipId, int effectIndex, const QString& paramName, const QVariant& value);
        void createObjectInternal(const QString& type, int startFrame, int layer);
        void addEffectInternal(int clipId, const EffectData& effectData);
        void removeEffectInternal(int clipId, int effectIndex);
        EffectData createEffectData(const QString& id);
        ClipData deepCopyClip(const ClipData& source);
        void updateActiveClipsList();

    signals:
        void timelineScaleChanged();
        void clipStartFrameChanged();
        void clipDurationFramesChanged();
        void layerChanged();
        void isClipActiveChanged();
        void keyframeListChanged();
        void activeObjectTypeChanged();
        void clipsChanged(); // 追加
        void clipEffectsChanged(int clipId);

    private:
        void updateClipActiveState();
        void updateObjectX();
    float calculateInterpolatedValue(int frame);

        QList<ClipData> m_clips;
        ClipModel* m_clipModel;
        int m_nextClipId = 1;

        QUndoStack* m_undoStack;
        std::unique_ptr<ClipData> m_clipboard;
        double m_timelineScale = 1.0; // 1 frame = 1 pixel (default)

        int m_clipStartFrame = 100;
        int m_clipDurationFrames = 200;
        int m_layer = 0;
        bool m_isClipActive = false;
        std::vector<Keyframe> m_keyframesX;
        QString m_activeObjectType = "rect"; // デフォルトは図形

        // Prototype Definitions (Type -> Default Properties)
        QMap<QString, QVariantMap> m_prototypes;

        // Services
        ProjectService* m_project;
        TransportService* m_transport;
        SelectionService* m_selection;
    };
}