#pragma once
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QQuickItem>
#include <QSGNode>
#include <QVariantMap>
#include <cstdint>

namespace AviQtl::UI::Effects {

// forward declare のみ: RHI ヘッダは compute_render_node.hpp に閉じ込める
class ComputeRenderNode;

// QML-exposed C++ class for Shader Storage Buffer Object / Vulkan RHI / QSGRenderNode / GPU
class ComputeEffect : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

    // 旧来プロパティ (Phase 6 以前)
    Q_PROPERTY(QVariantMap params READ params WRITE setParams NOTIFY paramsChanged)
    Q_PROPERTY(QVariantMap storageBuffers READ storageBuffers WRITE setStorageBuffers NOTIFY storageBuffersChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString shaderPath READ shaderPath WRITE setShaderPath NOTIFY shaderPathChanged)

    Q_PROPERTY(QString computeShader READ shaderPath WRITE setShaderPath NOTIFY shaderPathChanged)
    Q_PROPERTY(int workGroupSizeX READ workGroupSizeX WRITE setWorkGroupSizeX NOTIFY workGroupSizeXChanged)
    Q_PROPERTY(int workGroupSizeY READ workGroupSizeY WRITE setWorkGroupSizeY NOTIFY workGroupSizeYChanged)
    Q_PROPERTY(bool autoWorkGroup READ autoWorkGroup WRITE setAutoWorkGroup NOTIFY autoWorkGroupChanged)

  public:
    explicit ComputeEffect(QQuickItem *parent = nullptr);
    ~ComputeEffect() override;

    QVariantMap params() const { return m_params; }
    QVariantMap storageBuffers() const { return m_storageBuffers; }
    bool enabled() const { return m_enabled; }
    QString shaderPath() const { return m_shaderPath; }

    int workGroupSizeX() const { return m_workGroupX; }
    int workGroupSizeY() const { return m_workGroupY; }
    bool autoWorkGroup() const { return m_autoWorkGroup; }

    void setParams(const QVariantMap &params);
    void setStorageBuffers(const QVariantMap &buffers);
    void setEnabled(bool enabled);
    void setShaderPath(const QString &path);

    void setWorkGroupSizeX(int x);
    void setWorkGroupSizeY(int y);
    void setAutoWorkGroup(bool autoWG);

    Q_INVOKABLE void setStorageBufferRaw(const QString &name, int binding, const void *data, qsizetype byteSize);

  signals:
    void paramsChanged();
    void storageBuffersChanged();
    void enabledChanged();
    void shaderPathChanged();
    void workGroupSizeXChanged();
    void workGroupSizeYChanged();
    void autoWorkGroupChanged();

  protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

  private:
    void recalcAutoWorkGroup();

    // SSBO エントリ (setStorageBufferRaw → updatePaintNode → ComputeRenderNode の転送経路)
    struct RawSSBOEntry {
        QString name;
        int binding;
        QByteArray data;
    };
    static QByteArray ssboToBytes(const QVariantMap &bufferData);

    QVariantMap m_params;
    QVariantMap m_storageBuffers;
    bool m_enabled = true;
    QString m_shaderPath;
    bool m_dirty = false;

    int m_workGroupX = 1;
    int m_workGroupY = 1;
    bool m_autoWorkGroup = true;

    QList<RawSSBOEntry> m_rawSSBOs;
};

} // namespace AviQtl::UI::Effects
