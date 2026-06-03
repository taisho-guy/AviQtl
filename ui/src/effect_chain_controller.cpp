#include "effect_chain_controller.hpp"

namespace AviQtl::UI {

EffectChainController::EffectChainController(QObject *parent) : QObject(parent) {}

void EffectChainController::setEffectModels(const QVariantList &models) {
    if (m_effectModels == models)
        return;
    m_effectModels = models;
    rebuildChain();
    emit effectModelsChanged();
}

void EffectChainController::setOriginalSource(QQuickItem *source) {
    if (m_originalSource == source)
        return;
    m_originalSource = source;
    emit originalSourceChanged();
    emit outputItemChanged();
}

void EffectChainController::rebuildChain() {
    const int newSize = static_cast<int>(m_effectModels.size());
    if (static_cast<int>(m_chainItems.size()) > newSize) {
        m_chainItems.resize(static_cast<std::size_t>(newSize));
    } else {
        while (static_cast<int>(m_chainItems.size()) < newSize)
            m_chainItems.emplace_back(nullptr);
    }
    emit chainLengthChanged();
    emit outputItemChanged();
}

void EffectChainController::registerChainItem(int index, QQuickItem *item) {
    if (index < 0 || index >= static_cast<int>(m_chainItems.size()))
        return;
    m_chainItems[static_cast<std::size_t>(index)] = item;
    emit outputItemChanged();
}

void EffectChainController::unregisterChainItem(int index) {
    if (index < 0 || index >= static_cast<int>(m_chainItems.size()))
        return;
    m_chainItems[static_cast<std::size_t>(index)].clear();
    emit outputItemChanged();
}

QQuickItem *EffectChainController::chainInputFor(int index) const {
    if (index <= 0)
        return m_originalSource.data();
    for (int i = index - 1; i >= 0; --i) {
        const auto &ptr = m_chainItems[static_cast<std::size_t>(i)];
        if (!ptr.isNull())
            return ptr.data();
    }
    return m_originalSource.data();
}

QQuickItem *EffectChainController::outputItem() const {
    for (int i = static_cast<int>(m_chainItems.size()) - 1; i >= 0; --i) {
        const auto &ptr = m_chainItems[static_cast<std::size_t>(i)];
        if (!ptr.isNull())
            return ptr.data();
    }
    return m_originalSource.data();
}

} // namespace AviQtl::UI
