#include "timeline_controller.hpp"
#include "../../engine/timeline/ecs.hpp"
#include <algorithm>

namespace Rina::UI {
    TimelineController::TimelineController(QObject* parent) 
        : QObject(parent)
        , m_objectX(0)
        , m_currentFrame(0)
        , m_clipStartFrame(100)
        , m_clipDurationFrames(200)
        , m_layer(0)
        , m_isClipActive(false)
        , m_isPlaying(false)
        , m_activeObjectType("rect") // デフォルトは図形
    {
        m_playbackTimer = new QTimer(this);
        m_playbackTimer->setTimerType(Qt::PreciseTimer);
        updateTimerInterval(); // 初期FPSで設定
        connect(m_playbackTimer, &QTimer::timeout, this, &TimelineController::onPlaybackStep);
        updateClipActiveState();
    }

    int TimelineController::projectWidth() const { return m_projectWidth; }
    void TimelineController::setProjectWidth(int w) {
        if (m_projectWidth != w) {
            m_projectWidth = w;
            emit projectWidthChanged();
        }
    }
    
    void TimelineController::onPlaybackStep() {
        int nextFrame = m_currentFrame + 1;
        // ループ再生
        if (nextFrame > m_totalFrames) {
            nextFrame = 0;
        }
        setCurrentFrame(nextFrame);
    }

    int TimelineController::projectHeight() const { return m_projectHeight; }
    void TimelineController::setProjectHeight(int h) {
        if (m_projectHeight != h) {
            m_projectHeight = h;
            emit projectHeightChanged();
        }
    }

    double TimelineController::projectFps() const { return m_projectFps; }
    void TimelineController::setProjectFps(double fps) {
        if (!qFuzzyCompare(m_projectFps, fps)) {
            m_projectFps = fps;
            updateTimerInterval(); // FPS変更時にタイマー間隔を更新
            emit projectFpsChanged();
        }
    }

    int TimelineController::totalFrames() const { return m_totalFrames; }
    void TimelineController::setTotalFrames(int frames) {
        if (m_totalFrames != frames) {
            m_totalFrames = frames;
            emit totalFramesChanged();
        }
    }

    double TimelineController::timelineScale() const { return m_timelineScale; }
    void TimelineController::setTimelineScale(double scale) {
        if (qAbs(m_timelineScale - scale) > 0.001) {
            m_timelineScale = scale;
            emit timelineScaleChanged();
        }
    }

    int TimelineController::objectX() const { return m_objectX; }
    void TimelineController::setObjectX(int x) {
        if (m_objectX != x) {
            m_objectX = x;
            emit objectXChanged();
        }
    }

    int TimelineController::objectY() const { return m_objectY; }
    void TimelineController::setObjectY(int y) {
        if (m_objectY != y) {
            m_objectY = y;
            emit objectYChanged();
        }
    }

    QString TimelineController::textString() const { return m_textString; }
    void TimelineController::setTextString(const QString& text) {
        if (m_textString != text) {
            m_textString = text;
            emit textStringChanged();
        }
    }

    int TimelineController::textSize() const { return m_textSize; }
    void TimelineController::setTextSize(int size) {
        if (m_textSize != size) {
            m_textSize = size;
            emit textSizeChanged();
        }
    }

    int TimelineController::currentFrame() const { return m_currentFrame; }
    void TimelineController::setCurrentFrame(int frame) {
        if (m_currentFrame != frame) {
            m_currentFrame = frame;
            emit currentFrameChanged();
            updateClipActiveState();
            updateObjectX();
        }
    }

    int TimelineController::clipStartFrame() const { return m_clipStartFrame; }
    void TimelineController::setClipStartFrame(int frame) {
        if (m_clipStartFrame != frame) {
            m_clipStartFrame = frame;
            emit clipStartFrameChanged();
            updateClipActiveState();
            // Notify ECS of the change (Entity-Component Link)
            Rina::Engine::Timeline::ECS::instance().updateClipState(1, m_layer, m_clipStartFrame);
        }
    }

    int TimelineController::clipDurationFrames() const { return m_clipDurationFrames; }
    void TimelineController::setClipDurationFrames(int frames) {
        if (m_clipDurationFrames != frames) {
            m_clipDurationFrames = frames;
            emit clipDurationFramesChanged();
            updateClipActiveState();
        }
    }

    int TimelineController::layer() const { return m_layer; }
    void TimelineController::setLayer(int layer) {
        if (m_layer != layer) {
            m_layer = layer;
            emit layerChanged();
            // Notify ECS of the change
            Rina::Engine::Timeline::ECS::instance().updateClipState(1, m_layer, m_clipStartFrame);
        }
    }

    bool TimelineController::isClipActive() const { return m_isClipActive; }

