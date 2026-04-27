#include "compute_effect.hpp"
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMatrix4x4>
#include <QPointer>
#include <QQuickWindow>
#include <QSGRenderNode>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <rhi/qshaderdescription.h>

static QString resolveShaderPath(const QString &path) {
    if (path.startsWith(":/") || path.startsWith("qrc:/")) {
        return path.startsWith("qrc:/") ? ":" + path.mid(3) : path;
    }
    const QFileInfo fi(path);
    if (fi.isAbsolute() && fi.exists())
        return fi.absoluteFilePath();

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {appDir + "/effects/" + path, appDir + "/../Resources/effects/" + path, QDir::current().filePath("effects/" + path), path};
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c))
            return QFileInfo(c).absoluteFilePath();
    }
    return path;
}

namespace AviQtl::UI::Effects {

// Uniform シリアライズ (std140)
static void writeUniform(QByteArray &buf, const QShaderDescription::BlockVariable &member, const QVariant &val) {
    char *dst = buf.data() + member.offset;
    switch (member.type) {
    case QShaderDescription::Float: {
        float v = val.toFloat();
        memcpy(dst, &v, 4);
        break;
    }
    case QShaderDescription::Int: {
        int v = val.toInt();
        memcpy(dst, &v, 4);
        break;
    }
    case QShaderDescription::Bool:
    case QShaderDescription::Uint: {
        quint32 v = val.toUInt();
        memcpy(dst, &v, 4);
        break;
    }
    case QShaderDescription::Vec2: {
        QVector2D v;
        if (val.canConvert<QVector2D>())
            v = val.value<QVector2D>();
        else if (val.canConvert<QVariantList>()) {
            auto l = val.toList();
            v = {l.value(0).toFloat(), l.value(1).toFloat()};
        }
        float d[2] = {v.x(), v.y()};
        memcpy(dst, d, 8);
        break;
    }
    case QShaderDescription::Vec3: {
        QVector3D v;
        if (val.canConvert<QVector3D>())
            v = val.value<QVector3D>();
        else if (val.canConvert<QVariantList>()) {
            auto l = val.toList();
            v = {l.value(0).toFloat(), l.value(1).toFloat(), l.value(2).toFloat()};
        } else if (val.canConvert<QColor>()) {
            QColor c = val.value<QColor>();
            v = {(float)c.redF(), (float)c.greenF(), (float)c.blueF()};
        }
        float d[3] = {v.x(), v.y(), v.z()};
        memcpy(dst, d, 12);
        break;
    }
    case QShaderDescription::Vec4: {
        QVector4D v;
        if (val.canConvert<QVector4D>())
            v = val.value<QVector4D>();
        else if (val.canConvert<QVariantList>()) {
            auto l = val.toList();
            v = {l.value(0).toFloat(), l.value(1).toFloat(), l.value(2).toFloat(), l.value(3).toFloat()};
        } else if (val.canConvert<QColor>()) {
            QColor c = val.value<QColor>();
            v = {(float)c.redF(), (float)c.greenF(), (float)c.blueF(), (float)c.alphaF()};
        }
        float d[4] = {v.x(), v.y(), v.z(), v.w()};
        memcpy(dst, d, 16);
        break;
    }
    case QShaderDescription::Mat4: {
        QMatrix4x4 m;
        if (val.canConvert<QMatrix4x4>())
            m = val.value<QMatrix4x4>();
        memcpy(dst, m.constData(), 64);
        break;
    }
    default:
        qWarning() << "[ComputeEffect] unsupported uniform type:" << member.name;
    }
}

// ─── SSBO ペイロード変換 ─────────────────────────────────────────
static QByteArray ssboToBytes(const QVariant &val) {
    if (val.canConvert<QByteArray>())
        return val.toByteArray();
    if (val.canConvert<QVariantList>()) {
        const auto list = val.toList();
        QByteArray buf(list.size() * 4, Qt::Uninitialized);
        float *p = reinterpret_cast<float *>(buf.data());
        for (const auto &v : list)
            *p++ = v.toFloat();
        return buf;
    }
    return {};
}

class ComputePassNode : public QSGRenderNode {
  public:
    QString m_errorMessage;

    ComputePassNode(QQuickWindow *win, ComputeTextureProvider *prov, QPointer<ComputeEffect> item) : m_window(win), m_provider(prov), m_effectItem(item) {}

    ~ComputePassNode() override {
        for (auto &s : m_ssbos)
            delete s.buf;
        delete m_outTex;
        delete m_dummyTex;
        delete m_ubo;
        delete m_pipeline;
        delete m_srb;
        delete m_sampler;
        delete m_qsgTex;
    }

