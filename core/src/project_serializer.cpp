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

namespace Rina::Core {

auto ProjectSerializer::save(const QString &fileUrl, const UI::TimelineService *timeline, const UI::ProjectService *project, QString *errorMessage) -> bool {
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty()) {
        path = fileUrl;
    }

    QJsonObject root;
    QJsonObject settings;
    settings[QStringLiteral("width")] = project->width();
    settings[QStringLiteral("height")] = project->height();
    settings[QStringLiteral("fps")] = project->fps();
    settings[QStringLiteral("sampleRate")] = project->sampleRate();
    root[QStringLiteral("settings")] = settings;

    // --- シーン情報の保存 ---
    QJsonArray scenesArray;
    for (const auto &scene : timeline->getAllScenes()) {
        QJsonObject sObj;
        sObj[QStringLiteral("id")] = scene.id;
        sObj[QStringLiteral("name")] = scene.name;
        sObj[QStringLiteral("width")] = scene.width;
        sObj[QStringLiteral("height")] = scene.height;
        sObj[QStringLiteral("fps")] = scene.fps;
        sObj[QStringLiteral("totalFrames")] = scene.totalFrames;
        sObj[QStringLiteral("start")] = scene.startFrame;
        sObj[QStringLiteral("duration")] = scene.durationFrames;
        scenesArray.append(sObj);
    }
    root[QStringLiteral("scenes")] = scenesArray;