    void TimelineController::updateClipActiveState() {
        // シンプルな矩形判定: Start <= Current < Start + Duration
        bool active = (m_currentFrame >= m_clipStartFrame) && (m_currentFrame < m_clipStartFrame + m_clipDurationFrames);
        if (m_isClipActive != active) {
            m_isClipActive = active;
            emit isClipActiveChanged();
        }

        if (m_isClipActive) {
            Rina::Engine::Timeline::ECS::instance().updateClipState(1, m_layer, m_currentFrame - m_clipStartFrame);
        }
    }

    void TimelineController::addKeyframe(int frame, float value) {
        int relFrame = frame - m_clipStartFrame;

        // Check if keyframe exists at this frame
        auto it = std::find_if(m_keyframesX.begin(), m_keyframesX.end(), [relFrame](const Keyframe& k){
            return k.frame == relFrame;
        });

        if (it != m_keyframesX.end()) {
            it->value = value;
        } else {
            m_keyframesX.push_back({relFrame, value, 0});
            // Keep sorted by frame
            std::sort(m_keyframesX.begin(), m_keyframesX.end(), [](const Keyframe& a, const Keyframe& b){
                return a.frame < b.frame;
            });
        }
        
        emit keyframeListChanged();
        updateObjectX();
    }

    QVariantList TimelineController::keyframeList() const {
        QVariantList list;
        for (const auto& k : m_keyframesX) {
            QVariantMap map;
            map["frame"] = k.frame;
            map["value"] = k.value;
            list.append(map);
        }
        return list;
    }

    void TimelineController::updateObjectX() {
        if (m_keyframesX.empty()) return;
        float newVal = calculateInterpolatedValue(m_currentFrame);
        // Directly set member to avoid loop if we used setter, but emit signal
        int newX = qRound(newVal);
        if (m_objectX != newX) {
            m_objectX = newX;
            emit objectXChanged();
        }
    }

    void TimelineController::updateTimerInterval() {
        if (m_playbackTimer && m_projectFps > 0) {
            // 1000ms / FPS = 1フレームあたりのミリ秒数
            int interval = static_cast<int>(1000.0 / m_projectFps);
            m_playbackTimer->setInterval(interval);
        }
    }

    float TimelineController::calculateInterpolatedValue(int frame) {
        if (m_keyframesX.empty()) return m_objectX;
        
        int relFrame = frame - m_clipStartFrame;

        if (relFrame <= m_keyframesX.front().frame) return m_keyframesX.front().value;
        if (relFrame >= m_keyframesX.back().frame) return m_keyframesX.back().value;

        for (size_t i = 0; i < m_keyframesX.size() - 1; ++i) {
            const auto& k1 = m_keyframesX[i];
            const auto& k2 = m_keyframesX[i+1];
            if (relFrame >= k1.frame && relFrame < k2.frame) {
                float t = (relFrame - k1.frame) / (float)(k2.frame - k1.frame);
                return k1.value + (k2.value - k1.value) * t;
            }
        }
        return m_keyframesX.back().value;
    }

    bool TimelineController::isPlaying() const { return m_isPlaying; }

    void TimelineController::togglePlay() {
        m_isPlaying = !m_isPlaying;
        if (m_isPlaying) {
            m_playbackTimer->start();
        } else {
            m_playbackTimer->stop();
        }
        emit isPlayingChanged();
    }

    QString TimelineController::activeObjectType() const {
        return m_activeObjectType;
    }

    void TimelineController::createObject(const QString& type, int startFrame, int layer) {
        qDebug() << "Creating Object:" << type << "at Frame:" << startFrame << "Layer:" << layer;
        
        // 新規クリップ作成
        ClipData newClip;
        newClip.id = m_nextClipId++;
        newClip.type = type;
        newClip.startFrame = startFrame;
        newClip.durationFrames = 100; // デフォルト100フレーム
        newClip.layer = layer;
        
        m_clips.append(newClip);
        emit clipsChanged();

        // 選択状態にする（既存の単一変数を「選択中のクリップ」として使う）
        m_clipStartFrame = newClip.startFrame;
        m_clipDurationFrames = newClip.durationFrames;
        m_layer = newClip.layer;
        m_activeObjectType = type;
        
        emit clipStartFrameChanged();
        emit clipDurationFramesChanged();
        emit layerChanged();
        emit activeObjectTypeChanged();
    }

    QVariantList TimelineController::clips() const {
        QVariantList list;
        for (const auto& clip : m_clips) {
            QVariantMap map;
            map["id"] = clip.id;
            map["type"] = clip.type;
            map["startFrame"] = clip.startFrame;
            map["durationFrames"] = clip.durationFrames;
            map["layer"] = clip.layer;
            list.append(map);
        }
        return list;
    }

    void TimelineController::log(const QString& msg) {
        qDebug() << "[TimelineBridge] " << msg;
    }
}