    void updateState(const QString &shaderPath, const QVariantMap &uniforms, const QVariantMap &storageBuffers, QSGTexture *sourceTex, const QSize &size, bool autoWG, int wgX, int wgY) {
        if (m_shaderPath != shaderPath) {
            m_shaderPath = shaderPath;
            m_dirtyPipeline = true;
        }
        if (m_sourceTex != sourceTex) {
            m_sourceTex = sourceTex;
            m_dirtySrb = true;
        }
        if (m_storageBuffers != storageBuffers) {
            m_storageBuffers = storageBuffers;
            m_dirtySsbo = true;
        }
        m_uniforms = uniforms;
        m_size = size;
        m_autoWG = autoWG;
        m_wgXOverride = wgX;
        m_wgYOverride = wgY;
    }

    void addReadbacks(const std::vector<int> &bindings) {
        for (int b : bindings) {
            m_pendingReadbacks.push_back(b);
        }
    }

    void prepare() override {
        if (m_size.isEmpty())
            return;

        QRhi *rhi = static_cast<QRhi *>(m_window->rendererInterface()->getResource(m_window, QSGRendererInterface::RhiResource));
        QRhiCommandBuffer *cb = this->commandBuffer();
        if (!rhi || !cb)
            return;

        bool texRecreated = false;
        if (!m_outTex || m_outTex->pixelSize() != m_size) {
            delete m_qsgTex;
            m_qsgTex = nullptr;
            delete m_outTex;
            m_outTex = rhi->newTexture(QRhiTexture::RGBA8, m_size, 1, QRhiTexture::UsedWithLoadStore | QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
            if (!m_outTex->create()) {
                reportError(u"Output texture creation failed"_qs);
                return;
            }
            m_qsgTex = m_window->createTextureFromRhiTexture(m_outTex, QQuickWindow::TextureHasAlphaChannel);
            if (m_provider)
                m_provider->setTexture(m_qsgTex);
            texRecreated = true;
            m_dirtySrb = true;
        }

        if (!m_dummyTex) {
            m_dummyTex = rhi->newTexture(QRhiTexture::RGBA8, {1, 1}, 1, QRhiTexture::UsedAsTransferSource);
            m_dummyTex->create();
            m_sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
            m_sampler->create();
            m_dirtySrb = true;
        }

        if (m_shaderPath.isEmpty())
            return;

        if (m_dirtyPipeline || texRecreated) {
            const QString shaderFile = resolveShaderPath(m_shaderPath);
            QFile f(shaderFile);
            if (!f.open(QIODevice::ReadOnly)) {
                reportError(u"Cannot open shader: "_qs + shaderFile);
                return;
            }
            QShader shader = QShader::fromSerialized(f.readAll());
            if (!shader.isValid()) {
                reportError(u"Invalid shader: "_qs + shaderFile);
                return;
            }
            m_errorMessage.clear();
            m_shaderDesc = shader.description();

            const auto ls = m_shaderDesc.computeShaderLocalSize();
            m_localSize[0] = ls[0] > 0u ? (int)ls[0] : 8;
            m_localSize[1] = ls[1] > 0u ? (int)ls[1] : 8;

            m_uboSize = m_shaderDesc.uniformBlocks().isEmpty() ? 0 : m_shaderDesc.uniformBlocks()[0].size;
            if (m_uboSize > 0) {
                delete m_ubo;
                m_ubo = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_uboSize);
                m_ubo->create();
            }

            for (auto &s : m_ssbos)
                delete s.buf;
            m_ssbos.clear();
            for (const auto &sb : m_shaderDesc.storageBlocks()) {
                SsboEntry e;
                e.binding = sb.binding;
                const QByteArray initData = ssboToBytes(m_storageBuffers.value(QString::number(sb.binding)));
                e.size = initData.isEmpty() ? 64 : initData.size();
                e.buf = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::StorageBuffer, e.size);
                e.buf->create();
                m_ssbos.push_back(e);
            }

            delete m_pipeline;
            m_pipeline = rhi->newComputePipeline();
            m_pipeline->setShaderStage({QRhiShaderStage::Compute, shader});
            m_dirtyPipeline = false;
            m_dirtySrb = true;
            m_dirtySsbo = true;
        }

