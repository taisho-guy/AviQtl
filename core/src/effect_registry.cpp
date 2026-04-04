#include "effect_registry.hpp"
#include <QCoreApplication>
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
    // 外部JSON経由で読み込むため、何もしない
}

void EffectRegistry::loadEffectsFromDirectory(const QString &path) {
    QDir dir(path);
    if (!dir.exists()) {
        qWarning().noquote() << "Effect directory not found:" << path;
        return;
    }

    // *.json ファイルをサブディレクトリを含めて検索
    QDirIterator it(path, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        QJsonParseError error;
        // 最適化: 巨大なJSONの場合はメモリマップドファイル化を検討
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
        QVariantMap uiDef = obj["ui"].toObject().toVariantMap();

        if (id.isEmpty() || name.isEmpty() || qmlFileName.isEmpty()) {
            qWarning().noquote() << "不完全なエフェクト定義のためスキップ:" << file.fileName() << "。";
            if (id.isEmpty()) {
                qWarning().noquote() << "  - 理由: 'id' フィールドが空または存在しません。";
            }
            if (name.isEmpty()) {
                qWarning().noquote() << "  - 理由: 'name' フィールドが空または存在しません。";
            }
            if (qmlFileName.isEmpty()) {
                qWarning().noquote() << "  - 理由: 'qml' フィールドが空または存在しません。";
            }
            qWarning().noquote() << "  - 解析されたJSON内容:" << doc.toJson(QJsonDocument::Compact);
            continue;
        }

        EffectMetadata meta;
        meta.id = id;
        meta.name = name;
        meta.category = category;
        meta.defaultParams = params;
        meta.uiDefinition = uiDef;
        meta.color = obj["color"].toString();

        // qrc: で始まる場合は絶対パスとしてそのまま使用
        if (qmlFileName.startsWith("qrc:")) {
            meta.qmlSource = qmlFileName;
            registerEffect(meta);
            qDebug().noquote() << "外部エフェクトをロード:" << name << "(" << id << ") from" << file.fileName();
            continue;
        }

        // QMLファイルの絶対パスを解決 (JSONファイルからの相対パスとして処理)
        QFileInfo jsonInfo(file.fileName());
        QString absoluteQmlPath = jsonInfo.absoluteDir().filePath(qmlFileName);

        if (QFile::exists(absoluteQmlPath)) {
            meta.qmlSource = QUrl::fromLocalFile(absoluteQmlPath).toString();
        } else {
            qWarning() << "参照されているQMLファイルが見つかりません。エフェクト:" << id << "パス:" << absoluteQmlPath;
            continue;
        }

        registerEffect(meta);
        qDebug().noquote() << "外部エフェクトをロード:" << name << "(" << id << ") from" << file.fileName();
    }
}

} // namespace Rina::Core
