#include "timeline_export_manager.hpp"
#include "../../core/include/video_encoder.hpp"
#include "../../engine/audio_mixer.hpp"
#include "timeline_controller.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QTimer>

namespace Rina::UI {

TimelineExportManager::TimelineExportManager(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller) {}

bool TimelineExportManager::exportMedia(const QString &fileUrl, const QString &format, int quality) {
    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    if (format == "image_sequence")
        return exportImageSequence(localPath, quality);
    return false;
}

bool TimelineExportManager::exportImageSequence(const QString &dir, int quality) {
    int totalFrames = m_controller->timelineDuration();
    QString baseName = dir;
    if (baseName.endsWith(".png"))
        baseName.chop(4);
    const int sequencePadding = 6;

    QQuickItem *view = m_controller->compositeView();
    if (view)
        view->setProperty("exportMode", true);
    QQuickItem *targetItem = view ? (view->property("view3D").value<QQuickItem *>() ? view->property("view3D").value<QQuickItem *>() : view) : nullptr;

    for (int frame = 0; frame < totalFrames; ++frame) {
        m_controller->transport()->setCurrentFrame(frame);
        QImage renderedFrame;
        if (targetItem) {
            auto grabResult = targetItem->grabToImage(QSize(m_controller->project()->width(), m_controller->project()->height()));
            if (grabResult) {
                QEventLoop loop;
                connect(grabResult.get(), &QQuickItemGrabResult::ready, &loop, &QEventLoop::quit);
                QTimer::singleShot(2000, &loop, &QEventLoop::quit);
                loop.exec();
                renderedFrame = grabResult->image();
            }
        }
        QString filename = QString("%1_%2.png").arg(baseName).arg(frame, sequencePadding, 10, QChar('0'));
        renderedFrame.save(filename, "PNG", quality);
        emit m_controller->exportProgressChanged((frame * 100) / totalFrames, frame + 1, totalFrames);
    }
    if (view)
        view->setProperty("exportMode", false);
    emit m_controller->exportProgressChanged(100, totalFrames, totalFrames);
    return true;
}

void TimelineExportManager::exportVideoAsync(const Rina::Core::VideoEncoder::Config &config) {
    if (m_exporting.load())
        return;
    m_cancelRequested = false;

    m_exportThread = QThread::create([this, config]() { runExport(config); });
    connect(m_exportThread, &QThread::finished, m_exportThread, &QObject::deleteLater);
    m_exportThread->start();
}

void TimelineExportManager::cancelExport() { m_cancelRequested = true; }

void TimelineExportManager::runExport(const Rina::Core::VideoEncoder::Config &config) {
    m_exporting = true;

    Rina::Core::VideoEncoder encoder;
    if (!encoder.open(config)) {
        emit exportFinished(false, "エンコーダーの初期化に失敗しました");
        m_exporting = false;
        return;
    }
    encoder.addAudioStream(48000, 2);

    const double fps = m_controller->project()->fps();
    const int startFrame = config.startFrame;
    const int endFrame = config.endFrame >= 0 ? config.endFrame : m_controller->timelineDuration();
    const int totalFrames = endFrame - startFrame;

    emit exportStarted(totalFrames);

    QQuickItem *view = m_controller->compositeView();
    QQuickItem *targetItem = view ? (view->property("view3D").value<QQuickItem *>() ? view->property("view3D").value<QQuickItem *>() : view) : nullptr;

    if (view)
        QMetaObject::invokeMethod(view, [view] { view->setProperty("exportMode", true); }, Qt::BlockingQueuedConnection);

    for (int frame = startFrame; frame < endFrame; ++frame) {
        if (m_cancelRequested.load()) {
            emit exportFinished(false, "キャンセルされました");
            goto cleanup;
        }

        QMetaObject::invokeMethod(m_controller->transport(), [this, frame] { m_controller->transport()->setCurrentFrame(frame); }, Qt::BlockingQueuedConnection);

        QImage img;
        if (targetItem) {
            QSharedPointer<QQuickItemGrabResult> grab;
            QMetaObject::invokeMethod(targetItem, [&] { grab = targetItem->grabToImage(QSize(config.width, config.height)); }, Qt::BlockingQueuedConnection);
            if (grab) {
                QEventLoop loop;
                connect(grab.get(), &QQuickItemGrabResult::ready, &loop, &QEventLoop::quit);
                QTimer::singleShot(2000, &loop, &QEventLoop::quit);
                loop.exec();
                img = grab->image();
            }
        }

        encoder.pushFrame(img, frame - startFrame);

        const int samplesNeeded = static_cast<int>(48000 / fps);
        auto audio = m_controller->mediaManager()->audioMixer()->mix(frame, fps, samplesNeeded);
        encoder.pushAudio(audio.data(), samplesNeeded);

        const int done = frame - startFrame + 1;
        if (done % 5 == 0 || done == totalFrames)
            emit exportProgressChanged(done * 100 / totalFrames, done, totalFrames);
    }

    encoder.close();
    emit exportFinished(true, "書き出し完了");

cleanup:
    if (view)
        QMetaObject::invokeMethod(view, [view] { view->setProperty("exportMode", false); }, Qt::BlockingQueuedConnection);
    m_exporting = false;
}

} // namespace Rina::UI