#include "timeline_controller.hpp"
#include "../../engine/timeline/ecs.hpp"
#include <algorithm>

namespace Rina::UI {
    TimelineController::TimelineController(QObject* parent) 
        : QObject(parent)
        , m_objectX(0.0f)
        , m_currentTime(0.0f)
        , m_clipStartTime(100.0f)
        , m_clipDuration(200.0f)
        , m_layer(0)
        , m_isClipActive(false)
        , m_isPlaying(false)
    {
        m_playbackTimer = new QTimer(this);
        m_playbackTimer->setInterval(33); // ~30fps
        connect(m_playbackTimer, &QTimer::timeout, this, [this]() {
            setCurrentTime(m_currentTime + 1.0f);
        });
        updateClipActiveState();
    }

    float TimelineController::objectX() const { return m_objectX; }
    void TimelineController::setObjectX(float x) {
        if (qAbs(m_objectX - x) > 0.001f) {
            m_objectX = x;
            emit objectXChanged();
        }
    }

    float TimelineController::objectY() const { return m_objectY; }
    void TimelineController::setObjectY(float y) {
        if (qAbs(m_objectY - y) > 0.001f) {
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

    float TimelineController::currentTime() const { return m_currentTime; }
    void TimelineController::setCurrentTime(float time) {
        if (qAbs(m_currentTime - time) > 0.001f) {
            m_currentTime = time;
            emit currentTimeChanged();
            updateClipActiveState();
            updateObjectX();
        }
    }

    float TimelineController::clipStartTime() const { return m_clipStartTime; }
    void TimelineController::setClipStartTime(float time) {
        if (qAbs(m_clipStartTime - time) > 0.001f) {
            m_clipStartTime = time;
            emit clipStartTimeChanged();
            updateClipActiveState();
            // Notify ECS of the change (Entity-Component Link)
            Rina::Engine::Timeline::ECS::instance().updateClipState(1, m_layer, m_clipStartTime);
        }
    }

    float TimelineController::clipDuration() const { return m_clipDuration; }
    void TimelineController::setClipDuration(float duration) {
        if (qAbs(m_clipDuration - duration) > 0.001f) {
            m_clipDuration = duration;
            emit clipDurationChanged();
            updateClipActiveState();
        }
    }

    int TimelineController::layer() const { return m_layer; }
    void TimelineController::setLayer(int layer) {
        if (m_layer != layer) {
            m_layer = layer;
            emit layerChanged();
            // Notify ECS of the change
            Rina::Engine::Timeline::ECS::instance().updateClipState(1, m_layer, m_clipStartTime);
        }
    }

    bool TimelineController::isClipActive() const { return m_isClipActive; }

    void TimelineController::updateClipActiveState() {
        // シンプルな矩形判定: Start <= Current < Start + Duration
        bool active = (m_currentTime >= m_clipStartTime) && (m_currentTime < m_clipStartTime + m_clipDuration);
        if (m_isClipActive != active) {
            m_isClipActive = active;
            emit isClipActiveChanged();
        }
    }

    void TimelineController::addKeyframe(int frame, float value) {
        // Convert absolute frame to relative frame (Layer as Object)
        int relFrame = frame - qRound(m_clipStartTime);

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
        float newVal = calculateInterpolatedValue(m_currentTime);
        // Directly set member to avoid loop if we used setter, but emit signal
        if (qAbs(m_objectX - newVal) > 0.001f) {
            m_objectX = newVal;
            emit objectXChanged();
        }
    }

    float TimelineController::calculateInterpolatedValue(float time) {
        if (m_keyframesX.empty()) return m_objectX;
        
        float relTime = time - m_clipStartTime;

        if (relTime <= m_keyframesX.front().frame) return m_keyframesX.front().value;
        if (relTime >= m_keyframesX.back().frame) return m_keyframesX.back().value;

        for (size_t i = 0; i < m_keyframesX.size() - 1; ++i) {
            const auto& k1 = m_keyframesX[i];
            const auto& k2 = m_keyframesX[i+1];
            if (relTime >= k1.frame && relTime < k2.frame) {
                float t = (relTime - k1.frame) / (float)(k2.frame - k1.frame);
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

    void TimelineController::log(const QString& msg) {
        qDebug() << "[TimelineBridge] " << msg;
    }
}