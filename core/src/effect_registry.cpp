#include "effect_registry.hpp"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace Rina::Core {

void initializeStandardEffects() {
    // 標準エフェクトも外部ファイル(JSON)から読み込むため、ここは空にする
}

void EffectRegistry::loadEffectsFromDirectory(const QString &path) {
    QDir dir(path);
    if (!dir.exists()) {
        qWarning().noquote() << "Effect directory not found:" << path;
        return;
    }

    // Optimization: Pre-allocate string list is unnecessary, use brace
    // initialization
    QDirIterator it(path, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QJsonParseError error;
        // Optimization: Map file to memory for large JSONs
        const auto data = file.readAll();
        const auto doc = QJsonDocument::fromJson(data, &error);

        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning().noquote() << "Invalid JSON in" << file.fileName() << ":" << error.errorString();
            continue;
        }

        QJsonObject obj = doc.object();
        QString id = obj["id"].toString();
        QString name = obj["name"].toString();
        QString category = obj["category"].toString("filter");
        QString qmlFileName = obj["qml"].toString();
        QVariantMap params = obj["params"].toObject().toVariantMap();

        if (id.isEmpty() || name.isEmpty() || qmlFileName.isEmpty()) {
            qWarning().noquote() << "Skipping incomplete effect definition in:" << file.fileName();
            continue;
        }

        EffectMetadata meta;
        meta.id = id;
        meta.name = name;
        meta.category = category;
        meta.defaultParams = params;

        // QMLファイルの絶対パスを解決 (JSONファイルからの相対パスとして処理)
        QFileInfo jsonInfo(file.fileName());
        QString absoluteQmlPath = jsonInfo.absoluteDir().filePath(qmlFileName);

        if (QFile::exists(absoluteQmlPath)) {
            meta.qmlSource = QUrl::fromLocalFile(absoluteQmlPath).toString();
        } else {
            qWarning() << "Referenced QML file not found for effect" << id << ":" << absoluteQmlPath;
            continue;
        }

        registerEffect(meta);
        qDebug().noquote() << "Loaded external effect:" << name << "(" << id << ") from" << file.fileName();
    }
}

} // namespace Rina::Core
