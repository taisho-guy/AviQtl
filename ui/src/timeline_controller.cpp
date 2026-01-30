#include "timeline_controller.hpp"
#include "commands.hpp"
#include <QtGlobal>
#include "../../engine/timeline/ecs.hpp"
#include <algorithm>
#include <QFile>
#include <QUrl>
#include "effect_registry.hpp"
#include "project_service.hpp"
#include "transport_service.hpp"
#include "selection_service.hpp"
#include "timeline_service.hpp"
#include "clip_model.hpp"
#include "../../core/include/project_serializer.hpp"

namespace Rina::UI {

    TimelineController::TimelineController(QObject* parent) 
        : QObject(parent)
        , m_clipStartFrame(100)
        , m_clipDurationFrames(200)
        , m_layer(0)
        , m_isClipActive(false)
        , m_activeObjectType("rect") // デフォルトは図形
    {
        // サービスの初期化
        m_project = new ProjectService(this);
        m_transport = new TransportService(this);
        m_selection = new SelectionService(this);
        m_timeline = new TimelineService(m_selection, this);

        m_clipModel = new ClipModel(m_transport, this);

        // TimelineService signals
        connect(m_timeline, &TimelineService::clipsChanged, this, [this](){
            emit clipsChanged();
            updateActiveClipsList();
        });
        connect(m_timeline, &TimelineService::clipEffectsChanged, this, &TimelineController::clipEffectsChanged);

        // サービス間の連携
        connect(m_project, &ProjectService::fpsChanged, [this](){
            m_transport->updateTimerInterval(m_project->fps());
        });
        m_transport->updateTimerInterval(m_project->fps());

        // 再生位置が変わったらプレビュー更新
        connect(m_transport, &TransportService::currentFrameChanged, this, [this](){
            int nextFrame = m_transport->currentFrame();
            // ループ再生ロジック
            if (nextFrame > m_project->totalFrames()) {
                m_transport->setCurrentFrame(0);
            }
            updateClipActiveState();
            updateActiveClipsList();
            updateObjectX();
        });

        updateClipActiveState();
    }

    void TimelineController::undo() { m_timeline->undo(); }
    void TimelineController::redo() { m_timeline->redo(); }

    double TimelineController::timelineScale() const { return m_timelineScale; }
    void TimelineController::setTimelineScale(double scale) {
        if (qAbs(m_timelineScale - scale) > 0.001) {
            m_timelineScale = scale;
            emit timelineScaleChanged();
        }
    }

    // --- Generic Property Accessors ---
    void TimelineController::setClipProperty(const QString& name, const QVariant& value) {
        int id = m_selection->selectedClipId();
        if (id == -1) return;
        
        for (auto& clip : m_timeline->clipsMutable()) {
            if (clip.id == id) {
                // 暫定対応: プロパティ名に応じて適切なエフェクトのパラメータを更新する
                // 本来はUI側でエフェクトインデックスを指定すべき
                for (auto* eff : clip.effects) {
                    if (eff->params().contains(name)) {
                        eff->setParam(name, value);

                        // Update cache (flattened view for current UI)
                        QVariantMap data = m_selection->selectedClipData();
                        data[name] = value;
                        m_selection->select(id, data);
                        
                        // Update preview
                        updateActiveClipsList();
                        
                        // Trigger keyframe logic if needed
                        if (name == "x") updateObjectX(); 
                        break; // 最初に見つかったパラメータを更新
                    }
                }
                break;
            }
        }
    }

