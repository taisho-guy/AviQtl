#include "effect_data.hpp"
#include "effect_model.hpp"

namespace Rina::UI {

EffectData effectDataFromModel(const EffectModel &model) {
    EffectData d;
    d.id = model.id();
    d.name = model.name();
    d.kind = model.kind();
    d.categories = model.categories();
    d.qmlSource = model.qmlSource();
    d.uiDefinition = model.uiDefinition();
    d.enabled = model.isEnabled();
    d.params = model.params();
    d.keyframeTracks = model.keyframeTracks();
    return d;
}

EffectModel *effectModelFromData(const EffectData &data, QObject *parent) {
    auto *m = new EffectModel(data.id, data.name, data.kind, data.categories, data.params, data.qmlSource, data.uiDefinition, parent);
    m->setEnabled(data.enabled);
    m->setKeyframeTracks(data.keyframeTracks);
    // syncTrackEndpoints(durationFrames) は呼び出し元で別途呼ぶ
    return m;
}

} // namespace Rina::UI
