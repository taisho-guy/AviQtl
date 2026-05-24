#include "compute_render_node.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <rhi/qrhi.h>

Q_LOGGING_CATEGORY(lcComputeRenderNode, "aviqtl.compute_render_node")

namespace AviQtl::UI::Effects {

ComputeRenderNode::ComputeRenderNode(QQuickWindow *window) : m_window(window) {}

ComputeRenderNode::~ComputeRenderNode() { destroyResources(); }

void ComputeRenderNode::syncSSBOs(const QList<SSBOEntry> &entries) {
    // バッファの binding セットが変わった場合はレイアウト再構築フラグを立てる
    bool layoutChanged = (m_gpuBuffers.size() != entries.size());
    if (!layoutChanged) {
        for (int i = 0; i < entries.size(); ++i) {
            if (m_gpuBuffers[i].binding != entries[i].binding) {
                layoutChanged = true;
                break;
            }
        }
    }
    m_pendingSSBOs = entries;
    m_ssboDirty = !entries.isEmpty();
    if (layoutChanged)
        m_bufferLayoutDirty = true;
}

void ComputeRenderNode::syncShaderPath(const QString &path) {
    if (m_shaderPath == path)
        return;
    m_shaderPath = path;
    m_shaderDirty = true;
}

void ComputeRenderNode::syncSize(float w, float h) {
    if (qFuzzyCompare(m_width, w) && qFuzzyCompare(m_height, h))
        return;
    qCDebug(lcComputeRenderNode) << "ComputeRenderNode: Size changed to" << w << "x" << h;
    m_width = w;
    m_height = h;
    m_bufferLayoutDirty = true;
}

void ComputeRenderNode::syncInputTexture(QSGTexture *tex) {
    if (m_inputTexture == tex)
        return;
    qCDebug(lcComputeRenderNode) << "ComputeRenderNode: inputTexture updated to" << tex;
    m_inputTexture = tex;
    m_bufferLayoutDirty = true;
}

void ComputeRenderNode::syncWorkGroupSize(int x, int y, int z) {
    m_workGroupX = qMax(1, x);
    m_workGroupY = qMax(1, y);
    m_workGroupZ = qMax(1, z);
}

QRectF ComputeRenderNode::rect() const { return QRectF(0, 0, m_width, m_height); }

QRhi *ComputeRenderNode::resolveRhi() const { return static_cast<QRhi *>(m_window->rendererInterface()->getResource(m_window, QSGRendererInterface::RhiResource)); }

QRhiCommandBuffer *ComputeRenderNode::resolveCommandBuffer() const {
    auto *ri = m_window->rendererInterface();
    // Qt 6.6+: RhiRedirectCommandBuffer がメインレンダーパス前のコマンドバッファ
    // それ以前: CommandListResource にフォールバック
    auto *cb = static_cast<QRhiCommandBuffer *>(ri->getResource(m_window, QSGRendererInterface::RhiRedirectCommandBuffer));
    if (!cb) {
        cb = static_cast<QRhiCommandBuffer *>(ri->getResource(m_window, QSGRendererInterface::CommandListResource));
    }
    return cb;
}

bool ComputeRenderNode::ensureBuffers(QRhi *rhi) {
    if (!rhi->isFeatureSupported(QRhi::Compute)) {
        qCWarning(lcComputeRenderNode) << "Compute shaders are not supported on this hardware/backend.";
        return false;
    }

    bool textureSizeChanged = false;
    if (m_inputTexture) {
        QSize sz = m_inputTexture->textureSize();
        if (!m_outputTexture || m_outputTexture->pixelSize() != sz) {
            qCDebug(lcComputeRenderNode) << "ComputeRenderNode: Resizing output texture to" << sz;
            textureSizeChanged = true;
        }
    }

    bool needsRebuild = m_bufferLayoutDirty || textureSizeChanged;
    if (!needsRebuild && m_gpuBuffers.size() == m_pendingSSBOs.size()) {
        for (int i = 0; i < m_pendingSSBOs.size(); ++i) {
            if (m_gpuBuffers[i].size < static_cast<quint32>(m_pendingSSBOs[i].data.size())) {
                needsRebuild = true;
                break;
            }
        }
    }

    if (!needsRebuild)
        // バッファがなくてもテクスチャ（画像処理）があれば有効とみなす
        return (m_inputTexture != nullptr) || !m_gpuBuffers.isEmpty();

    // 既存 GPU バッファと SRB を安全に破棄
    for (auto &gb : m_gpuBuffers) {
        delete gb.buf;
        gb.buf = nullptr;
    }
    m_gpuBuffers.clear();
    if (m_outputTexture) {
        delete m_outputTexture;
        m_outputTexture = nullptr;
    }
    if (m_sampler) {
        delete m_sampler;
        m_sampler = nullptr;
    }
    if (m_vbuf) {
        delete m_vbuf;
        m_vbuf = nullptr;
    }
    if (m_ubuf) {
        delete m_ubuf;
        m_ubuf = nullptr;
    }
    if (m_renderSrb) {
        delete m_renderSrb;
        m_renderSrb = nullptr;
    }
    if (m_renderPipeline) {
        delete m_renderPipeline;
        m_renderPipeline = nullptr;
    }
    delete m_srb;
    m_srb = nullptr;

    if (m_pendingSSBOs.isEmpty()) {
        // バッファが空の場合はレイアウトが確定したとみなす
        if (m_gpuBuffers.isEmpty()) {
            m_bufferLayoutDirty = false;
        }
    }

    // 1. 出力サイズの決定 (入力があればそれに合わせ、なければアイテムサイズに合わせる)
    QSize sz(qMax(1, static_cast<int>(m_width)), qMax(1, static_cast<int>(m_height)));
    if (m_inputTexture) {
        QSize ts = m_inputTexture->textureSize();
        if (ts.isValid() && ts.width() > 0)
            sz = ts;
    }

    m_srb = rhi->newShaderResourceBindings();
    QList<QRhiShaderResourceBinding> bindings;

    // 2. 表示用リソース (m_renderSrb/m_outputTexture) の構築
    // 入力画像が無くても、サイズさえあれば表示用の「板」は先に作る
    if (true) {
        QRhiTexture *inRhiTex = m_inputTexture ? m_inputTexture->rhiTexture() : nullptr;

        if (!m_sampler) {
            m_sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
            m_sampler->create();
        }

        if (inRhiTex) {
            // 入力サンプラー (Binding 0)
            bindings.append(QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::ComputeStage, inRhiTex, m_sampler));
        }

        // 出力イメージ (Binding 1)
        m_outputTexture = rhi->newTexture(QRhiTexture::RGBA8, sz, 1, QRhiTexture::UsedWithLoadStore | QRhiTexture::RenderTarget);
        if (!m_outputTexture->create()) {
            return false;
        }
        bindings.append(QRhiShaderResourceBinding::imageLoadStore(1, QRhiShaderResourceBinding::ComputeStage, m_outputTexture, 0));

        // 描画用リソースの初期化 (Full-screen quad)
        static const float quadData[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f};
        m_vbuf = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(quadData));
        m_vbuf->create();