    // --- 全シーンのクリップをフラットに保存 ---
    QJsonArray clipsArray;
    for (const auto &scene : timeline->getAllScenes()) {
        for (const auto &clip : std::as_const(scene.clips)) {
            QJsonObject clipObj;
            clipObj[QStringLiteral("id")] = clip.id;
            clipObj[QStringLiteral("sceneId")] = clip.sceneId; // シーンIDを保存
            clipObj[QStringLiteral("type")] = clip.type;
            clipObj[QStringLiteral("start")] = clip.startFrame;
            clipObj[QStringLiteral("duration")] = clip.durationFrames;
            clipObj[QStringLiteral("layer")] = clip.layer;

            QJsonArray audioPluginsArray;
            for (const auto &plugin : std::as_const(clip.audioPlugins)) {
                QJsonObject pObj;
                pObj[QStringLiteral("id")] = plugin.id;
                pObj[QStringLiteral("enabled")] = plugin.enabled;
                pObj[QStringLiteral("params")] = QJsonObject::fromVariantMap(plugin.params);
                audioPluginsArray.append(pObj);
            }
            clipObj[QStringLiteral("audioPlugins")] = audioPluginsArray;

            QJsonArray effArray;
            for (const auto *eff : std::as_const(clip.effects)) {
                QJsonObject eObj;
                eObj[QStringLiteral("id")] = eff->id();
                eObj[QStringLiteral("name")] = eff->name();
                eObj[QStringLiteral("enabled")] = eff->isEnabled();
                eObj[QStringLiteral("params")] = QJsonObject::fromVariantMap(eff->params());
                eObj[QStringLiteral("keyframes")] = QJsonObject::fromVariantMap(eff->keyframeTracks());
                effArray.append(eObj);
            }
            clipObj[QStringLiteral("effects")] = effArray;
            clipsArray.append(clipObj);
        }
    }
    root[QStringLiteral("clips")] = clipsArray;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = "書き込み用にファイルを開けませんでした: " + file.errorString();
        }
        return false;
    }
    if (file.write(QJsonDocument(root).toJson()) == -1) {
        if (errorMessage != nullptr) {
            *errorMessage = "ファイルへの書き込みに失敗しました: " + file.errorString();
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
            *errorMessage = "読み込み用にファイルを開けませんでした: " + file.errorString();
        }
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (doc.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = "JSONの解析に失敗しました: " + error.errorString();
        }
        return false;
    }
    QJsonObject root = doc.object();

    // --- 1. 一時的な構造体にパース (検証フェーズ) ---

    // Project Settings
    bool hasSettings = root.contains(QStringLiteral("settings"));
    auto defaults = Rina::Core::SettingsManager::instance().settings();
    int pWidth = defaults.value(QStringLiteral("defaultProjectWidth"), 1920).toInt();
    int pHeight = defaults.value(QStringLiteral("defaultProjectHeight"), 1080).toInt();
    double pFps = defaults.value(QStringLiteral("defaultProjectFps"), 60.0).toDouble();
    int pSampleRate = defaults.value(QStringLiteral("defaultProjectSampleRate"), 48000).toInt();

    if (hasSettings) {
        QJsonObject s = root[QStringLiteral("settings")].toObject();
        pWidth = s[QStringLiteral("width")].toInt(pWidth);
        pHeight = s[QStringLiteral("height")].toInt(pHeight);
        pFps = s[QStringLiteral("fps")].toDouble(pFps);
        pSampleRate = s[QStringLiteral("sampleRate")].toInt(pSampleRate);
    }

    // Scenes (読み込み)
    QList<UI::SceneData> tempScenes;
    int maxSceneId = 0;
    if (root.contains(QStringLiteral("scenes"))) {
        QJsonArray scenesArray = root[QStringLiteral("scenes")].toArray();
        for (const auto &val : std::as_const(scenesArray)) {
            QJsonObject s = val.toObject();
            UI::SceneData scene;
            scene.id = s[QStringLiteral("id")].toInt();
            scene.name = s[QStringLiteral("name")].toString();
            scene.width = s[QStringLiteral("width")].toInt(pWidth);
            scene.height = s[QStringLiteral("height")].toInt(pHeight);
            scene.fps = s[QStringLiteral("fps")].toDouble(pFps);
            scene.totalFrames = s[QStringLiteral("totalFrames")].toInt(300); // SceneDataには残るがプロジェクト設定からは分離
            scene.startFrame = s[QStringLiteral("start")].toInt(0);
            scene.durationFrames = s[QStringLiteral("duration")].toInt(0);
            tempScenes.append(scene);
            maxSceneId = std::max(scene.id, maxSceneId);
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

    if (root.contains(QStringLiteral("clips"))) {
        QJsonArray clipsArray = root[QStringLiteral("clips")].toArray();
        for (const auto &val : std::as_const(clipsArray)) {
            QJsonObject c = val.toObject();
            UI::ClipData clip;
            clip.id = c[QStringLiteral("id")].toInt();
            clip.sceneId = c[QStringLiteral("sceneId")].toInt(0); // デフォルトは0
            tempMaxId = std::max(clip.id, tempMaxId);
            clip.type = c[QStringLiteral("type")].toString();
            clip.startFrame = c[QStringLiteral("start")].toInt();
            clip.durationFrames = c[QStringLiteral("duration")].toInt();
            clip.layer = c[QStringLiteral("layer")].toInt();

            if (c.contains(QStringLiteral("audioPlugins"))) {
                QJsonArray arr = c[QStringLiteral("audioPlugins")].toArray();
                for (const auto &val : std::as_const(arr)) {
                    QJsonObject pObj = val.toObject();
                    UI::AudioPluginState state;
                    state.id = pObj[QStringLiteral("id")].toString();
                    state.enabled = pObj[QStringLiteral("enabled")].toBool(true);
                    state.params = pObj[QStringLiteral("params")].toObject().toVariantMap();
                    clip.audioPlugins.append(state);
                }
            }

            if (c.contains(QStringLiteral("effects"))) {
                QJsonArray effArr = c[QStringLiteral("effects")].toArray();
                for (const auto &ev : std::as_const(effArr)) {
                    QJsonObject eObj = ev.toObject();
                    QString effId = eObj[QStringLiteral("id")].toString();
                    EffectMetadata meta = EffectRegistry::instance().getEffect(effId);

                    // 中断した場合にタイムラインによって自動削除されるのを防ぐため、最初は親なしで作成
                    // メタデータからUI定義を取得、なければ空
                    auto *eff = new UI::EffectModel(effId, eObj[QStringLiteral("name")].toString(), meta.category, eObj[QStringLiteral("params")].toObject().toVariantMap(), meta.qmlSource, meta.uiDefinition, nullptr);
                    eff->setEnabled(eObj[QStringLiteral("enabled")].toBool(true));
                    if (eObj.contains(QStringLiteral("keyframes"))) {
                        eff->setKeyframeTracks(eObj[QStringLiteral("keyframes")].toObject().toVariantMap());
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
                EffectMetadata meta = EffectRegistry::instance().getEffect("transform");
                if (!meta.id.isEmpty()) {
                    auto *eff = new UI::EffectModel(meta.id, meta.name, meta.category, meta.defaultParams, meta.qmlSource, meta.uiDefinition, nullptr);
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