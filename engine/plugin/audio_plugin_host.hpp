#pragma once
#include <QString>
#include <vector>

namespace Rina::Engine::Plugin {

class IAudioPlugin {
  public:
    virtual ~IAudioPlugin() = default;

    virtual bool load(const QString &path, int index = 0) = 0;
    // サンプルレートとブロックサイズを事前通知
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    // インターリーブステレオ (L,R,L,R...) float をインプレース処理
    virtual void process(float *buf, int frameCount) = 0;
    virtual void release() = 0;

    virtual QString name() const = 0;
    virtual QString format() const = 0;
    virtual int paramCount() const = 0;
    virtual QString paramName(int i) const = 0;
    virtual float getParam(int i) const = 0;
    virtual void setParam(int i, float v) = 0;
};

} // namespace Rina::Engine::Plugin