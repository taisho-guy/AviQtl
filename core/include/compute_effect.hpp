#pragma once
#include <QQuickItem>
#include <QSGTextureProvider>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <mutex>
#include <vector>

namespace AviQtl::UI::Effects {

class ComputeTextureProvider : public QSGTextureProvider {
    Q_OBJECT
  public:
    QSGTexture *texture() const override { return m_texture; }
    void setTexture(QSGTexture *tex) {
        if (m_texture != tex) {
            m_texture = tex;
            emit textureChanged();
        }
    }

  private:
    QSGTexture *m_texture = nullptr;
};

class ComputeEffect : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString computeShader READ computeShader WRITE setComputeShader NOTIFY computeShaderChanged)
    Q_PROPERTY(QVariantMap uniforms READ uniforms WRITE setUniforms NOTIFY uniformsChanged)
    Q_PROPERTY(QVariantMap storageBuffers READ storageBuffers WRITE setStorageBuffers NOTIFY storageBuffersChanged)
    Q_PROPERTY(QQuickItem *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int workGroupSizeX READ workGroupSizeX WRITE setWorkGroupSizeX NOTIFY workGroupSizeXChanged)
    Q_PROPERTY(int workGroupSizeY READ workGroupSizeY WRITE setWorkGroupSizeY NOTIFY workGroupSizeYChanged)
    Q_PROPERTY(bool autoWorkGroup READ autoWorkGroup WRITE setAutoWorkGroup NOTIFY autoWorkGroupChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  public:
    explicit ComputeEffect(QQuickItem *parent = nullptr);
    ~ComputeEffect() override;

    QString computeShader() const { return m_computeShader; }
    QVariantMap uniforms() const { return m_uniforms; }
    QVariantMap storageBuffers() const { return m_storageBuffers; }
    QQuickItem *source() const { return m_source; }
    int workGroupSizeX() const { return m_workGroupSizeX; }
    int workGroupSizeY() const { return m_workGroupSizeY; }
    bool autoWorkGroup() const { return m_autoWorkGroup; }
    QString error() const { return m_error; }

    void setComputeShader(const QString &v);
    void setUniforms(const QVariantMap &v);
    void setStorageBuffers(const QVariantMap &v);
    void setSource(QQuickItem *v);
    void setWorkGroupSizeX(int v);
    void setWorkGroupSizeY(int v);
    void setAutoWorkGroup(bool v);

    bool isTextureProvider() const override { return true; }
    QSGTextureProvider *textureProvider() const override;

    // SSBOリードバック要求（QMLから呼出可能）
    Q_INVOKABLE void requestReadback(int binding);

  signals:
    void computeShaderChanged();
    void uniformsChanged();
    void storageBuffersChanged();
    void sourceChanged();
    void workGroupSizeXChanged();
    void workGroupSizeYChanged();
    void autoWorkGroupChanged();
    void errorChanged();
    void shaderError(const QString &message);

    // Readback完了シグナル
    void ssboReadbackCompleted(int binding, const QByteArray &data);

  protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

  private:
    void setError(const QString &msg);

    QString m_computeShader;
    QVariantMap m_uniforms;
    QVariantMap m_storageBuffers;
    QQuickItem *m_source = nullptr;
    int m_workGroupSizeX = 8;
    int m_workGroupSizeY = 8;
    bool m_autoWorkGroup = true;
    QString m_error;

    mutable ComputeTextureProvider *m_provider = nullptr;

    // Readbackキュー保護用
    std::mutex m_readbackMutex;
    std::vector<int> m_pendingReadbacks;
};

} // namespace AviQtl::UI::Effects
