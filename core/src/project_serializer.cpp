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
#include <algorithm>

namespace AviQtl::Core {

auto ProjectSerializer::save(const QString &fileUrl, const UI::TimelineService *timeline, const UI::ProjectService *project, QString *errorMessage) -> bool {
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty()) {
        path = fileUrl;
    }

    QJsonObject root;
    QJsonObject settings;
    settings.insert(QStringLiteral("width"), project->width());
    settings.insert(QStringLiteral("height"), project->height());
    settings.insert(QStringLiteral("fps"), project->fps());
    settings.insert(QStringLiteral("sampleRate"), project->sampleRate());
    root.insert(QStringLiteral("settings"), settings);

    // --- シーン情報の保存 ---
    QJsonArray scenesArray;
    for (const auto &scene : timeline->getAllScenes()) {
        QJsonObject sObj;
        sObj.insert(QStringLiteral("id"), scene.id);
        sObj.insert(QStringLiteral("name"), scene.name);
        sObj.insert(QStringLiteral("width"), scene.width);
        sObj.insert(QStringLiteral("height"), scene.height);
        sObj.insert(QStringLiteral("fps"), scene.fps);
        sObj.insert(QStringLiteral("totalFrames"), scene.totalFrames);
        sObj.insert(QStringLiteral("start"), scene.startFrame);
        sObj.insert(QStringLiteral("duration"), scene.durationFrames);
        scenesArray.append(sObj);
    }
    root.insert(QStringLiteral("scenes"), scenesArray);

    // --- 全シーンのクリップをフラットに保存 ---
    QJsonArray clipsArray;
    for (const auto &scene : timeline->getAllScenes()) {
        for (const auto &clip : std::as_const(scene.clips)) {
            QJsonObject clipObj;
            clipObj.insert(QStringLiteral("id"), clip.id);
            clipObj.insert(QStringLiteral("sceneId"), clip.sceneId); // シーンIDを保存
            clipObj.insert(QStringLiteral("type"), clip.type);
            clipObj.insert(QStringLiteral("start"), clip.startFrame);
            clipObj.insert(QStringLiteral("duration"), clip.durationFrames);
            clipObj.insert(QStringLiteral("layer"), clip.layer);

            QJsonArray audioPluginsArray;
            for (const auto &plugin : std::as_const(clip.audioPlugins)) {
                QJsonObject pObj;
                pObj.insert(QStringLiteral("id"), plugin.id);
                pObj.insert(QStringLiteral("enabled"), plugin.enabled);
                pObj.insert(QStringLiteral("params"), QJsonObject::fromVariantMap(plugin.params));
                audioPluginsArray.append(pObj);
            }
            clipObj.insert(QStringLiteral("audioPlugins"), audioPluginsArray);

            QJsonArray effArray;
            for (const auto *eff : std::as_const(clip.effects)) {
                QJsonObject eObj;
                eObj.insert(QStringLiteral("id"), eff->id());
                eObj.insert(QStringLiteral("name"), eff->name());
                eObj.insert(QStringLiteral("enabled"), eff->isEnabled());
                eObj.insert(QStringLiteral("params"), QJsonObject::fromVariantMap(eff->params()));
                eObj.insert(QStringLiteral("keyframes"), QJsonObject::fromVariantMap(eff->keyframeTracks()));
                effArray.append(eObj);
            }
            clipObj.insert(QStringLiteral("effects"), effArray);
            clipsArray.append(clipObj);
        }
    }
    root.insert(QStringLiteral("clips"), clipsArray);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("書き込み用にファイルを開けませんでした: ") + file.errorString();
        }
        return false;
    }
    if (file.write(QJsonDocument(root).toJson()) == -1) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("ファイルへの書き込みに失敗しました: ") + file.errorString();
        }
        return false;
    }
    return true;
}

