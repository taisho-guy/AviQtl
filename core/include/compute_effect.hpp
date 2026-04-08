#pragma once
#include <QQuickItem>
#include <QSGTextureProvider>
#include <QVariantMap>

namespace Rina::UI::Effects {

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
    Q_PROPERTY(QString computeShader READ computeShader WRITE setComputeShader NOTIFY computeShaderChanged)
    Q_PROPERTY(QVariantMap uniforms READ uniforms WRITE setUniforms NOTIFY uniformsChanged)
    Q_PROPERTY(QQuickItem *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int workGroupSizeX READ workGroupSizeX WRITE setWorkGroupSizeX NOTIFY workGroupSizeXChanged)
    Q_PROPERTY(int workGroupSizeY READ workGroupSizeY WRITE setWorkGroupSizeY NOTIFY workGroupSizeYChanged)

  public:
    explicit ComputeEffect(QQuickItem *parent = nullptr);
    ~ComputeEffect() override;

    QString computeShader() const { return m_computeShader; }
    void setComputeShader(const QString &shader);

    QVariantMap uniforms() const { return m_uniforms; }
    void setUniforms(const QVariantMap &uniforms);

    QQuickItem *source() const { return m_source; }
    void setSource(QQuickItem *source);

    int workGroupSizeX() const { return m_workGroupSizeX; }
    void setWorkGroupSizeX(int x);

    int workGroupSizeY() const { return m_workGroupSizeY; }
    void setWorkGroupSizeY(int y);

    bool isTextureProvider() const override { return true; }
    QSGTextureProvider *textureProvider() const override;

  protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

  signals:
    void computeShaderChanged();
    void uniformsChanged();
    void sourceChanged();
    void workGroupSizeXChanged();
    void workGroupSizeYChanged();

  private:
    QString m_computeShader;
    QVariantMap m_uniforms;
    QQuickItem *m_source = nullptr;
    int m_workGroupSizeX = 1;
    int m_workGroupSizeY = 1;

    mutable ComputeTextureProvider *m_provider = nullptr;
};

} // namespace Rina::UI::Effects
