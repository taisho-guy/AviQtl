#pragma once
#include <QString>
#include <QtGlobal>
#include <vector>

namespace Rina::Engine::Plugin {

struct ParamInfo {
    QString name;
    float min = 0.0f;
    float max = 1.0f;
    float defaultValue = 0.0f;
    bool isLog = false;
    bool isInteger = false;
    bool isToggle = false;
};

class IAudioPlugin {
  public:
    virtual ~IAudioPlugin() = default;

    virtual bool load(const QString &path, int index = 0) = 0;
    // サンプルレートとブロックサイズを事前通知
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    // インターリーブステレオ (L,R,L,R...) float をインプレース処理
    virtual void process(float *buf, int frameCount) = 0;
    virtual bool active() const { return true; }
    // バイパス制御 (true: 処理有効, false: バイパス)
    virtual void setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;
    virtual void release() = 0;

    virtual QString name() const = 0;
    virtual QString format() const = 0;
    virtual int paramCount() const = 0;

    virtual QString paramName(int i) const = 0;
    virtual float getParam(int i) const = 0;
    virtual void setParam(int i, float v) = 0;
    virtual ParamInfo getParamInfo(int i) const { return {paramName(i)}; }

    // GUI
    virtual bool hasEditor() const { return false; }
    virtual void openEditor(qintptr parentId) {}
    virtual void closeEditor() {}
    virtual void onEditorIdle() {}
};

} // namespace Rina::Engine::Plugin