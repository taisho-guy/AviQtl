#include "effect_registry.hpp"
#include "engine/plugin/audio_plugin_manager.hpp"
#include "project_serializer.hpp"
#include "project_service.hpp"
#include "settings_manager.hpp"
#include "timeline_controller.hpp"
#include "timeline_service.hpp"
#include <QFile>
#include <QUrl>

namespace Rina::UI {

auto TimelineController::saveProject(const QString &fileUrl) -> bool {
    // 渡されたパスが空の場合は内部で保持しているパスを割り当てる
    QString targetUrl = fileUrl.isEmpty() ? m_currentProjectUrl : fileUrl;

    // パスが空の場合は新規作成直後なのでエラーで返す
    if (targetUrl.isEmpty()) {
        emit errorOccurred(tr("保存先のファイルパスが不明です"));
        return false;
    }

    QString error;
    bool result = Rina::Core::ProjectSerializer::save(targetUrl, m_timeline, m_project, &error);

    if (result) {
        // 保存に成功したパスを現在のプロジェクトパスとして記憶する
        m_currentProjectUrl = targetUrl;
        emit currentProjectUrlChanged();
    } else {
        emit errorOccurred(error);
    }
    return result;
}

auto TimelineController::loadProject(const QString &fileUrl) -> bool {
    QString error;
    bool result = Rina::Core::ProjectSerializer::load(fileUrl, m_timeline, m_project, &error);

    if (result) {
        // 読み込みに成功したパスを現在のプロジェクトパスとして記憶する
        m_currentProjectUrl = fileUrl;
        emit currentProjectUrlChanged();
    } else {
        emit errorOccurred(error);
    }
    return result;
}

auto TimelineController::getProjectInfo(const QString &fileUrl) -> QVariantMap {
    QVariantMap result;
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty()) {
        path = fileUrl;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "プロジェクトファイル情報取得のためファイルを開けませんでした:" << path;
        return result;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "プロジェクトJSONの解析に失敗しました:" << error.errorString();
        return result;
    }

    QJsonObject root = doc.object();
    if (root.contains(QStringLiteral("settings"))) {
        result = root.value(QStringLiteral("settings")).toObject().toVariantMap();
    }

    return result;
}

auto TimelineController::getAvailableEffects() -> QVariantList {
    QVariantList list;
    const auto effects = Rina::Core::EffectRegistry::instance().getAllEffects();
    for (const auto &meta : effects) {
        if (meta.category != "filter") {
            continue;
        }
        QVariantMap m;
        m.insert(QStringLiteral("id"), meta.id);
        m.insert(QStringLiteral("name"), meta.name);
        list.append(m);
    }
    return list;
}

auto TimelineController::getAvailableObjects() -> QVariantList {
    QVariantList list;
    const auto effects = Rina::Core::EffectRegistry::instance().getAllEffects();
    for (const auto &meta : effects) {
        if (meta.category != "object") {
            continue;
        }
        QVariantMap m;
        m.insert(QStringLiteral("id"), meta.id);
        m.insert(QStringLiteral("name"), meta.name);
        list.append(m);
    }
    return list;
}

auto TimelineController::getClipTypeColor(const QString &type) -> QString { return Rina::Core::EffectRegistry::instance().getEffect(type).color; }

auto TimelineController::getAvailableObjects(const QString &category) -> QVariantList {
    QVariantList list;
    const auto effects = Rina::Core::EffectRegistry::instance().getAllEffects();

    for (const auto &meta : effects) {
        if (meta.category == category) {
            QVariantMap map;
            map.insert(QStringLiteral("id"), meta.id);
            map.insert(QStringLiteral("name"), meta.name);
            list.append(map);
        }
    }
    return list;
}

auto TimelineController::getAvailableAudioPlugins() -> QVariantList { return Rina::Engine::Plugin::AudioPluginManager::instance().getPluginList(); }

auto TimelineController::getPluginCategories() -> QVariantList {
    // AudioPluginManagerから重複のないカテゴリ名リストを抽出
    return Rina::Engine::Plugin::AudioPluginManager::instance().getCategories();
}

auto TimelineController::getPluginsByCategory(const QString &category) -> QVariantList {
    // 特定カテゴリに属するプラグインのみを返す
    return Rina::Engine::Plugin::AudioPluginManager::instance().getPluginsInCategory(category);
}

} // namespace Rina::UI