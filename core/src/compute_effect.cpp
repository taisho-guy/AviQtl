#include "compute_effect.hpp"
#include <QDebug>
#include <QFile>
#include <QQuickWindow>
#include <QSGRenderNode>
#include <QSGRendererInterface>
#include <QSGTexture>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <rhi/qshaderdescription.h>

namespace Rina::UI::Effects {

// QSGRenderNodeを継承し、prepare()でComputeを実行するノード
class ComputeNode : public QSGRenderNode {
  public:
    ComputeNode(QQuickWindow *window, ComputeTextureProvider *provider) : m_window(window), m_provider(provider) {}

    ~ComputeNode() override {
        delete m_outTex;
        delete m_dummyTex;
        delete m_ubo;
        delete m_pipeline;
        delete m_srb;
        delete m_sampler;
        delete m_qsgTex;
    }

    void updateState(const QString &shaderPath, const QVariantMap &uniforms, QSGTexture *sourceTex, const QSize &size, int wgX, int wgY) {
        if (m_shaderPath != shaderPath) {
            m_shaderPath = shaderPath;
            m_dirtyPipeline = true;
        }
        m_uniforms = uniforms;
        m_sourceTex = sourceTex;
        m_size = size;
        m_wgX = wgX;
        m_wgY = wgY;
    }

    // prepare()はレンダーパス開始前に呼ばれるため、Compute Passを安全に発行できる唯一の場所
    void prepare() override {
        if (m_size.isEmpty())
            return;

        // Qt 6.6+ QSGRenderNodeのメンバからRHIリソースを取得
        // Qt 6.11ではrhi()はQSGRenderNodeのメンバとして存在しないため
        // QSGRendererInterfaceを経由してQRhiを取得する
        QRhi *rhi = static_cast<QRhi *>(m_window->rendererInterface()->getResource(m_window, QSGRendererInterface::RhiResource));
        QRhiCommandBuffer *cb = this->commandBuffer();
        if (!rhi || !cb)
            return;

        bool textureRecreated = false;

        // 1. 出力 Storage Texture の構築
        // Storage/UAV用フラグは UsedWithLoadStore（StorageはQt6に存在しない）
        if (!m_outTex || m_outTex->pixelSize() != m_size) {
            delete m_qsgTex;
            m_qsgTex = nullptr;
            delete m_outTex;
            m_outTex = rhi->newTexture(QRhiTexture::RGBA8, m_size, 1, QRhiTexture::UsedWithLoadStore | QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
            if (!m_outTex->create())
                return;

            m_qsgTex = m_window->createTextureFromRhiTexture(m_outTex, QQuickWindow::TextureHasAlphaChannel);
            if (m_provider)
                m_provider->setTexture(m_qsgTex);
            textureRecreated = true;
        }

        // 入力なし時のフォールバック用ダミーテクスチャ
        if (!m_dummyTex) {
            m_dummyTex = rhi->newTexture(QRhiTexture::RGBA8, QSize(1, 1), 1, QRhiTexture::UsedAsTransferSource);
            m_dummyTex->create();
            m_sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
            m_sampler->create();
        }

        // 2. Compute Pipelineの構築（シェーダー変更時）
        if (m_dirtyPipeline || textureRecreated) {
            QFile f(m_shaderPath);
            if (f.open(QIODevice::ReadOnly)) {
                QShader shader = QShader::fromSerialized(f.readAll());
                m_shaderDesc = shader.description();

                m_uboSize = m_shaderDesc.uniformBlocks().isEmpty() ? 0 : m_shaderDesc.uniformBlocks()[0].size;
                if (m_uboSize > 0) {
                    delete m_ubo;
                    m_ubo = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_uboSize);
                    m_ubo->create();
                }

                delete m_pipeline;
                m_pipeline = rhi->newComputePipeline();
                m_pipeline->setShaderStage({QRhiShaderStage::Compute, shader});

                delete m_srb;
                m_srb = rhi->newShaderResourceBindings();

                QRhiTexture *inTexRhi = (m_sourceTex && m_sourceTex->rhiTexture()) ? m_sourceTex->rhiTexture() : m_dummyTex;

                // バインディング: 0=出力(UAV), 1=入力テクスチャ, 2=Uniform
                if (m_ubo) {
                    m_srb->setBindings({QRhiShaderResourceBinding::imageLoadStore(0, QRhiShaderResourceBinding::ComputeStage, m_outTex, 0), QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::ComputeStage, inTexRhi, m_sampler),
                                        QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::ComputeStage, m_ubo)});
                } else {
                    m_srb->setBindings({QRhiShaderResourceBinding::imageLoadStore(0, QRhiShaderResourceBinding::ComputeStage, m_outTex, 0), QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::ComputeStage, inTexRhi, m_sampler)});
                }
                m_srb->create();
                m_pipeline->setShaderResourceBindings(m_srb);
                m_pipeline->create();
            }
            m_dirtyPipeline = false;
        }

        // 3. Uniformデータのアップロード（passの外で実行）
        if (m_ubo && !m_shaderDesc.uniformBlocks().isEmpty()) {
            QByteArray uData(m_uboSize, 0);
            for (const auto &member : m_shaderDesc.uniformBlocks()[0].members) {
                if (!m_uniforms.contains(member.name))
                    continue;
                const QVariant val = m_uniforms.value(member.name);
                if (member.type == QShaderDescription::Float) {
                    float v = val.toFloat();
                    memcpy(uData.data() + member.offset, &v, sizeof(float));
                } else if (member.type == QShaderDescription::Int) {
                    int v = val.toInt();
                    memcpy(uData.data() + member.offset, &v, sizeof(int));
                }
                // TODO: vec2/vec4/mat4 のシリアライズは必要に応じて拡張
            }
            // resourceUpdateはpassの外（prepare内）で呼ぶ
            QRhiResourceUpdateBatch *rub = rhi->nextResourceUpdateBatch();
            rub->updateDynamicBuffer(m_ubo, 0, m_uboSize, uData.constData());
            cb->resourceUpdate(rub);
        }

        // 4. Compute Passのディスパッチ
        if (m_pipeline && m_srb && m_outTex) {
            cb->beginComputePass();             // 引数なし（Uniformは上でupdate済み）
            cb->setComputePipeline(m_pipeline); // pass内でパイプラインをバインド
            cb->setShaderResources(m_srb);      // SRBをバインド
            cb->dispatch(m_wgX, m_wgY, 1);      // ディスパッチ（dispatch が正しいメソッド名）
            cb->endComputePass();
        }
    }

    // このノードはprepare()でGPU演算し、テクスチャをプロバイダ経由で出力するため
    // render()での描画処理は不要
    void render(const RenderState *) override {}

    StateFlags changedStates() const override { return {}; }

    // NoExternalRendering: beginExternal/endExternalを使わないことをQtに通知
    RenderingFlags flags() const override { return NoExternalRendering; }

  private:
    QQuickWindow *m_window;
    ComputeTextureProvider *m_provider;

    QString m_shaderPath;
    QVariantMap m_uniforms;
    QSGTexture *m_sourceTex = nullptr;
    QSize m_size;
    int m_wgX = 1, m_wgY = 1;
    bool m_dirtyPipeline = true;

    QShaderDescription m_shaderDesc;
    int m_uboSize = 0;

    QRhiComputePipeline *m_pipeline = nullptr;
    QRhiShaderResourceBindings *m_srb = nullptr;
    QRhiBuffer *m_ubo = nullptr;
    QRhiTexture *m_outTex = nullptr;
    QRhiTexture *m_dummyTex = nullptr;
    QRhiSampler *m_sampler = nullptr;
    QSGTexture *m_qsgTex = nullptr;
};