        if ((m_dirtySrb || m_dirtySsbo) && m_pipeline) {
            QRhiTexture *inTex = (m_sourceTex && m_sourceTex->rhiTexture()) ? m_sourceTex->rhiTexture() : m_dummyTex;

            QList<QRhiShaderResourceBinding> bindings;
            bindings.append(QRhiShaderResourceBinding::imageLoadStore(0, QRhiShaderResourceBinding::ComputeStage, m_outTex, 0));
            bindings.append(QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::ComputeStage, inTex, m_sampler));
            if (m_ubo)
                bindings.append(QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::ComputeStage, m_ubo));
            for (const auto &s : m_ssbos)
                bindings.append(QRhiShaderResourceBinding::bufferLoadStore(s.binding, QRhiShaderResourceBinding::ComputeStage, s.buf));

            delete m_srb;
            m_srb = rhi->newShaderResourceBindings();
            m_srb->setBindings(bindings.cbegin(), bindings.cend());
            m_srb->create();
            m_pipeline->setShaderResourceBindings(m_srb);
            m_pipeline->create();
            m_dirtySrb = false;
            m_dirtySsbo = false;
        }

        if (!m_pipeline || !m_srb || !m_outTex)
            return;

        auto *rub = rhi->nextResourceUpdateBatch();
        bool hasUpdate = false;

        if (m_ubo && !m_shaderDesc.uniformBlocks().isEmpty()) {
            QByteArray uData(m_uboSize, 0);
            for (const auto &mem : m_shaderDesc.uniformBlocks()[0].members) {
                if (m_uniforms.contains(mem.name)) {
                    writeUniform(uData, mem, m_uniforms.value(mem.name));
                }
            }
            rub->updateDynamicBuffer(m_ubo, 0, m_uboSize, uData.constData());
            hasUpdate = true;
        }

        for (auto &s : m_ssbos) {
            const QByteArray data = ssboToBytes(m_storageBuffers.value(QString::number(s.binding)));
            if (data.isEmpty() || data.size() != s.size)
                continue;
            rub->updateDynamicBuffer(s.buf, 0, s.size, data.constData());
            hasUpdate = true;
        }

        // Readbackキューの処理
        for (int binding : m_pendingReadbacks) {
            auto it = std::find_if(m_ssbos.begin(), m_ssbos.end(), [binding](const SsboEntry &e) { return e.binding == binding; });
            if (it != m_ssbos.end() && it->buf) {
                auto *readback = new QRhiReadbackResult();
                QPointer<ComputeEffect> safeItem = m_effectItem;
                readback->completed = [safeItem, binding, readback]() {
                    if (safeItem) {
                        QByteArray res = readback->data;
                        QMetaObject::invokeMethod(safeItem.data(), [safeItem, binding, res]() { emit safeItem->ssboReadbackCompleted(binding, res); }, Qt::QueuedConnection);
                    }
                    // メインスレッドで安全に破棄
                    QMetaObject::invokeMethod(QCoreApplication::instance(), [readback]() { delete readback; }, Qt::QueuedConnection);
                };
                rub->readBackBuffer(it->buf, 0, it->size, readback);
                hasUpdate = true;
            }
        }
        m_pendingReadbacks.clear();

        if (hasUpdate) {
            cb->resourceUpdate(rub);
        } else {
            rub->release();
        }

        const int wgX = m_autoWG ? (m_size.width() + m_localSize[0] - 1) / m_localSize[0] : m_wgXOverride;
        const int wgY = m_autoWG ? (m_size.height() + m_localSize[1] - 1) / m_localSize[1] : m_wgYOverride;

        cb->beginComputePass();
        cb->setComputePipeline(m_pipeline);
        cb->setShaderResources(m_srb);
        cb->dispatch(wgX, wgY, 1);
        cb->endComputePass();
    }

    void render(const RenderState *) override {}
    StateFlags changedStates() const override { return {}; }
    RenderingFlags flags() const override { return NoExternalRendering; }

  private:
    void reportError(const QString &msg) {
        if (m_errorMessage != msg)
            m_errorMessage = msg;
        qWarning() << "[ComputeEffect]" << msg;
    }

    struct SsboEntry {
        int binding = -1;
        QRhiBuffer *buf = nullptr;
        int size = 0;
    };

    QQuickWindow *m_window;
    ComputeTextureProvider *m_provider;
    QPointer<ComputeEffect> m_effectItem;

    QString m_shaderPath;
    QVariantMap m_uniforms;
    QVariantMap m_storageBuffers;
    QSGTexture *m_sourceTex = nullptr;
    QSize m_size;
    bool m_autoWG = true;
    int m_wgXOverride = 8, m_wgYOverride = 8;
    int m_localSize[2] = {8, 8};
    bool m_dirtyPipeline = true;
    bool m_dirtySrb = true;
    bool m_dirtySsbo = false;

    QShaderDescription m_shaderDesc;
    int m_uboSize = 0;
    std::vector<SsboEntry> m_ssbos;
    std::vector<int> m_pendingReadbacks;

    QRhiComputePipeline *m_pipeline = nullptr;
    QRhiShaderResourceBindings *m_srb = nullptr;
    QRhiBuffer *m_ubo = nullptr;
    QRhiTexture *m_outTex = nullptr;
    QRhiTexture *m_dummyTex = nullptr;
    QRhiSampler *m_sampler = nullptr;
    QSGTexture *m_qsgTex = nullptr;
};

