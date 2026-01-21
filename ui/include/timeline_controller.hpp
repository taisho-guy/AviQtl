#pragma once
#include <QObject>
#include <QDebug>
#include <QtMath>
#include <vector>
#include <QVariant>
#include <QTimer>
#include <QColor>

namespace Rina::UI {
    struct Keyframe {
        int frame;
        float value;
        int interpolationType; // 0: Linear
    };

    class TimelineController : public QObject {
        Q_OBJECT
        // プロジェクト設定
        Q_PROPERTY(int projectWidth READ projectWidth WRITE setProjectWidth NOTIFY projectWidthChanged)
        Q_PROPERTY(int projectHeight READ projectHeight WRITE setProjectHeight NOTIFY projectHeightChanged)
        Q_PROPERTY(double projectFps READ projectFps WRITE setProjectFps NOTIFY projectFpsChanged)
        Q_PROPERTY(int totalFrames READ totalFrames WRITE setTotalFrames NOTIFY totalFramesChanged)
        Q_PROPERTY(double timelineScale READ timelineScale WRITE setTimelineScale NOTIFY timelineScaleChanged)

        Q_PROPERTY(int objectX READ objectX WRITE setObjectX NOTIFY objectXChanged)
        Q_PROPERTY(int objectY READ objectY WRITE setObjectY NOTIFY objectYChanged)
        Q_PROPERTY(int objectWidth READ objectWidth WRITE setObjectWidth NOTIFY objectWidthChanged)
        Q_PROPERTY(int objectHeight READ objectHeight WRITE setObjectHeight NOTIFY objectHeightChanged)
        Q_PROPERTY(QString textString READ textString WRITE setTextString NOTIFY textStringChanged)
        Q_PROPERTY(int textSize READ textSize WRITE setTextSize NOTIFY textSizeChanged)
        Q_PROPERTY(double objectRotation READ objectRotation WRITE setObjectRotation NOTIFY objectRotationChanged)
        Q_PROPERTY(double objectOpacity READ objectOpacity WRITE setObjectOpacity NOTIFY objectOpacityChanged)
        Q_PROPERTY(QColor objectColor READ objectColor WRITE setObjectColor NOTIFY objectColorChanged)
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
        Q_PROPERTY(QVariantList activeClips READ activeClips NOTIFY activeClipsChanged)
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

        int objectX() const;
        void setObjectX(int x);

        int objectY() const;
        void setObjectY(int y);

        int objectWidth() const;
        void setObjectWidth(int w);

        int objectHeight() const;
        void setObjectHeight(int h);

        QString textString() const;
        void setTextString(const QString& text);

        int textSize() const;
        void setTextSize(int size);

        double objectRotation() const;
        void setObjectRotation(double rotation);

        double objectOpacity() const;
        void setObjectOpacity(double opacity);

        QColor objectColor() const;
        void setObjectColor(const QColor& color);

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
        QVariantList activeClips() const;

        int selectedClipId() const;
        Q_INVOKABLE void selectClip(int id);

    signals:
        void projectWidthChanged();
        void projectHeightChanged();
        void projectFpsChanged();
        void totalFramesChanged();
        void timelineScaleChanged();
        void objectXChanged();
        void objectYChanged();
        void objectWidthChanged();
        void objectHeightChanged();
        void textStringChanged();
        void textSizeChanged();
        void objectRotationChanged();
        void objectOpacityChanged();
        void objectColorChanged();
        void currentFrameChanged();
        void clipStartFrameChanged();
        void clipDurationFramesChanged();
        void layerChanged();
        void isClipActiveChanged();
        void keyframeListChanged();
        void isPlayingChanged();
        void activeObjectTypeChanged();
        void clipsChanged(); // 追加
        void activeClipsChanged();
        void selectedClipIdChanged();

    private:
    void onPlaybackStep();
        void updateClipActiveState();
        void updateObjectX();
        void updateTimerInterval();
    float calculateInterpolatedValue(int frame);

        struct ClipData {
            int id;
            QString type;
            int startFrame;
            int durationFrames;
            int layer;
            // 個別プロパティ
            int x = 0;
            int y = 0;
            int width = 100;
            int height = 100;
            QString text = "Text";
            int textSize = 64;
            double rotation = 0.0;
            double opacity = 1.0;
            QString color = "#ffffff";
            // 簡易化のためプロパティは最小限
        };
        QList<ClipData> m_clips;
        int m_nextClipId = 1;

        int m_projectWidth = 1920;
        int m_projectHeight = 1080;
        double m_projectFps = 60.0;
        int m_totalFrames = 3600;
        double m_timelineScale = 1.0; // 1 frame = 1 pixel (default)
        int m_selectedClipId = -1;

        int m_objectX = 0;
        int m_objectY = 0;
        int m_objectWidth = 100;
        int m_objectHeight = 100;
        QString m_textString = "Rina Text";
        int m_textSize = 64;
        double m_objectRotation = 0.0;
        double m_objectOpacity = 1.0;
        QColor m_objectColor = QColor(Qt::white);
        int m_currentFrame = 0;
        int m_clipStartFrame = 100;
        int m_clipDurationFrames = 200;
        int m_layer = 0;
        bool m_isClipActive = false;
        std::vector<Keyframe> m_keyframesX;
        QTimer* m_playbackTimer;
        bool m_isPlaying = false;
        QString m_activeObjectType = "rect"; // デフォルトは図形
    };
}