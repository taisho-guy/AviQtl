#pragma once
#include <QByteArray>
#include <QList>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QSGRenderNode>
#include <QSGTexture>
#include <QString>
#include <QVariantMap>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

Q_DECLARE_LOGGING_CATEGORY(lcComputeRenderNode)

QT_FORWARD_DECLARE_CLASS(QQuickWindow)

namespace AviQtl::UI::Effects {

class ComputeRenderNode final : public QSGRenderNode {
  public:
    explicit ComputeRenderNode(QQuickWindow *window);
    ~ComputeRenderNode() override;

    void syncParams(const QVariantMap &params);
    void syncShaderPath(const QString &path);
    void syncInputTexture(QSGTexture *tex);
    void syncSize(float w, float h);
    void syncWorkGroupSize(int x, int y, int z = 1);

    QString errorMessage() const { return m_error; }

    QRectF rect() const override;
    void prepare() override;
    void render(const RenderState *state) override;
    void releaseResources() override;
    StateFlags changedStates() const override;
    RenderingFlags flags() const override;

  private:
    QRhi *resolveRhi() const;
    QRhiCommandBuffer *resolveCommandBuffer() const;

    bool ensureBuffers(QRhi *rhi);
    bool ensurePipeline(QRhi *rhi);
    void destroyResources();

    QQuickWindow *m_window = nullptr;
    QRhi *m_rhi = nullptr;

    QRhiTexture *m_outputTexture = nullptr;
    QRhiSampler *m_sampler = nullptr;
    QRhiBuffer *m_vbuf = nullptr;
    QRhiBuffer *m_ubuf = nullptr;
    QRhiBuffer *m_paramUbuf = nullptr;
    QRhiShaderResourceBindings *m_renderSrb = nullptr;
    QRhiTexture *m_renderTexture = nullptr;
    QRhiGraphicsPipeline *m_renderPipeline = nullptr;

    QRhiShaderResourceBindings *m_srb = nullptr;
    QRhiComputePipeline *m_pipeline = nullptr;
    QShader m_shader;

    QVariantMap m_params;
    bool m_paramsDirty = false;

    QString m_shaderPath;
    QString m_error;
    float m_width = 0;
    float m_height = 0;
    QSGTexture *m_inputTexture = nullptr;
    QRhiTexture *m_inputRhiTexture = nullptr;

    bool m_shaderDirty = true;
    bool m_bufferLayoutDirty = true;
    bool m_renderTargetDirty = true;
    bool m_verticesUploaded = false;

    int m_workGroupX = 1;
    int m_workGroupY = 1;
    int m_workGroupZ = 1;
};

} // namespace AviQtl::UI::Effects