// ─── ComputeEffect ──────────────────────────────────────────────────────────
ComputeEffect::ComputeEffect(QQuickItem *parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); }
ComputeEffect::~ComputeEffect() = default;

void ComputeEffect::setComputeShader(const QString &v) {
    if (m_computeShader != v) {
        m_computeShader = v;
        emit computeShaderChanged();
        update();
    }
}
void ComputeEffect::setUniforms(const QVariantMap &v) {
    m_uniforms = v;
    emit uniformsChanged();
    update();
}
void ComputeEffect::setStorageBuffers(const QVariantMap &v) {
    if (m_storageBuffers != v) {
        m_storageBuffers = v;
        emit storageBuffersChanged();
        update();
    }
}
void ComputeEffect::setSource(QQuickItem *v) {
    if (m_source != v) {
        m_source = v;
        emit sourceChanged();
        update();
    }
}
void ComputeEffect::setWorkGroupSizeX(int v) {
    if (m_workGroupSizeX != v) {
        m_workGroupSizeX = v;
        emit workGroupSizeXChanged();
        update();
    }
}
void ComputeEffect::setWorkGroupSizeY(int v) {
    if (m_workGroupSizeY != v) {
        m_workGroupSizeY = v;
        emit workGroupSizeYChanged();
        update();
    }
}
void ComputeEffect::setAutoWorkGroup(bool v) {
    if (m_autoWorkGroup != v) {
        m_autoWorkGroup = v;
        emit autoWorkGroupChanged();
        update();
    }
}
void ComputeEffect::setError(const QString &msg) {
    if (m_error != msg) {
        m_error = msg;
        emit errorChanged();
        if (!msg.isEmpty())
            emit shaderError(msg);
    }
}

void ComputeEffect::requestReadback(int binding) {
    std::lock_guard<std::mutex> lock(m_readbackMutex);
    m_pendingReadbacks.push_back(binding);
    update();
}

QSGTextureProvider *ComputeEffect::textureProvider() const {
    if (!m_provider)
        m_provider = new ComputeTextureProvider();
    return m_provider;
}

QSGNode *ComputeEffect::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    QSGNode *root = oldNode ? oldNode : new QSGNode();

    ComputePassNode *computeNode = nullptr;
    QSGSimpleTextureNode *displayNode = nullptr;

    if (!oldNode) {
        textureProvider();
        computeNode = new ComputePassNode(window(), m_provider, this);
        root->appendChildNode(computeNode);

        displayNode = new QSGSimpleTextureNode();
        displayNode->setOwnsTexture(false);
        displayNode->setFiltering(QSGTexture::Linear);
        root->appendChildNode(displayNode);
    } else {
        computeNode = static_cast<ComputePassNode *>(root->firstChild());
        displayNode = static_cast<QSGSimpleTextureNode *>(computeNode->nextSibling());
    }

    QSGTexture *srcTex = nullptr;
    if (m_source && m_source->isTextureProvider())
        srcTex = m_source->textureProvider()->texture();

    computeNode->updateState(m_computeShader, m_uniforms, m_storageBuffers, srcTex, boundingRect().size().toSize(), m_autoWorkGroup, m_workGroupSizeX, m_workGroupSizeY);

    // Readbackキューの転送
    {
        std::lock_guard<std::mutex> lock(m_readbackMutex);
        if (!m_pendingReadbacks.empty()) {
            computeNode->addReadbacks(m_pendingReadbacks);
            m_pendingReadbacks.clear();
        }
    }

    if (QSGTexture *outTex = m_provider ? m_provider->texture() : nullptr) {
        displayNode->setTexture(outTex);
        displayNode->setRect(boundingRect());
        displayNode->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);
    }

    const QString nodeErr = computeNode->m_errorMessage;
    if (!nodeErr.isEmpty() && nodeErr != m_error) {
        QMetaObject::invokeMethod(this, [this, nodeErr]() { setError(nodeErr); }, Qt::QueuedConnection);
    }

    return root;
}

void ComputeEffect::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();
}

} // namespace AviQtl::UI::Effects

#include "compute_effect.moc"
