#include "project_serializer.hpp"
#include "effect_registry.hpp"
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
            *errorMessage = "Failed to open file for writing: " + file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(root).toJson()) == -1) {
        if (errorMessage)
            *errorMessage = "Failed to write to file: " + file.errorString();
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
            *errorMessage = "Failed to open file for reading: " + file.errorString();
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (doc.isNull()) {
        if (errorMessage)
            *errorMessage = "Failed to parse JSON: " + error.errorString();
        return false;
    }
    QJsonObject root = doc.object();

    // --- 1. Parse into temporary structures (Validation Phase) ---

    // Project Settings
    bool hasSettings = root.contains("settings");
    int pWidth = 1920, pHeight = 1080, pTotalFrames = 3600;
    double pFps = 60.0;

    if (hasSettings) {
        QJsonObject s = root["settings"].toObject();
        pWidth = s["width"].toInt(1920);
        pHeight = s["height"].toInt(1080);
        pFps = s["fps"].toDouble(60.0);
        pTotalFrames = s["totalFrames"].toInt(3600);
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

                    // Create with NO parent initially to avoid auto-deletion by timeline if we abort
                    auto *eff = new UI::EffectModel(effId, eObj["name"].toString(), eObj["params"].toObject().toVariantMap(), meta.qmlSource, nullptr);
                    eff->setEnabled(eObj["enabled"].toBool(true));
                    if (eObj.contains("keyframes"))
                        eff->setKeyframeTracks(eObj["keyframes"].toObject().toVariantMap());
                    
                    clip.effects.append(eff);
                }
            }
            tempClips.append(clip);
        }
    }

    // --- 2. Apply changes (Commit Phase) ---

    if (hasSettings) {
        project->setWidth(pWidth);
        project->setHeight(pHeight);
        project->setFps(pFps);
        project->setTotalFrames(pTotalFrames);
    }

    // Clear existing clips and delete their effects
    auto &currentClips = timeline->clipsMutable();
    for (auto &clip : currentClips) {
        qDeleteAll(clip.effects);
    }
    currentClips.clear();

    // Move temp clips to timeline
    for (auto &clip : tempClips) {
        for (auto *eff : clip.effects) {
            eff->setParent(timeline); // Transfer ownership
            QObject::connect(eff, &UI::EffectModel::keyframeTracksChanged, timeline, &UI::TimelineService::clipsChanged);
        }
        currentClips.append(clip);
    }

    timeline->setNextClipId(tempMaxId + 1);
    QMetaObject::invokeMethod(timeline, "clipsChanged");

    return true;
}
} // namespace Rina::Core