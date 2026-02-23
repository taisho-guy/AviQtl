#include "project_serializer.hpp"
#include "effect_registry.hpp"
#include "project_service.hpp"
#include "settings_manager.hpp"
#include "timeline_service.hpp"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace Rina::Core {

bool ProjectSerializer::save(const QString &fileUrl, const UI::TimelineService *timeline, const UI::ProjectService *project, QString *errorMessage) {
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty())
        path = fileUrl;

    QJsonObject root;
    QJsonObject settings;
    settings["width"] = project->width();
    settings["height"] = project->height();
    settings["fps"] = project->fps();
    settings["sampleRate"] = project->sampleRate();
    root["settings"] = settings;

    // --- シーン情報の保存 ---
    QJsonArray scenesArray;
    for (const auto &scene : timeline->getAllScenes()) {
        QJsonObject sObj;
        sObj["id"] = scene.id;
        sObj["name"] = scene.name;
        sObj["width"] = scene.width;
        sObj["height"] = scene.height;
        sObj["fps"] = scene.fps;
        sObj["totalFrames"] = scene.totalFrames;
        sObj["start"] = scene.startFrame;
        sObj["duration"] = scene.durationFrames;
        scenesArray.append(sObj);
    }
    root["scenes"] = scenesArray;

    // --- 全シーンのクリップをフラットに保存 ---
    QJsonArray clipsArray;
    for (const auto &scene : timeline->getAllScenes()) {
        for (const auto &clip : scene.clips) {
            QJsonObject clipObj;
            clipObj["id"] = clip.id;
            clipObj["sceneId"] = clip.sceneId; // シーンIDを保存
            clipObj["type"] = clip.type;
            clipObj["start"] = clip.startFrame;
            clipObj["duration"] = clip.durationFrames;
            clipObj["layer"] = clip.layer;

            QJsonArray audioPluginsArray;
            for (const auto &plugin : clip.audioPlugins) {
                QJsonObject pObj;
                pObj["id"] = plugin.id;
                pObj["enabled"] = plugin.enabled;
                pObj["params"] = QJsonObject::fromVariantMap(plugin.params);
                audioPluginsArray.append(pObj);
            }
            clipObj["audioPlugins"] = audioPluginsArray;

            QJsonArray effArray;
            for (const auto *eff : clip.effects) {
                QJsonObject eObj;
                eObj["id"] = eff->id();
                eObj["name"] = eff->name();
                eObj["enabled"] = eff->isEnabled();
                eObj["params"] = QJsonObject::fromVariantMap(eff->params());
                eObj["keyframes"] = QJsonObject::fromVariantMap(eff->keyframeTracks());
                effArray.append(eObj);
            }
            clipObj["effects"] = effArray;
            clipsArray.append(clipObj);
        }
    }
    root["clips"] = clipsArray;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = "書き込み用にファイルを開けませんでした: " + file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(root).toJson()) == -1) {
        if (errorMessage)
            *errorMessage = "ファイルへの書き込みに失敗しました: " + file.errorString();
        return false;
    }
    return true;
}

