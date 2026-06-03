#pragma once
#include "../../core/include/effect_model.hpp"
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QVariant>
#include <vector>

namespace AviQtl::UI {

class EffectChainController : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList effectModels READ effectModels WRITE setEffectModels NOTIFY effectModelsChanged)
    Q_PROPERTY(QQuickItem *originalSource READ originalSource WRITE setOriginalSource NOTIFY originalSourceChanged)
    Q_PROPERTY(QQuickItem *outputItem READ outputItem NOTIFY outputItemChanged)
    Q_PROPERTY(int chainLength READ chainLength NOTIFY chainLengthChanged)

  public:
    explicit EffectChainController(QObject *parent = nullptr);

    QVariantList effectModels() const { return m_effectModels; }
    void setEffectModels(const QVariantList &models);

    QQuickItem *originalSource() const { return m_originalSource; }
    void setOriginalSource(QQuickItem *source);

    QQuickItem *outputItem() const;
    int chainLength() const { return static_cast<int>(m_chainItems.size()); }

    Q_INVOKABLE void registerChainItem(int index, QQuickItem *item);
    Q_INVOKABLE void unregisterChainItem(int index);
    Q_INVOKABLE QQuickItem *chainInputFor(int index) const;

  signals:
    void effectModelsChanged();
    void originalSourceChanged();
    void outputItemChanged();
    void chainLengthChanged();

  private:
    void rebuildChain();

    QVariantList m_effectModels;
    QPointer<QQuickItem> m_originalSource;
    std::vector<QPointer<QQuickItem>> m_chainItems;
};

} // namespace AviQtl::UI
