#include "project_serializer.hpp"
#include "timeline_service.hpp"
#include "project_service.hpp"
#include "effect_registry.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDebug>

namespace Rina::Core {

    bool ProjectSerializer::save(const QString& fileUrl, const UI::TimelineService* timeline, const UI::ProjectService* project) {
        QString path = QUrl(fileUrl).toLocalFile();
        if (path.isEmpty()) path = fileUrl;

        QJsonObject root;
        QJsonObject settings;
        settings["width"] = project->width();
        settings["height"] = project->height();
        settings["fps"] = project->fps();
        settings["totalFrames"] = project->totalFrames();
        root["settings"] = settings;

        QJsonArray clipsArray;
        for (const auto& clip : timeline->clips()) {
            QJsonObject clipObj;
            clipObj["id"] = clip.id;
            clipObj["type"] = clip.type;
            clipObj["start"] = clip.startFrame;
            clipObj["duration"] = clip.durationFrames;
            clipObj["layer"] = clip.layer;
            
            QJsonArray effArray;
            for (const auto& eff : clip.effects) {
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
        if (!file.open(QIODevice::WriteOnly)) return false;
        file.write(QJsonDocument(root).toJson());
        return true;
    }

    bool ProjectSerializer::load(const QString& fileUrl, UI::TimelineService* timeline, UI::ProjectService* project) {
        QString path = QUrl(fileUrl).toLocalFile();
        if (path.isEmpty()) path = fileUrl;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isNull()) return false;
        QJsonObject root = doc.object();

        if (root.contains("settings")) {
            QJsonObject s = root["settings"].toObject();
            project->setWidth(s["width"].toInt(1920));
            project->setHeight(s["height"].toInt(1080));
            project->setFps(s["fps"].toDouble(60.0));
            project->setTotalFrames(s["totalFrames"].toInt(3600));
        }

        timeline->clipsMutable().clear();
        int maxId = 0;
        QJsonArray clipsArray = root["clips"].toArray();
        for (const auto& val : clipsArray) {
            QJsonObject c = val.toObject();
            UI::ClipData clip;
            clip.id = c["id"].toInt();
            if (clip.id > maxId) maxId = clip.id;
            clip.type = c["type"].toString();
            clip.startFrame = c["start"].toInt();
            clip.durationFrames = c["duration"].toInt();
            clip.layer = c["layer"].toInt();
            
            if (c.contains("effects")) {
                QJsonArray effArr = c["effects"].toArray();
                for (const auto& ev : effArr) {
                    // (省略: エフェクト復元ロジック。TimelineService::createEffectData等を利用して実装)
                }
            }
            timeline->clipsMutable().append(clip);
        }
        timeline->setNextClipId(maxId + 1);
        return true;
    }
}