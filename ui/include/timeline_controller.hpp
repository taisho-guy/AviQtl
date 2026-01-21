#pragma once
#include <QObject>
#include <QDebug>
#include <QtMath>
#include <vector>
#include <QVariant>
#include <QTimer>

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

        Q_PROPERTY(float objectX READ objectX WRITE setObjectX NOTIFY objectXChanged)
        Q_PROPERTY(float objectY READ objectY WRITE setObjectY NOTIFY objectYChanged)
        Q_PROPERTY(QString textString READ textString WRITE setTextString NOTIFY textStringChanged)
        Q_PROPERTY(int textSize READ textSize WRITE setTextSize NOTIFY textSizeChanged)
        Q_PROPERTY(float currentTime READ currentTime WRITE setCurrentTime NOTIFY currentTimeChanged)
        Q_PROPERTY(float clipStartTime READ clipStartTime WRITE setClipStartTime NOTIFY clipStartTimeChanged)
        Q_PROPERTY(float clipDuration READ clipDuration WRITE setClipDuration NOTIFY clipDurationChanged)
        Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY layerChanged)
        Q_PROPERTY(bool isClipActive READ isClipActive NOTIFY isClipActiveChanged)
        Q_PROPERTY(QVariantList keyframeList READ keyframeList NOTIFY keyframeListChanged)
        Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
        Q_PROPERTY(QString activeObjectType READ activeObjectType NOTIFY activeObjectTypeChanged)

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

        float objectX() const;
        void setObjectX(float x);

        float objectY() const;
        void setObjectY(float y);

        QString textString() const;
        void setTextString(const QString& text);

        int textSize() const;
        void setTextSize(int size);

        float currentTime() const;
        void setCurrentTime(float time);

        float clipStartTime() const;
        void setClipStartTime(float time);

        float clipDuration() const;
        void setClipDuration(float duration);

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

    signals:
        void projectWidthChanged();
        void projectHeightChanged();
        void projectFpsChanged();
        void totalFramesChanged();
        void objectXChanged();
        void objectYChanged();
        void textStringChanged();
        void textSizeChanged();
        void currentTimeChanged();
        void clipStartTimeChanged();
        void clipDurationChanged();
        void layerChanged();
        void isClipActiveChanged();
        void keyframeListChanged();
        void isPlayingChanged();
        void activeObjectTypeChanged();

    private:
        void updateClipActiveState();
        void updateObjectX();
        void updateTimerInterval();
        float calculateInterpolatedValue(float time);

        int m_projectWidth = 1920;
        int m_projectHeight = 1080;
        double m_projectFps = 60.0;
        int m_totalFrames = 3000; // 50秒 @ 60fps

        float m_objectX = 0.0f;
        float m_objectY = 0.0f;
        QString m_textString = "Rina Text";
        int m_textSize = 64;
        float m_currentTime = 0.0f;
        float m_clipStartTime = 100.0f;
        float m_clipDuration = 200.0f;
        int m_layer = 0;
        bool m_isClipActive = false;
        std::vector<Keyframe> m_keyframesX;
        QTimer* m_playbackTimer;
        bool m_isPlaying = false;
        QString m_activeObjectType = "rect"; // デフォルトは図形
    };
}