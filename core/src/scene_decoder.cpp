#include "scene_decoder.hpp"
#include <QDebug>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>

namespace Rina::Core {

SceneDecoder::SceneDecoder(QObject *parent) : QObject(parent) {}

SceneDecoder::~SceneDecoder() {
    if (m_offscreenWindow) {
        m_offscreenWindow->deleteLater();
    }
}

void SceneDecoder::setTargetSceneId(int id) {
    if (m_targetSceneId == id)
        return;
    m_targetSceneId = id;
    emit targetSceneIdChanged();
    emit frameKeyChanged();
    updateRender();
}

void SceneDecoder::setCurrentFrame(int frame) {
    if (m_currentFrame == frame)
        return;
    m_currentFrame = frame;
    emit currentFrameChanged();
    updateRender();
}

void SceneDecoder::setStore(VideoFrameStore *store) {
    if (m_store == store)
        return;
    m_store = store;
    emit storeChanged();
}

void SceneDecoder::setTimelineBridge(QObject *bridge) {
    if (m_timelineBridge == bridge)
        return;
    m_timelineBridge = bridge;
    emit timelineBridgeChanged();
    updateRender();
}

QString SceneDecoder::frameKey() const { return QString("scene_%1").arg(m_targetSceneId); }

void SceneDecoder::ensureWindow() {
    if (m_offscreenWindow)
        return;

    QQmlEngine *engine = qmlEngine(this);
    if (!engine) {
        qWarning() << "[SceneDecoder] No QML Engine found";
        return;
    }

    // SceneRenderer.qml をロード
    QQmlComponent component(engine, QUrl("qrc:/qt/qml/Rina/ui/qml/SceneRenderer.qml"));
    if (component.status() != QQmlComponent::Ready) {
        qWarning() << "[SceneDecoder] Failed to load SceneRenderer:" << component.errorString();
        return;
    }

    QObject *obj = component.create();
    QQuickItem *item = qobject_cast<QQuickItem *>(obj);
    if (!item) {
        delete obj;
        return;
    }

    m_offscreenWindow = new QQuickWindow();
    // オフスクリーンレンダリング用に設定
    m_offscreenWindow->resize(1920, 1080); // 初期サイズ
    item->setParentItem(m_offscreenWindow->contentItem());
    m_rootItem = item;

    // ウィンドウを表示しないとレンダリングされない場合があるが、
    // grabWindowは内部でレンダリングをトリガーする
    m_offscreenWindow->create();
}

void SceneDecoder::updateRender() {
    if (m_targetSceneId < 0 || !m_store || !m_timelineBridge)
        return;

    ensureWindow();
    if (!m_offscreenWindow || !m_rootItem)
        return;

    // プロパティ更新
    m_rootItem->setProperty("sceneId", m_targetSceneId);
    m_rootItem->setProperty("currentFrame", m_currentFrame);
    m_rootItem->setProperty("timelineBridge", QVariant::fromValue(m_timelineBridge));

    // シーンサイズに合わせてウィンドウサイズを更新
    int w = m_rootItem->property("sceneWidth").toInt();
    int h = m_rootItem->property("sceneHeight").toInt();
    if (w > 0 && h > 0 && (m_offscreenWindow->width() != w || m_offscreenWindow->height() != h)) {
        m_offscreenWindow->resize(w, h);
    }

    // レンダリングとキャプチャ
    // 注意: grabWindowは重い処理なので、実運用ではFBO共有などを検討すべき
    QImage img = m_offscreenWindow->grabWindow();

    if (!img.isNull()) {
        m_store->setFrame(frameKey(), img);
    }
}

} // namespace Rina::Core
