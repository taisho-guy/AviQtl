#include "compute_effect.hpp"
#include <QDebug>
#include <QSGNode>
#include <algorithm>
#include <cstring>

namespace AviQtl::UI::Effects {

ComputeEffect::ComputeEffect(QQuickItem *parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); }

ComputeEffect::~ComputeEffect() = default;

void ComputeEffect::setParams(const QVariantMap &params) {
    if (m_params == params)
        return;
    m_params = params;
    m_dirty = true;
    emit paramsChanged();
    update();
}

void ComputeEffect::setStorageBuffers(const QVariantMap &buffers) {
    if (m_storageBuffers == buffers)
        return;
    m_storageBuffers = buffers;
    m_dirty = true;
    emit storageBuffersChanged();
    update();
}

void ComputeEffect::setEnabled(bool enabled) {
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    m_dirty = true;
    emit enabledChanged();
    update();
}

void ComputeEffect::setShaderPath(const QString &path) {
    if (m_shaderPath == path)
        return;
    m_shaderPath = path;
    m_dirty = true;
    emit shaderPathChanged();
    update();
}

// フェーズ6: ECS writeSSBOLayout → GPU への直接ゼロコピーパス
// フェーズ7: 呼び出し元は GpuClipSoA を SSBO_BINDING_CLIP (=0) に渡す
//   使用例:
//     GpuClipSoA gpuClipSoA;
//     ECS::instance().writeSSBOLayout(gpuClipSoA);
//     effect->setStorageBufferRaw("clips", SSBO_BINDING_CLIP, &gpuClipSoA, sizeof(GpuClipSoA));
void ComputeEffect::setStorageBufferRaw(const QString &name, int binding, const void *data, qsizetype byteSize) {
    for (auto &entry : m_rawSSBOs) {
        if (entry.name == name) {
            if (entry.data.size() == byteSize && std::memcmp(entry.data.constData(), data, static_cast<std::size_t>(byteSize)) == 0)
                return;
            entry.binding = binding;
            entry.data = QByteArray(static_cast<const char *>(data), static_cast<qsizetype>(byteSize));
            m_dirty = true;
            update();
            return;
        }
    }
    m_rawSSBOs.push_back(RawSSBOEntry{name, binding, QByteArray(static_cast<const char *>(data), static_cast<qsizetype>(byteSize))});
    m_dirty = true;
    update();
}

// QVariantMap → バイト列変換 (旧来の互換パス。フェーズ6以降は setStorageBufferRaw を優先)
auto ComputeEffect::ssboToBytes(const QVariantMap &bufferData) -> QByteArray {
    QByteArray result;
    result.reserve(static_cast<qsizetype>(bufferData.size()) * static_cast<qsizetype>(sizeof(float)));
    for (const QVariant &v : bufferData.values()) {
        if (v.canConvert<float>()) {
            float f = v.toFloat();
            result.append(reinterpret_cast<const char *>(&f), sizeof(float));
        } else if (v.canConvert<int>()) {
            int i = v.toInt();
            result.append(reinterpret_cast<const char *>(&i), sizeof(int));
        }
    }
    return result;
}

void ComputeEffect::updateState() {
    // フェーズ7: m_rawSSBOs が存在する場合は QVariantMap パスをバイパスする
    // フェーズ8 で QRhiComputePipeline へ接続する際はここを起点にする
}

auto ComputeEffect::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) -> QSGNode * {
    if (!m_enabled) {
        delete oldNode;
        return nullptr;
    }

    if (!m_dirty)
        return oldNode;

    m_dirty = false;

    // フェーズ7 完了: GpuClipSoA が SSBO_BINDING_CLIP (=0) に乗って到達する
    // フェーズ8 でここに QSGRenderNode (Vulkan Compute) の実装を差し込む
    //   1. QRhiBuffer を確保し m_rawSSBOs を vkCmdCopyBuffer でアップロード
    //   2. QRhiComputePipeline に SSBO_BINDING_CLIP をバインド
    //   3. vkCmdDispatch で Compute Shader を起動

    for (const auto &entry : m_rawSSBOs) {
        if (!entry.data.isEmpty()) {
            // binding=entry.binding, data=entry.data.constData(), size=entry.data.size()
            // (フェーズ8: 実 RHI 呼び出しに置き換える)
        }
    }

    if (m_rawSSBOs.isEmpty() && !m_storageBuffers.isEmpty()) {
        for (auto it = m_storageBuffers.begin(); it != m_storageBuffers.end(); ++it) {
            QByteArray bytes = ssboToBytes(it.value().toMap());
            Q_UNUSED(bytes);
        }
    }

    return oldNode;
}

} // namespace AviQtl::UI::Effects
