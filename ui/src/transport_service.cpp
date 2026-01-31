#include "transport_service.hpp"

namespace Rina::UI {

TransportService::TransportService(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, [this]() { setCurrentFrame(m_currentFrame + 1); });
}

int TransportService::currentFrame() const { return m_currentFrame; }
void TransportService::setCurrentFrame(int f) {
    if (m_currentFrame != f) {
        m_currentFrame = f;
        emit currentFrameChanged();
    }
}

bool TransportService::isPlaying() const { return m_isPlaying; }
void TransportService::togglePlay() {
    m_isPlaying = !m_isPlaying;
    if (m_isPlaying)
        m_timer->start();
    else
        m_timer->stop();
    emit isPlayingChanged();
}

void TransportService::updateTimerInterval(double fps) {
    if (fps > 0) {
        m_timer->setInterval(static_cast<int>(1000.0 / fps));
    }
}
} // namespace Rina::UI