        // 頂点データは prepare() 内のバッチで安全にアップロードするためここでは作成のみ

        m_ubuf = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64);
        m_ubuf->create();

        m_renderSrb = rhi->newShaderResourceBindings();
        m_renderSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_ubuf), QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_outputTexture, m_sampler)});
        m_renderSrb->create();

        // 入力ソースはあるが実体(GPUメモリ)がまだの場合は、次フレームで再構築(Compute用)を促す
        if (m_inputTexture && !inRhiTex) {
            m_bufferLayoutDirty = true;
        }
    }

    // 各 SSBO エントリに対応するバッファを確保
    for (const auto &entry : std::as_const(m_pendingSSBOs)) {
        const qsizetype sz = entry.data.size();
        if (sz == 0)
            continue;
        // 頻繁な再確保を避けるため、少し余裕を持って確保
        auto *buf = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::StorageBuffer, static_cast<quint32>(sz));
        if (!buf->create()) {
            qCWarning(lcComputeRenderNode) << "QRhiBuffer::create() 失敗 binding=" << entry.binding;
            delete buf;
            return false;
        }
        m_gpuBuffers.push_back({entry.binding, buf, static_cast<quint32>(sz)});
        // SSBO は Binding 2 以降へオフセットしてバインド
        bindings.append(QRhiShaderResourceBinding::bufferLoad(entry.binding + 2, QRhiShaderResourceBinding::ComputeStage, buf));
    }

    m_srb->setBindings(bindings.cbegin(), bindings.cend());
    if (!m_srb->create()) {
        qCWarning(lcComputeRenderNode) << "QRhiShaderResourceBindings::create() 失敗";
        return false;
    }

    m_bufferLayoutDirty = false;
    m_shaderDirty = true;
    return true;
}

