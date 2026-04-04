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
        qWarning().noquote() << QStringLiteral("Effect directory not found:") << path;
        return;
    }

    // *.json ファイルをサブディレクトリを含めて検索
    QDirIterator it(path, {QStringLiteral("*.json")}, QDir::Files, QDirIterator::Subdirectories);

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
            qWarning().noquote() << QStringLiteral("Invalid JSON in") << file.fileName() << QStringLiteral(":") << error.errorString();
            continue;
        }

        QJsonObject obj = doc.object();
        QString id = obj.value(QStringLiteral("id")).toString();
        QString name = obj.value(QStringLiteral("name")).toString();
        QString category = obj.value(QStringLiteral("category")).toString(QStringLiteral("filter"));
        QString qmlFileName = obj.value(QStringLiteral("qml")).toString();
        QVariantMap params = obj.value(QStringLiteral("params")).toObject().toVariantMap();
        QVariantMap uiDef = obj.value(QStringLiteral("ui")).toObject().toVariantMap();

        if (id.isEmpty() || name.isEmpty() || qmlFileName.isEmpty()) {
            qWarning().noquote() << QStringLiteral("不完全なエフェクト定義のためスキップ:") << file.fileName() << QStringLiteral("。");
            if (id.isEmpty()) {
                qWarning().noquote() << QStringLiteral("  - 理由: 'id' フィールドが空または存在しません。");
            }
            if (name.isEmpty()) {
                qWarning().noquote() << QStringLiteral("  - 理由: 'name' フィールドが空または存在しません。");
            }
            if (qmlFileName.isEmpty()) {
                qWarning().noquote() << QStringLiteral("  - 理由: 'qml' フィールドが空または存在しません。");
            }
            qWarning().noquote() << QStringLiteral("  - 解析されたJSON内容:") << doc.toJson(QJsonDocument::Compact);
            continue;
        }

        EffectMetadata meta;
        meta.id = id;
        meta.name = name;
        meta.category = category;
        meta.defaultParams = params;
        meta.uiDefinition = uiDef;
        meta.color = obj.value(QStringLiteral("color")).toString();

        // qrc: で始まる場合は絶対パスとしてそのまま使用
        if (qmlFileName.startsWith(QStringLiteral("qrc:"))) {
            meta.qmlSource = qmlFileName;
            registerEffect(meta);
            qDebug().noquote() << QStringLiteral("外部エフェクトをロード:") << name << QStringLiteral("(") << id << QStringLiteral(") from") << file.fileName();
            continue;
        }

        // QMLファイルの絶対パスを解決 (JSONファイルからの相対パスとして処理)
        QFileInfo jsonInfo(file.fileName());
        QString absoluteQmlPath = jsonInfo.absoluteDir().filePath(qmlFileName);

        if (QFile::exists(absoluteQmlPath)) {
            meta.qmlSource = QUrl::fromLocalFile(absoluteQmlPath).toString();
        } else {
            qWarning() << QStringLiteral("参照されているQMLファイルが見つかりません。エフェクト:") << id << QStringLiteral("パス:") << absoluteQmlPath;
            continue;
        }

        registerEffect(meta);
        qDebug().noquote() << QStringLiteral("外部エフェクトをロード:") << name << QStringLiteral("(") << id << QStringLiteral(") from") << file.fileName();
    }
}

} // namespace Rina::Core
