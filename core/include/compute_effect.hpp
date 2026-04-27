#pragma once
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QQuickItem>
#include <QSGNode>
#include <QVariantMap>
#include <cstdint>

namespace AviQtl::UI::Effects {

// QML側から C++ のShader Storage Buffer Object を操作するブリッジクラス
// Vulkan RHI (QSGRenderNode) を通じてGPUバッファを管理する
class ComputeEffect : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap params READ params WRITE setParams NOTIFY paramsChanged)
    Q_PROPERTY(QVariantMap storageBuffers READ storageBuffers WRITE setStorageBuffers NOTIFY storageBuffersChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString shaderPath READ shaderPath WRITE setShaderPath NOTIFY shaderPathChanged)

  public:
    explicit ComputeEffect(QQuickItem *parent = nullptr);
    ~ComputeEffect() override;

    QVariantMap params() const { return m_params; }
    QVariantMap storageBuffers() const { return m_storageBuffers; }
    bool enabled() const { return m_enabled; }
    QString shaderPath() const { return m_shaderPath; }

    void setParams(const QVariantMap &params);
    void setStorageBuffers(const QVariantMap &buffers);
    void setEnabled(bool enabled);
    void setShaderPath(const QString &path);

    // フェーズ6: ECS writeSSBOLayout から直接呼ぶゼロコピーパス
    // data は GpuTransformSoA や GpuAudioSoA を reinterpret_cast した生バイト列
    // binding は GLSL 側の layout(std430, binding=N) の N に対応する
    Q_INVOKABLE void setStorageBufferRaw(const QString &name, int binding, const void *data, qsizetype byteSize);

  signals:
    void paramsChanged();
    void storageBuffersChanged();
    void enabledChanged();
    void shaderPathChanged();

  protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void updateState();

  private:
    QVariantMap m_params;
    QVariantMap m_storageBuffers;
    bool m_enabled = true;
    QString m_shaderPath;
    bool m_dirty = true;

    // フェーズ6: バイナリSSBOエントリ (name, binding, rawBytes)
    struct RawSSBOEntry {
        QString name;
        int binding = 0;
        QByteArray data;
    };
    QList<RawSSBOEntry> m_rawSSBOs;

    // QVariantMap ベースのSSBOをバイト列に変換 (Phase5以前の互換パス)
    static QByteArray ssboToBytes(const QVariantMap &bufferData);
};

} // namespace AviQtl::UI::Effects