bool ComputeRenderNode::ensurePipeline(QRhi *rhi) {
    if (!m_shaderDirty && m_pipeline && m_renderPipeline)
        return true;

    if (m_pipeline)
        delete m_pipeline;
    m_pipeline = nullptr;

    if (m_renderPipeline)
        delete m_renderPipeline;
    m_renderPipeline = nullptr;

    m_error.clear();

    auto *ri = m_window->rendererInterface();

    // 1. 表示用グラフィックスパイプラインの構築
    // 1. 表示用グラフィックスパイプラインの構築 (Graphics Framework)
    bool graphicsOk = false;
    if (m_renderPipeline) {
        // 既に構築済みの場合はスキップ
    } else if (m_renderSrb) {
        m_renderPipeline = rhi->newGraphicsPipeline();
        m_renderPipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        m_renderPipeline->setShaderResourceBindings(m_renderSrb);
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({{4 * sizeof(float)}});
        inputLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0}, {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)}});
        m_renderPipeline->setVertexInputLayout(inputLayout);

        QString baseDir = QCoreApplication::applicationDirPath();
        if (baseDir.endsWith("/bin"))
            baseDir.chop(4);
        QString shaderDir = baseDir + "/common/shaders/";
        QFile vfile(shaderDir + "blit.vert.qsb"), ffile(shaderDir + "blit.frag.qsb");
        if (vfile.open(QIODevice::ReadOnly) && ffile.open(QIODevice::ReadOnly)) {
            m_renderPipeline->setShaderStages({{QRhiShaderStage::Vertex, QShader::fromSerialized(vfile.readAll())}, {QRhiShaderStage::Fragment, QShader::fromSerialized(ffile.readAll())}});
            auto *rt = static_cast<QRhiRenderPassDescriptor *>(ri->getResource(m_window, QSGRendererInterface::RenderPassResource));
            m_renderPipeline->setTargetBlends({{}});
            m_renderPipeline->setRenderPassDescriptor(rt);
            graphicsOk = m_renderPipeline->create();
        }
    }

    // 2. 計算用パイプラインの構築 (Compute Path)
    bool computeOk = false;
    if (m_srb && !m_shaderPath.isEmpty()) {
        QFile f(m_shaderPath);
        if (f.open(QIODevice::ReadOnly)) {
            m_shader = QShader::fromSerialized(f.readAll());
            if (m_shader.isValid()) {
                m_pipeline = rhi->newComputePipeline();
                m_pipeline->setShaderStage({QRhiShaderStage::Compute, m_shader});
                m_pipeline->setShaderResourceBindings(m_srb);
                computeOk = m_pipeline->create();
            }
        }
    }

    m_shaderDirty = false;
    qCDebug(lcComputeRenderNode) << (graphicsOk ? (computeOk ? "Compute/Graphics" : "Graphics Only") : "Compute Only") << "パイプライン構築完了:" << m_shaderPath;
    return graphicsOk; // 表示用さえ出来ていればOKとする
}

