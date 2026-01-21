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
        , m_selectedClipId(-1)
        , m_isClipActive(false)
        , m_isPlaying(false)
        , m_activeObjectType("rect") // デフォルトは図形
        , m_objectRotation(0.0)
        , m_objectOpacity(1.0)
        , m_objectColor(Qt::white)
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
            
            // 選択中のクリップがあればデータを更新
            if (m_selectedClipId != -1) {
                for (auto& clip : m_clips) {
                    if (clip.id == m_selectedClipId) {
                        clip.x = x;
                        emit activeClipsChanged(); // プレビュー更新
                        break;
                    }
                }
            }
        }
    }

    int TimelineController::objectY() const { return m_objectY; }
    void TimelineController::setObjectY(int y) {
        if (m_objectY != y) {
            m_objectY = y;
            emit objectYChanged();
            
            if (m_selectedClipId != -1) {
                for (auto& clip : m_clips) {
                    if (clip.id == m_selectedClipId) {
                        clip.y = y;
                        emit activeClipsChanged();
                        break;
                    }
                }
            }
        }
    }

    int TimelineController::objectWidth() const { return m_objectWidth; }
    void TimelineController::setObjectWidth(int w) {
        if (m_objectWidth != w) {
            m_objectWidth = w;
            emit objectWidthChanged();
            
            if (m_selectedClipId != -1) {
                for (auto& clip : m_clips) {
                    if (clip.id == m_selectedClipId) {
                        clip.width = w;
                        emit activeClipsChanged();
                        break;
                    }
                }
            }
        }
    }

    int TimelineController::objectHeight() const { return m_objectHeight; }
    void TimelineController::setObjectHeight(int h) {
        if (m_objectHeight != h) {
            m_objectHeight = h;
            emit objectHeightChanged();
            
            if (m_selectedClipId != -1) {
                for (auto& clip : m_clips) {
                    if (clip.id == m_selectedClipId) {
                        clip.height = h;
                        emit activeClipsChanged();
                        break;
                    }
                }
            }
        }
    }

    QString TimelineController::textString() const { return m_textString; }
    void TimelineController::setTextString(const QString& text) {
        if (m_textString != text) {
            m_textString = text;
            emit textStringChanged();
            
            if (m_selectedClipId != -1) {
                for (auto& clip : m_clips) {
                    if (clip.id == m_selectedClipId) {
                        clip.text = text;
                        emit activeClipsChanged();
                        break;
                    }
                }
            }
        }
    }

    int TimelineController::textSize() const { return m_textSize; }
    void TimelineController::setTextSize(int size) {
        if (m_textSize != size) {
            m_textSize = size;
            emit textSizeChanged();
            
            if (m_selectedClipId != -1) {
                for (auto& clip : m_clips) {
                    if (clip.id == m_selectedClipId) {
                        clip.textSize = size;
                        emit activeClipsChanged();
                        break;
                    }
                }
            }
        }
    }

    double TimelineController::objectRotation() const { return m_objectRotation; }
    void TimelineController::setObjectRotation(double rotation) {
        if (!qFuzzyCompare(m_objectRotation, rotation)) {
            m_objectRotation = rotation;
            emit objectRotationChanged();
            if (m_selectedClipId != -1) {
                for (auto& clip : m_clips) {
                    if (clip.id == m_selectedClipId) {
                        clip.rotation = rotation;
                        emit activeClipsChanged();
                        break;
                    }
                }
            }
        }
    }

    double TimelineController::objectOpacity() const { return m_objectOpacity; }
    void TimelineController::setObjectOpacity(double opacity) {
        if (!qFuzzyCompare(m_objectOpacity, opacity)) {
            m_objectOpacity = opacity;
            emit objectOpacityChanged();
            if (m_selectedClipId != -1) {
                for (auto& clip : m_clips) {
                    if (clip.id == m_selectedClipId) {
                        clip.opacity = opacity;
                        emit activeClipsChanged();
                        break;
                    }
                }
            }
        }
    }

    QColor TimelineController::objectColor() const { return m_objectColor; }
    void TimelineController::setObjectColor(const QColor& color) {
        if (m_objectColor != color) {
            m_objectColor = color;
            emit objectColorChanged();
            if (m_selectedClipId != -1) {
                for (auto& clip : m_clips) {
                    if (clip.id == m_selectedClipId) {
                        clip.color = color.name();
                        emit activeClipsChanged();
                        break;
                    }
                }
            }
        }
    }

    int TimelineController::currentFrame() const { return m_currentFrame; }
    void TimelineController::setCurrentFrame(int frame) {
        if (m_currentFrame != frame) {
            m_currentFrame = frame;
            emit currentFrameChanged();
            updateClipActiveState();
            emit activeClipsChanged();
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
        newClip.x = 0;
        newClip.y = 0;
        newClip.width = 100;
        newClip.height = 100;
        newClip.text = (type == "text") ? "Text" : "";
        newClip.textSize = 64;
        newClip.rotation = 0.0;
        newClip.opacity = 1.0;
        newClip.color = "#ffffff";
        
        m_clips.append(newClip);
        emit clipsChanged();

        // 選択状態にする（既存の単一変数を「選択中のクリップ」として使う）
        m_clipStartFrame = newClip.startFrame;
        m_clipDurationFrames = newClip.durationFrames;
        m_layer = newClip.layer;
        m_activeObjectType = type;
        m_selectedClipId = newClip.id;
        m_objectX = newClip.x;
        m_objectY = newClip.y;
        m_objectWidth = newClip.width;
        m_objectHeight = newClip.height;
        m_textString = newClip.text;
        m_textSize = newClip.textSize;
        m_objectRotation = newClip.rotation;
        m_objectOpacity = newClip.opacity;
        m_objectColor = QColor(newClip.color);
        
        emit clipStartFrameChanged();
        emit clipDurationFramesChanged();
        emit layerChanged();
        emit activeObjectTypeChanged();
        emit activeClipsChanged();
        emit selectedClipIdChanged();
        emit objectXChanged();
        emit objectYChanged();
        emit objectWidthChanged();
        emit objectHeightChanged();
        emit textStringChanged();
        emit textSizeChanged();
        emit objectRotationChanged();
        emit objectOpacityChanged();
        emit objectColorChanged();
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
            map["x"] = clip.x;
            map["y"] = clip.y;
            map["text"] = clip.text;
            
            list.append(map);
        }
        return list;
    }

    QVariantList TimelineController::activeClips() const {
        QVariantList list;
        
        // 現在フレームにあるクリップを抽出
        QList<ClipData> active;
        for (const auto& clip : m_clips) {
            if (m_currentFrame >= clip.startFrame && 
                m_currentFrame < clip.startFrame + clip.durationFrames) {
                active.append(clip);
            }
        }
        
        // レイヤー順（昇順）にソート
        std::sort(active.begin(), active.end(), [](const ClipData& a, const ClipData& b) {
            return a.layer < b.layer;
        });

        for (const auto& clip : active) {
            QVariantMap map;
            map["id"] = clip.id;
            map["type"] = clip.type;
            map["startFrame"] = clip.startFrame;
            map["durationFrames"] = clip.durationFrames;
            map["layer"] = clip.layer;
            map["x"] = clip.x;
            map["y"] = clip.y;
            map["width"] = clip.width;
            map["height"] = clip.height;
            map["text"] = clip.text;
            map["textSize"] = clip.textSize;
            map["rotation"] = clip.rotation;
            map["opacity"] = clip.opacity;
            map["color"] = clip.color;
            list.append(map);
        }
        return list;
    }

    void TimelineController::log(const QString& msg) {
        qDebug() << "[TimelineBridge] " << msg;
    }

    int TimelineController::selectedClipId() const {
        return m_selectedClipId;
    }

    void TimelineController::selectClip(int id) {
        if (m_selectedClipId == id) return;
        
        m_selectedClipId = id;
        emit selectedClipIdChanged();

        // 選択されたクリップの情報をプロパティにロード
        for (const auto& clip : m_clips) {
            if (clip.id == id) {
                if (m_objectX != clip.x) { m_objectX = clip.x; emit objectXChanged(); }
                if (m_objectY != clip.y) { m_objectY = clip.y; emit objectYChanged(); }
                if (m_objectWidth != clip.width) { m_objectWidth = clip.width; emit objectWidthChanged(); }
                if (m_objectHeight != clip.height) { m_objectHeight = clip.height; emit objectHeightChanged(); }
                if (m_textString != clip.text) { m_textString = clip.text; emit textStringChanged(); }
                if (m_textSize != clip.textSize) { m_textSize = clip.textSize; emit textSizeChanged(); }
                if (!qFuzzyCompare(m_objectRotation, clip.rotation)) { m_objectRotation = clip.rotation; emit objectRotationChanged(); }
                if (!qFuzzyCompare(m_objectOpacity, clip.opacity)) { m_objectOpacity = clip.opacity; emit objectOpacityChanged(); }
                if (m_objectColor.name() != clip.color) { m_objectColor = QColor(clip.color); emit objectColorChanged(); }
                
                if (m_activeObjectType != clip.type) { m_activeObjectType = clip.type; emit activeObjectTypeChanged(); }
                if (m_layer != clip.layer) { m_layer = clip.layer; emit layerChanged(); }
                if (m_clipStartFrame != clip.startFrame) { m_clipStartFrame = clip.startFrame; emit clipStartFrameChanged(); }
                if (m_clipDurationFrames != clip.durationFrames) { m_clipDurationFrames = clip.durationFrames; emit clipDurationFramesChanged(); }
                
                // 選択変更時にプレビュー更新（選択枠表示などのため）
                emit activeClipsChanged();
                return;
            }
        }
        
        // 見つからなかった場合（選択解除など）の処理は必要に応じて追加
    }
}