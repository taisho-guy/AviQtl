#include "effect_registry.hpp"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QUrl>

namespace Rina::Core {

void initializeStandardEffects() {
    // 標準エフェクトも外部ファイル(JSON)から読み込むため、ここは空にする
}

void EffectRegistry::loadEffectsFromDirectory(const QString& path) {
    QDir dir(path);
    if (!dir.exists()) {
        qWarning() << "Effect directory not found:" << path;
        return;
    }

    // 再帰的探索のためにQDirIteratorを使用 (Subdirectoriesフラグ)
    QDirIterator it(path, QStringList() << "*.json", QDir::Files, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        QString filePath = it.next();
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) continue;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "Invalid JSON in" << filePath << ":" << error.errorString();
            continue;
        }

        QJsonObject obj = doc.object();
        QString id = obj["id"].toString();
        QString name = obj["name"].toString();
        QString category = obj["category"].toString("filter");
        QString qmlFileName = obj["qml"].toString();
        QVariantMap params = obj["params"].toObject().toVariantMap();

        if (id.isEmpty() || name.isEmpty() || qmlFileName.isEmpty()) {
            qWarning() << "Skipping incomplete effect definition in:" << filePath;
            continue;
        }

        EffectMetadata meta;
        meta.id = id;
        meta.name = name;
        meta.category = category;
        meta.defaultParams = params;

        // QMLファイルの絶対パスを解決 (JSONファイルからの相対パスとして処理)
        QFileInfo jsonInfo(filePath);
        QString absoluteQmlPath = jsonInfo.absoluteDir().filePath(qmlFileName);
        
        if (QFile::exists(absoluteQmlPath)) {
            meta.qmlSource = QUrl::fromLocalFile(absoluteQmlPath).toString();
        } else {
            qWarning() << "Referenced QML file not found for effect" << id << ":" << absoluteQmlPath;
            continue;
        }

        registerEffect(meta);
        qDebug() << "Loaded external effect:" << name << "(" << id << ") from" << filePath;
    }
}

}