bool ProjectSerializer::load(const QString &fileUrl, UI::TimelineService *timeline, UI::ProjectService *project, QString *errorMessage) {
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty())
        path = fileUrl;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = "読み込み用にファイルを開けませんでした: " + file.errorString();
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (doc.isNull()) {
        if (errorMessage)
            *errorMessage = "JSONの解析に失敗しました: " + error.errorString();
        return false;
    }
    QJsonObject root = doc.object();

    // --- 1. 一時的な構造体にパース (検証フェーズ) ---

    // Project Settings
    bool hasSettings = root.contains("settings");
    auto defaults = Rina::Core::SettingsManager::instance().settings();
    int pWidth = defaults.value("defaultProjectWidth", 1920).toInt();
    int pHeight = defaults.value("defaultProjectHeight", 1080).toInt();
    double pFps = defaults.value("defaultProjectFps", 60.0).toDouble();
    int pSampleRate = defaults.value("defaultProjectSampleRate", 48000).toInt();

    if (hasSettings) {
        QJsonObject s = root["settings"].toObject();
        pWidth = s["width"].toInt(pWidth);
        pHeight = s["height"].toInt(pHeight);
        pFps = s["fps"].toDouble(pFps);
        pSampleRate = s["sampleRate"].toInt(pSampleRate);
    }

    // Scenes (読み込み)
    QList<UI::SceneData> tempScenes;
    int maxSceneId = 0;
    if (root.contains("scenes")) {
        QJsonArray scenesArray = root["scenes"].toArray();
        for (const auto &val : scenesArray) {
            QJsonObject s = val.toObject();
            UI::SceneData scene;
            scene.id = s["id"].toInt();
            scene.name = s["name"].toString();
            scene.width = s["width"].toInt(pWidth);
            scene.height = s["height"].toInt(pHeight);
            scene.fps = s["fps"].toDouble(pFps);
            scene.totalFrames = s["totalFrames"].toInt(300); // SceneDataには残るがプロジェクト設定からは分離
            scene.startFrame = s["start"].toInt(0);
            scene.durationFrames = s["duration"].toInt(0);
            tempScenes.append(scene);
            if (scene.id > maxSceneId)
                maxSceneId = scene.id;
        }
    } else {
        // 旧形式互換: デフォルトシーンを作成
        UI::SceneData rootScene;
        rootScene.id = 0;
        rootScene.name = "Root";
        tempScenes.append(rootScene);
    }

    // Clips
    QList<UI::ClipData> tempClips;
    int tempMaxId = 0;

    if (root.contains("clips")) {
        QJsonArray clipsArray = root["clips"].toArray();
        for (const auto &val : clipsArray) {
            QJsonObject c = val.toObject();
            UI::ClipData clip;
            clip.id = c["id"].toInt();
            clip.sceneId = c["sceneId"].toInt(0); // デフォルトは0
            if (clip.id > tempMaxId)
                tempMaxId = clip.id;
            clip.type = c["type"].toString();
            clip.startFrame = c["start"].toInt();
            clip.durationFrames = c["duration"].toInt();
            clip.layer = c["layer"].toInt();

            if (c.contains("audioPlugins")) {
                QJsonArray arr = c["audioPlugins"].toArray();
                for (const auto &val : arr) {
                    QJsonObject pObj = val.toObject();
                    UI::AudioPluginState state;
                    state.id = pObj["id"].toString();
                    state.enabled = pObj["enabled"].toBool(true);
                    state.params = pObj["params"].toObject().toVariantMap();
                    clip.audioPlugins.append(state);
                }
            }

            if (c.contains("effects")) {
                QJsonArray effArr = c["effects"].toArray();
                for (const auto &ev : effArr) {
                    QJsonObject eObj = ev.toObject();
                    QString effId = eObj["id"].toString();
                    EffectMetadata meta = EffectRegistry::instance().getEffect(effId);

                    // 中断した場合にタイムラインによって自動削除されるのを防ぐため、最初は親なしで作成
                    // メタデータからUI定義を取得、なければ空
                    auto *eff = new UI::EffectModel(effId, eObj["name"].toString(), eObj["params"].toObject().toVariantMap(), meta.qmlSource, meta.uiDefinition, nullptr);
                    eff->setEnabled(eObj["enabled"].toBool(true));
                    if (eObj.contains("keyframes"))
                        eff->setKeyframeTracks(eObj["keyframes"].toObject().toVariantMap());

                    clip.effects.append(eff);
                }
            }
            tempClips.append(clip);
        }
    }

    // --- 2. 変更を適用 (コミットフェーズ) ---

    if (hasSettings) {
        project->setWidth(pWidth);
        project->setHeight(pHeight);
        project->setFps(pFps);
        project->setSampleRate(pSampleRate);
    }

    // クリップを各シーンに分配
    for (auto &clip : tempClips) {
        for (auto *eff : clip.effects) {
            eff->setParent(timeline); // 所有権を移譲
            QObject::connect(eff, &UI::EffectModel::keyframeTracksChanged, timeline, &UI::TimelineService::clipsChanged);
        }

        // 対応するシーンを探して追加
        bool added = false;
        for (auto &scene : tempScenes) {
            if (scene.id == clip.sceneId) {
                scene.clips.append(clip);
                added = true;
                break;
            }
        }
        // シーンが見つからない場合はルート(0)または先頭に追加
        if (!added && !tempScenes.isEmpty()) {
            tempScenes[0].clips.append(clip);
        }
    }

    timeline->setScenes(tempScenes);
    timeline->setNextClipId(tempMaxId + 1);
    timeline->setNextSceneId(maxSceneId + 1);
    QMetaObject::invokeMethod(timeline, "clipsChanged");

    return true;
}
} // namespace Rina::Core