#include "project_serializer.hpp"
#include "effect_registry.hpp"
#include "settings_manager.hpp"
#include "project_service.hpp"
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
    settings["totalFrames"] = project->totalFrames();
    root["settings"] = settings;

    QJsonArray clipsArray;
    for (const auto &clip : timeline->clips()) {
        QJsonObject clipObj;
        clipObj["id"] = clip.id;
        clipObj["type"] = clip.type;
        clipObj["start"] = clip.startFrame;
        clipObj["duration"] = clip.durationFrames;
        clipObj["layer"] = clip.layer;

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
    int pTotalFrames = defaults.value("defaultProjectFrames", 300).toInt();
    double pFps = defaults.value("defaultProjectFps", 60.0).toDouble();

    if (hasSettings) {
        QJsonObject s = root["settings"].toObject();
        pWidth = s["width"].toInt(pWidth);
        pHeight = s["height"].toInt(pHeight);
        pFps = s["fps"].toDouble(pFps);
        pTotalFrames = s["totalFrames"].toInt(pTotalFrames);
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
            if (clip.id > tempMaxId)
                tempMaxId = clip.id;
            clip.type = c["type"].toString();
            clip.startFrame = c["start"].toInt();
            clip.durationFrames = c["duration"].toInt();
            clip.layer = c["layer"].toInt();

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
        project->setTotalFrames(pTotalFrames);
    }

    // 既存のクリップをクリアし、そのエフェクトを削除
    auto &currentClips = timeline->clipsMutable();
    for (auto &clip : currentClips) {
        qDeleteAll(clip.effects);
    }
    currentClips.clear();

    // 一時クリップをタイムラインに移動
    for (auto &clip : tempClips) {
        for (auto *eff : clip.effects) {
            eff->setParent(timeline); // 所有権を移譲
            QObject::connect(eff, &UI::EffectModel::keyframeTracksChanged, timeline, &UI::TimelineService::clipsChanged);
        }
        currentClips.append(clip);
    }

    timeline->setNextClipId(tempMaxId + 1);
    QMetaObject::invokeMethod(timeline, "clipsChanged");

    return true;
}
} // namespace Rina::Core