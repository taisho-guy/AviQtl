#include "workspace.hpp"
#include <QFileInfo>
#include <QUrl>

namespace Rina::UI {

Workspace::Workspace(QObject *parent) : QObject(parent) { newProject(); }

void Workspace::setCurrentIndex(int index) {
    if (m_currentIndex == index || index < 0 || index >= m_timelines.size())
        return;
    m_currentIndex = index;
    emit currentIndexChanged();
    emit currentTimelineChanged();
}

TimelineController *Workspace::currentTimeline() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_timelines.size()) {
        return m_timelines[m_currentIndex];
    }
    return nullptr;
}

QVariantList Workspace::tabs() const {
    QVariantList list;
    for (auto *tc : m_timelines) {
        QVariantMap map;
        QString url = tc->currentProjectUrl();
        QString name;
        if (url.isEmpty()) {
            name = tc->property("untitledName").toString();
        } else {
            QUrl qurl(url);
            name = QFileInfo(qurl.toLocalFile().isEmpty() ? url : qurl.toLocalFile()).fileName();
        }
        map["name"] = name;
        map["hasUnsavedChanges"] = tc->hasUnsavedChanges();
        list.append(map);
    }
    return list;
}

void Workspace::setVideoFrameStore(Rina::Core::VideoFrameStore *store) {
    m_videoFrameStore = store;
    for (auto *tc : m_timelines) {
        tc->setVideoFrameStore(store);
    }
}

void Workspace::newProject() {
    auto *tc = new TimelineController(this);
    if (m_videoFrameStore) {
        tc->setVideoFrameStore(m_videoFrameStore);
    }
    tc->setProperty("untitledName", QString("Untitled %1").arg(m_untitledCounter++));

    connect(tc, &TimelineController::currentProjectUrlChanged, this, &Workspace::onTabStateChanged);
    connect(tc, &TimelineController::hasUnsavedChangesChanged, this, &Workspace::onTabStateChanged);

    m_timelines.append(tc);
    emit tabsChanged();

    setCurrentIndex(m_timelines.size() - 1);
}

void Workspace::closeProject(int index) {
    if (index < 0 || index >= m_timelines.size())
        return;

    auto *tc = m_timelines.takeAt(index);
    tc->deleteLater();

    if (m_timelines.isEmpty()) {
        m_currentIndex = -1;
        newProject();
    } else {
        if (m_currentIndex >= m_timelines.size()) {
            m_currentIndex = m_timelines.size() - 1;
        }
        emit currentIndexChanged();
        emit currentTimelineChanged();
        emit tabsChanged();
    }
}

void Workspace::loadProject(const QString &fileUrl) {
    auto *current = currentTimeline();
    if (current && current->currentProjectUrl().isEmpty() && !current->hasUnsavedChanges()) {
        current->loadProject(fileUrl);
    } else {
        newProject();
        currentTimeline()->loadProject(fileUrl);
    }
}

void Workspace::onTabStateChanged() { emit tabsChanged(); }

} // namespace Rina::UI