auto ProjectSerializer::load(const QString &fileUrl, UI::TimelineService *timeline, UI::ProjectService *project, QString *errorMessage) -> bool {
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty()) {
        path = fileUrl;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("読み込み用にファイルを開けませんでした: ") + file.errorString();
        }
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (doc.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("JSONの解析に失敗しました: ") + error.errorString();
        }
        return false;
    }
    QJsonObject root = doc.object();

    // --- 1. 一時的な構造体にパース (検証フェーズ) ---

    // Project Settings
    bool hasSettings = root.contains(QStringLiteral("settings"));
    auto defaults = AviQtl::Core::SettingsManager::instance().settings();
    int pWidth = defaults.value(QStringLiteral("defaultProjectWidth"), 1920).toInt();
    int pHeight = defaults.value(QStringLiteral("defaultProjectHeight"), 1080).toInt();
    double pFps = defaults.value(QStringLiteral("defaultProjectFps"), 60.0).toDouble();
    int pSampleRate = defaults.value(QStringLiteral("defaultProjectSampleRate"), 48000).toInt();

    if (hasSettings) {
        QJsonObject s = root.value(QStringLiteral("settings")).toObject();
        pWidth = s.value(QStringLiteral("width")).toInt(pWidth);
        pHeight = s.value(QStringLiteral("height")).toInt(pHeight);
        pFps = s.value(QStringLiteral("fps")).toDouble(pFps);
        pSampleRate = s.value(QStringLiteral("sampleRate")).toInt(pSampleRate);
    }

    // Scenes (読み込み)
    QList<UI::SceneData> tempScenes;
    int maxSceneId = 0;
    if (root.contains(QStringLiteral("scenes"))) {
        QJsonArray scenesArray = root.value(QStringLiteral("scenes")).toArray();
        for (const auto &val : std::as_const(scenesArray)) {
            QJsonObject s = val.toObject();
            UI::SceneData scene;
            scene.id = s.value(QStringLiteral("id")).toInt();
            scene.name = s.value(QStringLiteral("name")).toString();
            scene.width = s.value(QStringLiteral("width")).toInt(pWidth);
            scene.height = s.value(QStringLiteral("height")).toInt(pHeight);
            scene.fps = s.value(QStringLiteral("fps")).toDouble(pFps);
            scene.totalFrames = s.value(QStringLiteral("totalFrames")).toInt(300); // SceneDataには残るがプロジェクト設定からは分離
            scene.startFrame = s.value(QStringLiteral("start")).toInt(0);
            scene.durationFrames = s.value(QStringLiteral("duration")).toInt(0);
            tempScenes.append(scene);
            maxSceneId = std::max(scene.id, maxSceneId);
        }
    } else {
        // 旧形式互換: デフォルトシーンを作成
        UI::SceneData rootScene;
        rootScene.id = 0;
        rootScene.name = QLatin1String("Root");
        tempScenes.append(rootScene);
    }

    // Clips
    QList<UI::ClipData> tempClips;
    int tempMaxId = 0;

    if (root.contains(QStringLiteral("clips"))) {
        QJsonArray clipsArray = root.value(QStringLiteral("clips")).toArray();
        for (const auto &val : std::as_const(clipsArray)) {
            QJsonObject c = val.toObject();
            UI::ClipData clip;
            clip.id = c.value(QStringLiteral("id")).toInt();
            clip.sceneId = c.value(QStringLiteral("sceneId")).toInt(0); // デフォルトは0
            tempMaxId = std::max(clip.id, tempMaxId);
            clip.type = c.value(QStringLiteral("type")).toString();
            clip.startFrame = c.value(QStringLiteral("start")).toInt();
            clip.durationFrames = c.value(QStringLiteral("duration")).toInt();
            clip.layer = c.value(QStringLiteral("layer")).toInt();

            if (c.contains(QStringLiteral("audioPlugins"))) {
                QJsonArray arr = c.value(QStringLiteral("audioPlugins")).toArray();
                for (const auto &val : std::as_const(arr)) {
                    QJsonObject pObj = val.toObject();
                    UI::AudioPluginState state;
                    state.id = pObj.value(QStringLiteral("id")).toString();
                    state.enabled = pObj.value(QStringLiteral("enabled")).toBool(true);
                    state.params = pObj.value(QStringLiteral("params")).toObject().toVariantMap();
                    clip.audioPlugins.append(state);
                }
            }

            if (c.contains(QStringLiteral("effects"))) {
                QJsonArray effArr = c.value(QStringLiteral("effects")).toArray();
                for (const auto &ev : std::as_const(effArr)) {
                    QJsonObject eObj = ev.toObject();
                    QString effId = eObj.value(QStringLiteral("id")).toString();
                    EffectMetadata meta = EffectRegistry::instance().getEffect(effId);

                    // 中断した場合にタイムラインによって自動削除されるのを防ぐため、最初は親なしで作成
                    // メタデータからUI定義を取得、なければ空
                    auto *eff = new UI::EffectModel(effId, eObj.value(QStringLiteral("name")).toString(), meta.kind, meta.categories, eObj.value(QStringLiteral("params")).toObject().toVariantMap(), meta.qmlSource, meta.uiDefinition, nullptr);
                    eff->setEnabled(eObj.value(QStringLiteral("enabled")).toBool(true));
                    if (eObj.contains(QStringLiteral("keyframes"))) {
                        eff->setKeyframeTracks(eObj.value(QStringLiteral("keyframes")).toObject().toVariantMap());
                    }

                    clip.effects.append(eff);
                }
            }

            // 後方互換性: transform エフェクトがなければ追加
            bool hasTransform = false;
            for (const auto *eff : std::as_const(clip.effects)) {
                if (eff->id() == QStringLiteral("transform")) {
                    hasTransform = true;
                    break;
                }
            }
            if (!hasTransform) {
                EffectMetadata meta = EffectRegistry::instance().getEffect(QStringLiteral("transform"));
                if (!meta.id.isEmpty()) {
                    auto *eff = new UI::EffectModel(meta.id, meta.name, meta.kind, meta.categories, meta.defaultParams, meta.qmlSource, meta.uiDefinition, nullptr);
                    clip.effects.prepend(eff); // 慣例的に先頭に追加
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
    for (const auto &clip : std::as_const(tempClips)) {
        for (auto *eff : std::as_const(clip.effects)) {
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
            tempScenes.value(0).clips.append(clip);
        }
    }

    timeline->setScenes(tempScenes);
    timeline->setNextClipId(tempMaxId + 1);
    timeline->setNextSceneId(maxSceneId + 1);
    QMetaObject::invokeMethod(timeline, "clipsChanged");

    return true;
}
} // namespace AviQtl::Core