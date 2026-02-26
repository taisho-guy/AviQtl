#include "media_decoder.hpp"
#include "settings_manager.hpp"
#include <QMetaObject>

namespace Rina::Core {

MediaDecoder::MediaDecoder(int clipId, const QUrl &source, QObject *parent) : QObject(parent), m_clipId(clipId), m_source(source) { m_sampleRate = SettingsManager::instance().value("_runtime_projectSampleRate", 48000).toInt(); }

void MediaDecoder::scheduleStart() {
    // メインスレッドブロックを回避: 次イベントループで startDecoding() を呼ぶ
    QMetaObject::invokeMethod(this, &MediaDecoder::startDecoding, Qt::QueuedConnection);
}

} // namespace Rina::Core