ComputeEffect::ComputeEffect(QQuickItem *parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); }
ComputeEffect::~ComputeEffect() = default;

void ComputeEffect::setComputeShader(const QString &shader) {
    if (m_computeShader != shader) {
        m_computeShader = shader;
        emit computeShaderChanged();
        update();
    }
}
void ComputeEffect::setUniforms(const QVariantMap &uniforms) {
    m_uniforms = uniforms;
    emit uniformsChanged();
    update();
}
void ComputeEffect::setSource(QQuickItem *source) {
    if (m_source != source) {
        m_source = source;
        emit sourceChanged();
        update();
    }
}
void ComputeEffect::setWorkGroupSizeX(int x) {
    if (m_workGroupSizeX != x) {
        m_workGroupSizeX = x;
        emit workGroupSizeXChanged();
        update();
    }
}
void ComputeEffect::setWorkGroupSizeY(int y) {
    if (m_workGroupSizeY != y) {
        m_workGroupSizeY = y;
        emit workGroupSizeYChanged();
        update();
    }
}

QSGTextureProvider *ComputeEffect::textureProvider() const {
    if (!m_provider)
        m_provider = new ComputeTextureProvider();
    return m_provider;
}

QSGNode *ComputeEffect::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *node = static_cast<ComputeNode *>(oldNode);
    if (!node) {
        textureProvider(); // provider を初期化
        node = new ComputeNode(window(), m_provider);
    }

    QSGTexture *srcTex = nullptr;
    if (m_source && m_source->isTextureProvider())
        srcTex = m_source->textureProvider()->texture();

    // GUIスレッドの状態をレンダリングスレッドへコピー（DOD原則）
    node->updateState(m_computeShader, m_uniforms, srcTex, boundingRect().size().toSize(), m_workGroupSizeX, m_workGroupSizeY);

    return node;
}

void ComputeEffect::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();
}

} // namespace Rina::UI::Effects

#include "compute_effect.moc"