    QVariant TimelineController::getClipProperty(const QString& name) const {
        return m_selection->selectedClipData().value(name);
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
        int current = m_transport->currentFrame();
        // シンプルな矩形判定: Start <= Current < Start + Duration
        bool active = (current >= m_clipStartFrame) && (current < m_clipStartFrame + m_clipDurationFrames);
        if (m_isClipActive != active) {
            m_isClipActive = active;
            emit isClipActiveChanged();
        }

        if (m_isClipActive) {
            Rina::Engine::Timeline::ECS::instance().updateClipState(1, m_layer, current - m_clipStartFrame);
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
        float newVal = calculateInterpolatedValue(m_transport->currentFrame());
        int newX = qRound(newVal);
        // Update property via generic setter
        setClipProperty("x", newX);
    }

    float TimelineController::calculateInterpolatedValue(int frame) {
        if (m_keyframesX.empty()) return m_selection->selectedClipData().value("x", 0).toFloat();
        
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

    QString TimelineController::activeObjectType() const {
        return m_activeObjectType;
    }

    void TimelineController::createObject(const QString& type, int startFrame, int layer) {
        if (m_timeline) m_timeline->createClip(type, startFrame, layer);
    }

    QList<QObject*> TimelineController::getClipEffectsModel(int clipId) const {
        QList<QObject*> list;
        for (const auto& clip : m_timeline->clips()) {
            if (clip.id == clipId) {
                for (auto* eff : clip.effects) {
                    list.append(eff);
                }
                break;
            }
        }
        return list;
    }

    void TimelineController::updateClipEffectParam(int clipId, int effectIndex, const QString& paramName, const QVariant& value) {
        m_timeline->updateEffectParam(clipId, effectIndex, paramName, value);
    }

    QVariantList TimelineController::clips() const {
        QVariantList list;
        for (const auto& clip : m_timeline->clips()) {
            QVariantMap map;
            map["id"] = clip.id;
            map["type"] = clip.type;
            map["startFrame"] = clip.startFrame;
            map["durationFrames"] = clip.durationFrames;
            map["layer"] = clip.layer;
            
            // オブジェクトのQMLパスを取得して追加
            auto meta = Rina::Core::EffectRegistry::instance().getEffect(clip.type);
            if (!meta.qmlSource.isEmpty()) {
                map["qmlSource"] = meta.qmlSource;
            } else {
                // 見つからない場合のフォールバック
                // 実行時のカレントディレクトリ基準になるので注意が必要
                if(clip.type == "text") map["qmlSource"] = "file:objects/TextObject.qml";
                else if(clip.type == "rect") map["qmlSource"] = "file:objects/RectObject.qml";
            }
            
            // Include some properties for list view if needed
            // 暫定: テキストエフェクトがあればそのテキストを表示
            for(auto* eff : clip.effects) {
                if(eff->id() == "text") {
                    QVariantMap params = eff->params();
                    if(params.contains("text")) map["text"] = params["text"];
                    break;
                }
            }

            list.append(map);
        }
        return list;
    }

    void TimelineController::updateActiveClipsList() {
        // 現在フレームにあるクリップを抽出
        int current = m_transport->currentFrame();
        QList<ClipData*> active;
        for (auto& clip : m_timeline->clipsMutable()) {
            if (current >= clip.startFrame && 
                current < clip.startFrame + clip.durationFrames) {
                
                active.append(&clip);
            }
        }
        
        // レイヤー順（昇順）にソート
        std::sort(active.begin(), active.end(), [](const ClipData* a, const ClipData* b) {
            return a->layer < b->layer;
        });

        m_clipModel->updateClips(active);
    }

    void TimelineController::log(const QString& msg) {
        qDebug() << "[TimelineBridge] " << msg;
    }

    void TimelineController::updateClip(int id, int layer, int startFrame, int duration) {
        m_timeline->updateClip(id, layer, startFrame, duration);
    }

    bool TimelineController::saveProject(const QString& fileUrl) {
        return Rina::Core::ProjectSerializer::save(fileUrl, m_timeline, m_project);
    }

    bool TimelineController::loadProject(const QString& fileUrl) {
        return Rina::Core::ProjectSerializer::load(fileUrl, m_timeline, m_project);
    }

    void TimelineController::selectClip(int id) {
        if (m_selection->selectedClipId() == id) return;

        // 選択されたクリップの情報をプロパティにロード
        for (const auto& clip : m_timeline->clips()) {
            if (clip.id == id) {
                
                QVariantMap cache;
                for(auto* eff : clip.effects) {
                    QVariantMap params = eff->params();
                    for(auto it = params.begin(); it != params.end(); ++it) 
                        cache.insert(it.key(), it.value());
                }
                
                m_selection->select(id, cache);
                
                if (m_activeObjectType != clip.type) { m_activeObjectType = clip.type; emit activeObjectTypeChanged(); }
                if (m_layer != clip.layer) { m_layer = clip.layer; emit layerChanged(); }
                if (m_clipStartFrame != clip.startFrame) { m_clipStartFrame = clip.startFrame; emit clipStartFrameChanged(); }
                if (m_clipDurationFrames != clip.durationFrames) { m_clipDurationFrames = clip.durationFrames; emit clipDurationFramesChanged(); }
                
                // 選択変更時にプレビュー更新（選択枠表示などのため）
                updateActiveClipsList();
                return;
            }
        }
        
        // 見つからなかった場合（選択解除など）の処理は必要に応じて追加
        m_selection->select(-1, {});
    }

    // === エフェクト操作 (Internal実装) ===
    
    QVariantList TimelineController::getAvailableEffects() const {
        QVariantList list;
        const auto effects = Rina::Core::EffectRegistry::instance().getAllEffects();
        for (const auto& meta : effects) {
            if (meta.category != "filter") continue;
            QVariantMap m;
            m["id"] = meta.id;
            m["name"] = meta.name;
            list.append(m);
        }
        return list;
    }

    QVariantList TimelineController::getAvailableObjects() const {
        QVariantList list;
        const auto effects = Rina::Core::EffectRegistry::instance().getAllEffects();
        for (const auto& meta : effects) {
            if (meta.category != "object") continue;
            QVariantMap m;
            m["id"] = meta.id;
            m["name"] = meta.name;
            list.append(m);
        }
        return list;
    }

    void TimelineController::addEffect(int clipId, const QString& effectId) {
        m_timeline->addEffect(clipId, effectId);
    }

    void TimelineController::removeEffect(int clipId, int effectIndex) {
        m_timeline->removeEffect(clipId, effectIndex);
    }

    void TimelineController::deleteClip(int clipId) {
        if (m_timeline) m_timeline->deleteClip(clipId);
        if (m_selection->selectedClipId() == clipId) selectClip(-1);
    }

    void TimelineController::splitClip(int clipId, int frame) {
        if (m_timeline) m_timeline->splitClip(clipId, frame);
    }

    void TimelineController::copyClip(int clipId) {
        m_timeline->copyClip(clipId);
    }

    void TimelineController::cutClip(int clipId) {
        m_timeline->cutClip(clipId);
    }

    void TimelineController::pasteClip(int frame, int layer) {
        m_timeline->pasteClip(frame, layer);
    }
}