#include "effect_registry.hpp"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QUrl>

namespace Rina::Core {

void initializeStandardEffects() {
    auto& reg = EffectRegistry::instance();

    reg.registerEffect({
        "blur", "ぼかし", "",
        {{"size", 10}, {"aspect", 0}}
    });

    reg.registerEffect({
        "color_correction", "色調補正", "",
        {{"brightness", 100}, {"contrast", 100}, {"saturation", 100}}
    });

    reg.registerEffect({
        "glow", "発光", "",
        {{"strength", 50}, {"radius", 10}, {"color", "#ffffff"}}
    });
}

void EffectRegistry::loadEffectsFromDirectory(const QString& path) {
    QDir dir(path);
    if (!dir.exists()) return;

    QStringList filters; filters << "*.json";
    dir.setNameFilters(filters);

    for (const QString& filename : dir.entryList(QDir::Files)) {
        QFile file(dir.absoluteFilePath(filename));
        if (!file.open(QIODevice::ReadOnly)) continue;

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject()) continue;

        QJsonObject obj = doc.object();
        QString id = obj["id"].toString();
        QString name = obj["name"].toString();
        QString qmlFile = obj["qml"].toString();
        QVariantMap params = obj["params"].toObject().toVariantMap();

        if (id.isEmpty() || name.isEmpty()) continue;

        EffectMetadata meta;
        meta.id = id;
        meta.name = name;
        meta.defaultParams = params;

        // QMLファイルの絶対パスを解決 (JSONと同じ場所にあると仮定)
        if (!qmlFile.isEmpty()) {
            meta.qmlSource = QUrl::fromLocalFile(dir.absoluteFilePath(qmlFile)).toString();
        }

        registerEffect(meta);
        qDebug() << "Loaded external effect:" << name << "(" << id << ")";
    }
}

}
