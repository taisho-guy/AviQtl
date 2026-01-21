#include "timeline_controller.hpp"
#include "../../engine/timeline/ecs.hpp"
#include <algorithm>
#include <QFile>
#include <QUrl>

namespace Rina::UI {
    TimelineController::TimelineController(QObject* parent) 
        : QObject(parent)
        , m_currentFrame(0)
        , m_clipStartFrame(100)
        , m_clipDurationFrames(200)
        , m_layer(0)
        , m_selectedClipId(-1)
        , m_isClipActive(false)
        , m_isPlaying(false)
        , m_activeObjectType("rect") // デフォルトは図形
    {
        m_playbackTimer = new QTimer(this);
        m_playbackTimer->setTimerType(Qt::PreciseTimer);
        updateTimerInterval(); // 初期FPSで設定
        connect(m_playbackTimer, &QTimer::timeout, this, &TimelineController::onPlaybackStep);
        updateClipActiveState();

        // --- Define Prototypes ---
        QVariantMap rectProps;
        rectProps["x"] = 0;
        rectProps["y"] = 0;
        rectProps["z"] = 0;
        rectProps["width"] = 100;
        rectProps["height"] = 100;
        rectProps["scale"] = 100.0;
        rectProps["aspect"] = 0.0;
        rectProps["rotationX"] = 0.0;
        rectProps["rotationY"] = 0.0;
        rectProps["rotationZ"] = 0.0;
        rectProps["anchorX"] = 0.0;
        rectProps["anchorY"] = 0.0;
        rectProps["anchorZ"] = 0.0;
        rectProps["color"] = "#66aa99";
        rectProps["opacity"] = 1.0;
        m_prototypes["rect"] = rectProps;

        QVariantMap textProps;
        textProps["x"] = 0;
        textProps["y"] = 0;
        textProps["z"] = 0;
        textProps["text"] = "Text";
        textProps["textSize"] = 64;
        textProps["scale"] = 100.0;
        textProps["aspect"] = 0.0;
        textProps["rotationX"] = 0.0;
        textProps["rotationY"] = 0.0;
        textProps["rotationZ"] = 0.0;
        textProps["anchorX"] = 0.0;
        textProps["anchorY"] = 0.0;
        textProps["anchorZ"] = 0.0;
        textProps["color"] = "#ffffff";
        textProps["opacity"] = 1.0;
        m_prototypes["text"] = textProps;
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

    // --- Generic Property Accessors ---
    void TimelineController::setClipProperty(const QString& name, const QVariant& value) {
        if (m_selectedClipId == -1) return;
        
        for (auto& clip : m_clips) {
            if (clip.id == m_selectedClipId) {
                clip.properties[name] = value;
                
                // Update cache
                m_selectedClipCache[name] = value;
                emit selectedClipDataChanged();
                
                // Update preview
                emit activeClipsChanged();
                
                // Trigger keyframe logic if needed
                if (name == "x") updateObjectX(); 
                break;
            }
        }
    }

    QVariant TimelineController::getClipProperty(const QString& name) const {
        if (m_selectedClipId != -1) {
            return m_selectedClipCache.value(name);
        }
        return QVariant();
    }

    QVariantMap TimelineController::selectedClipData() const {
        return m_selectedClipCache;
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
        int newX = qRound(newVal);
        // Update property via generic setter
        setClipProperty("x", newX);
    }

    void TimelineController::updateTimerInterval() {
        if (m_playbackTimer && m_projectFps > 0) {
            // 1000ms / FPS = 1フレームあたりのミリ秒数
            int interval = static_cast<int>(1000.0 / m_projectFps);
            m_playbackTimer->setInterval(interval);
        }
    }

    float TimelineController::calculateInterpolatedValue(int frame) {
        if (m_keyframesX.empty()) return m_selectedClipCache.value("x", 0).toFloat();
        
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
        
        // Initialize properties from prototype
        if (m_prototypes.contains(type)) {
            newClip.properties = m_prototypes[type];
        } else {
            newClip.properties = m_prototypes["rect"]; // Fallback
        }

        m_clips.append(newClip);
        emit clipsChanged();

        // 選択状態にする（既存の単一変数を「選択中のクリップ」として使う）
        m_clipStartFrame = newClip.startFrame;
        m_clipDurationFrames = newClip.durationFrames;
        m_layer = newClip.layer;
        m_activeObjectType = type;
        m_selectedClipId = newClip.id;
        
        // Update property cache
        m_selectedClipCache = newClip.properties;
        
        emit clipStartFrameChanged();
        emit clipDurationFramesChanged();
        emit layerChanged();
        emit activeObjectTypeChanged();
        emit activeClipsChanged();
        emit selectedClipIdChanged();
        emit selectedClipDataChanged();
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
            
            // Include some properties for list view if needed
            map["text"] = clip.properties.value("text", "");

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
            
            // Flatten dynamic properties into the map
            for (auto it = clip.properties.begin(); it != clip.properties.end(); ++it) {
                map[it.key()] = it.value();
            }

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

    void TimelineController::updateClip(int id, int layer, int startFrame, int duration) {
        if (startFrame < 0) startFrame = 0;
        if (duration < 1) duration = 1;
        if (layer < 0) layer = 0;

        bool changed = false;
        for (auto& clip : m_clips) {
            if (clip.id == id) {
                if (clip.layer != layer || clip.startFrame != startFrame || clip.durationFrames != duration) {
                    clip.layer = layer;
                    clip.startFrame = startFrame;
                    clip.durationFrames = duration;
                    changed = true;
                    
                    // 選択中のクリップならUIプロパティも更新
                    if (m_selectedClipId == id) {
                        m_layer = layer; emit layerChanged();
                        m_clipStartFrame = startFrame; emit clipStartFrameChanged();
                        m_clipDurationFrames = duration; emit clipDurationFramesChanged();
                    }
                }
                break;
            }
        }

        if (changed) {
            emit clipsChanged();
            emit activeClipsChanged();
            updateClipActiveState();
        }
    }

    bool TimelineController::saveProject(const QString& fileUrl) {
        QString path = QUrl(fileUrl).toLocalFile();
        if (path.isEmpty()) path = fileUrl; // URLでない場合はそのまま

        QJsonObject root;
        
        // プロジェクト設定
        QJsonObject settings;
        settings["width"] = m_projectWidth;
        settings["height"] = m_projectHeight;
        settings["fps"] = m_projectFps;
        settings["totalFrames"] = m_totalFrames;
        root["settings"] = settings;

        // クリップデータ
        QJsonArray clipsArray;
        for (const auto& clip : m_clips) {
            QJsonObject clipObj;
            clipObj["id"] = clip.id;
            clipObj["type"] = clip.type;
            clipObj["start"] = clip.startFrame;
            clipObj["duration"] = clip.durationFrames;
            clipObj["layer"] = clip.layer;
            
            // 動的プロパティ (QVariantMap -> QJsonObject)
            clipObj["properties"] = QJsonObject::fromVariantMap(clip.properties);
            
            clipsArray.append(clipObj);
        }
        root["clips"] = clipsArray;

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to open file for saving:" << path;
            return false;
        }
        
        file.write(QJsonDocument(root).toJson());
        return true;
    }

    bool TimelineController::loadProject(const QString& fileUrl) {
        QString path = QUrl(fileUrl).toLocalFile();
        if (path.isEmpty()) path = fileUrl;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open file for loading:" << path;
            return false;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isNull()) return false;

        QJsonObject root = doc.object();
        
        // 設定復元
        if (root.contains("settings")) {
            QJsonObject s = root["settings"].toObject();
            setProjectWidth(s["width"].toInt(1920));
            setProjectHeight(s["height"].toInt(1080));
            setProjectFps(s["fps"].toDouble(60.0));
            setTotalFrames(s["totalFrames"].toInt(3600));
        }

        // クリップ復元
        m_clips.clear();
        m_selectedClipId = -1;
        m_nextClipId = 1;

        QJsonArray clipsArray = root["clips"].toArray();
        for (const auto& val : clipsArray) {
            QJsonObject c = val.toObject();
            ClipData clip;
            clip.id = c["id"].toInt();
            if (clip.id >= m_nextClipId) m_nextClipId = clip.id + 1;
            
            clip.type = c["type"].toString();
            clip.startFrame = c["start"].toInt();
            clip.durationFrames = c["duration"].toInt();
            clip.layer = c["layer"].toInt();
            
            // プロパティ復元
            if (c.contains("properties")) {
                clip.properties = c["properties"].toObject().toVariantMap();
            } else {
                // 旧形式などのフォールバック
                if (m_prototypes.contains(clip.type)) clip.properties = m_prototypes[clip.type];
            }

            m_clips.append(clip);
        }

        emit clipsChanged();
        emit activeClipsChanged();
        emit selectedClipIdChanged();
        
        return true;
    }

    void TimelineController::selectClip(int id) {
        if (m_selectedClipId == id) return;
        
        m_selectedClipId = id;
        emit selectedClipIdChanged();

        // 選択されたクリップの情報をプロパティにロード
        for (const auto& clip : m_clips) {
            if (clip.id == id) {
                
                m_selectedClipCache = clip.properties;
                emit selectedClipDataChanged();
                
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