void ComputeRenderNode::prepare() {
    m_rhi = resolveRhi();
    if (!m_rhi)
        return;

    if (!ensureBuffers(m_rhi))
        return;
    if (!ensurePipeline(m_rhi))
        return;

    // 実行リソースが揃っているなら、フラグに関わらず実行を許可
    // (テクスチャの内容そのものが変わっている可能性があるため)
    if (!m_pipeline || !m_srb)
        return;

    auto *cb = resolveCommandBuffer();
    if (!cb)
        return;

    // GPU リソース更新用のバッチを作成
    QRhiResourceUpdateBatch *batch = m_rhi->nextResourceUpdateBatch();

    // 1. 頂点データのアップロード (m_vbuf が有効で、かつ中身が未ロードの場合に実行)
    static const float quadData[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    if (m_vbuf)
        batch->uploadStaticBuffer(m_vbuf, quadData);

    // 実行の追跡（初回またはリセット後のみ）
    static bool firstDispatch = true;
    if (firstDispatch || m_shaderDirty) {
        qCDebug(lcComputeRenderNode) << "ComputeRenderNode: Dispatching compute shader" << m_workGroupX << "x" << m_workGroupY;
        firstDispatch = false;
    }

    // 2. 行列の更新 (render() 内で行うのは不可。prepare で計算してアップロード)
    if (m_ubuf) {
        QMatrix4x4 mat;
        // 2Dエフェクトの場合、Itemの座標系を正規化デバイス座標(NDC)にマッピングする基本行列
        // projectionMatrix()をUIスレッドから同期するか、ここでウィンドウサイズから計算します。
        mat.scale(m_width, m_height, 1.0f);
        batch->updateDynamicBuffer(m_ubuf, 0, 64, mat.constData());
    }

    // 3. SSBO の更新
    for (const auto &entry : std::as_const(m_pendingSSBOs)) {
        for (const auto &gb : std::as_const(m_gpuBuffers)) {
            if (gb.binding != entry.binding || !gb.buf)
                continue;
            // Dynamic バッファの部分更新: サイズ不一致は ensureBuffers 内の再確保で吸収済み
            const quint32 uploadSize = static_cast<quint32>(qMin(entry.data.size(), gb.size));
            batch->updateDynamicBuffer(gb.buf, 0, uploadSize, entry.data.constData());
            break;
        }
    }

    // Compute パスを実行 (batch は beginComputePass で消費される)
    cb->beginComputePass(batch);
    cb->setComputePipeline(m_pipeline);
    cb->setShaderResources(m_srb);
    cb->dispatch(m_workGroupX, m_workGroupY, m_workGroupZ);
    cb->endComputePass();

    m_ssboDirty = false;
}

void ComputeRenderNode::render(const RenderState *state) {
    auto *cb = resolveCommandBuffer();
    if (!cb || !m_renderPipeline || !m_vbuf || !m_outputTexture)
        return;

    cb->setGraphicsPipeline(m_renderPipeline);

    // Qt 6.11 では RenderState から直接矩形が得られないため、ウィンドウの物理サイズをビューポートに使用
    const float dpr = m_window->devicePixelRatio();
    cb->setViewport(QRhiViewport(0, 0, static_cast<float>(m_window->width()) * dpr, static_cast<float>(m_window->height()) * dpr));

    // シーングラフによるクリッピングが有効な場合は適用
    if (state && state->scissorEnabled()) {
        const QRect s = state->scissorRect();
        cb->setScissor(QRhiScissor(s.x(), s.y(), s.width(), s.height()));
    }

    cb->setShaderResources(m_renderSrb);

    const QRhiCommandBuffer::VertexInput vbufBinding(m_vbuf, 0);
    cb->setVertexInput(0, 1, &vbufBinding);
    cb->draw(4);
}

void ComputeRenderNode::releaseResources() { destroyResources(); }

void ComputeRenderNode::destroyResources() {
    delete m_pipeline;
    m_pipeline = nullptr;
    delete m_srb;
    m_srb = nullptr;
    delete m_renderPipeline;
    m_renderPipeline = nullptr;
    delete m_renderSrb;
    m_renderSrb = nullptr;
    delete m_outputTexture;
    m_outputTexture = nullptr;
    delete m_sampler;
    m_sampler = nullptr;
    delete m_vbuf;
    m_vbuf = nullptr;
    delete m_ubuf;
    m_ubuf = nullptr;

    for (auto &gb : m_gpuBuffers) {
        if (gb.buf)
            delete gb.buf;
        gb.buf = nullptr;
    }
    m_gpuBuffers.clear();
    m_bufferLayoutDirty = true;
    m_shaderDirty = true;
}

QSGRenderNode::StateFlags ComputeRenderNode::changedStates() const { return {}; }

QSGRenderNode::RenderingFlags ComputeRenderNode::flags() const {
    // BoundedRectRendering: バウンディングボックス内のみ描画
    // OpaqueRendering: アルファブレンド不要
    return BoundedRectRendering | OpaqueRendering;
}

} // namespace AviQtl::UI::Effects
