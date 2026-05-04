#pragma once
#include <QObject>
#include <array>
#include <atomic>
#include <cstdint>

namespace AviQtl::UI {

// QMLとECS Worldの唯一の接点となるシングルトンブリッジ。
// QML側はこのクラスのみを通じてコマンドを発行し、ECS/Filamentの内部構造を知らない。
// コマンドはロックフリーSPSCリングバッファでECS CommandSystemへ転送される。
// 注: QML登録は main.cpp で qmlRegisterSingletonInstance により手動実施する。
class CoreBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(int currentFrame READ currentFrame NOTIFY currentFrameChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)

  public:
    static CoreBridge &instance();

    enum class CommandType : uint8_t { Seek, Play, Pause };

    struct Command {
        CommandType type;
        int32_t value;
    };

    Q_INVOKABLE void requestSeek(int frame);
    Q_INVOKABLE void requestPlay();
    Q_INVOKABLE void requestPause();

    bool dequeueCommand(Command &out);

    int currentFrame() const { return m_currentFrame.load(std::memory_order_relaxed); }
    bool isPlaying() const { return m_isPlaying.load(std::memory_order_relaxed); }

    void notifyFrameAdvanced(int frame);

  signals:
    void currentFrameChanged(int frame);
    void isPlayingChanged(bool playing);

  private:
    CoreBridge() = default;

    static constexpr int QUEUE_CAPACITY = 64;
    std::array<Command, QUEUE_CAPACITY> m_queue{};
    std::atomic<int> m_head{0};
    std::atomic<int> m_tail{0};
    std::atomic<int> m_currentFrame{0};
    std::atomic<bool> m_isPlaying{false};
};

} // namespace AviQtl::UI
