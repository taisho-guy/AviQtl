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
// QVariantMap を経由せず、生バイト列のコピーのみで済む
void ComputeEffect::setStorageBufferRaw(const QString &name, int binding, const void *data, qsizetype byteSize) {
    // 既存エントリの上書きチェック
    for (auto &entry : m_rawSSBOs) {
        if (entry.name == name) {
            if (entry.data.size() == byteSize && std::memcmp(entry.data.constData(), data, static_cast<std::size_t>(byteSize)) == 0)
                return; // 変更なし
            entry.binding = binding;
            entry.data = QByteArray(static_cast<const char *>(data), static_cast<qsizetype>(byteSize));
            m_dirty = true;
            update();
            return;
        }
    }
    // 新規エントリ
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
    // フェーズ6: m_rawSSBOs が存在する場合はそちらを優先し QVariantMap パスをバイパス
    // m_rawSSBOs が空の場合のみ m_storageBuffers (旧来パス) を使用する
    // 実際のGPUバッファ転送は updatePaintNode から QSGRenderNode 経由で行う
}

auto ComputeEffect::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) -> QSGNode * {
    if (!m_enabled) {
        delete oldNode;
        return nullptr;
    }

    if (!m_dirty)
        return oldNode;

    m_dirty = false;

    // TODO: QSGRenderNode (Vulkan Compute) 実装を差し込む場所
    // 以下は暫定スタブ。実際の RHI バインディングはフェーズ7以降で実装する。

    // 1. m_rawSSBOs を走査 → 各エントリを SSBO binding に転送
    //    → glBindBuffer / vkCmdCopyBuffer 相当
    for (const auto &entry : m_rawSSBOs) {
        if (!entry.data.isEmpty()) {
            // binding=entry.binding, data=entry.data.constData(), size=entry.data.size()
            // (実RHI呼び出し省略)
        }
    }

    // 2. m_storageBuffers (旧来 QVariantMap パス)
    if (m_rawSSBOs.isEmpty() && !m_storageBuffers.isEmpty()) {
        for (auto it = m_storageBuffers.begin(); it != m_storageBuffers.end(); ++it) {
            QByteArray bytes = ssboToBytes(it.value().toMap());
            // (実RHI呼び出し省略)
            Q_UNUSED(bytes);
        }
    }

    return oldNode;
}

} // namespace AviQtl::UI::Effects
