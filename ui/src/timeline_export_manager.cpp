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
    else if (format == "avi" || format == "mp4")
        return exportVideo(localPath, format, quality);
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
        emit m_controller->exportProgressChanged((frame * 100) / totalFrames);
    }
    if (view)
        view->setProperty("exportMode", false);
    emit m_controller->exportProgressChanged(100);
    return true;
}

bool TimelineExportManager::exportVideo(const QString &path, const QString &format, int quality) {
    Rina::Core::VideoEncoder encoder;
    Rina::Core::VideoEncoder::Config config;
    config.width = m_controller->project()->width();
    config.height = m_controller->project()->height();
    config.fps_num = static_cast<int>(m_controller->project()->fps() * 1000);
    config.fps_den = 1000;
    config.bitrate = static_cast<int>(config.width * config.height * m_controller->project()->fps() * 0.15);
    config.outputUrl = path;
    config.codecName = "h264_vaapi";

    if (!encoder.open(config))
        return false;
    encoder.addAudioStream(48000, 2, 192000);

    QQuickItem *view = m_controller->compositeView();
    if (view)
        view->setProperty("exportMode", true);
    QQuickItem *targetItem = view ? (view->property("view3D").value<QQuickItem *>() ? view->property("view3D").value<QQuickItem *>() : view) : nullptr;

    int totalFrames = m_controller->timelineDuration();
    for (int frame = 0; frame < totalFrames; ++frame) {
        m_controller->transport()->setCurrentFrame(frame);
        QImage renderedFrame;
        if (targetItem) {
            auto grabResult = targetItem->grabToImage(QSize(config.width, config.height));
            if (grabResult) {
                QEventLoop loop;
                connect(grabResult.get(), &QQuickItemGrabResult::ready, &loop, &QEventLoop::quit);
                QTimer::singleShot(2000, &loop, &QEventLoop::quit);
                loop.exec();
                renderedFrame = grabResult->image();
            }
        }
        encoder.pushFrame(renderedFrame, frame);
        int samplesNeeded = 48000 / m_controller->project()->fps();
        std::vector<float> audioSamples = m_controller->mediaManager()->audioMixer()->mix(frame, m_controller->project()->fps(), samplesNeeded);
        encoder.pushAudio(audioSamples.data(), samplesNeeded);
        emit m_controller->exportProgressChanged((frame * 100) / totalFrames);
    }
    if (view)
        view->setProperty("exportMode", false);
    encoder.close();
    emit m_controller->exportProgressChanged(100);
    return true;
}

void TimelineExportManager::exportVideoHW(Rina::Core::VideoEncoder *encoder) {
    if (!encoder || !m_controller->project())
        return;
    QQuickItem *view = m_controller->compositeView();
    if (!view)
        return;

    const int endFrame = m_controller->timelineDuration();
    const int width = m_controller->project()->width();
    const int height = m_controller->project()->height();
    encoder->addAudioStream(48000, 2, 192000);
    view->setProperty("exportMode", true);
    QQuickItem *targetItem = view->property("view3D").value<QQuickItem *>() ? view->property("view3D").value<QQuickItem *>() : view;

    for (int frame = 0; frame < endFrame; ++frame) {
        m_controller->transport()->setCurrentFrame(frame);
        QImage img;
        auto grabResult = targetItem->grabToImage(QSize(width, height));
        if (grabResult) {
            QEventLoop loop;
            connect(grabResult.get(), &QQuickItemGrabResult::ready, &loop, &QEventLoop::quit);
            QTimer::singleShot(2000, &loop, &QEventLoop::quit);
            loop.exec();
            img = grabResult->image();
        }
        if (!encoder->pushFrame(img, frame))
            break;
        int samplesNeeded = 48000 / m_controller->project()->fps();
        std::vector<float> audioSamples = m_controller->mediaManager()->audioMixer()->mix(frame, m_controller->project()->fps(), samplesNeeded);
        encoder->pushAudio(audioSamples.data(), samplesNeeded);
        if (frame % 10 == 0) {
            emit m_controller->exportProgressChanged((frame * 100) / endFrame);
            QCoreApplication::processEvents();
        }
    }
    view->setProperty("exportMode", false);
    encoder->close();
    emit m_controller->exportProgressChanged(100);
}
} // namespace Rina::UI