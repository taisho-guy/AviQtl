#pragma once
#include "component_storage.hpp"
#include "entity_id.hpp"
#include <algorithm>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace AviQtl {
namespace Engine {

class World {
public:
  World() : m_nextEntityId(1) {}

  EntityId createEntity() {
    EntityId id = m_nextEntityId++;
    m_entities.push_back(id);
    return id;
  }

  void destroyEntity(EntityId entity) {
    auto it = std::find(m_entities.begin(), m_entities.end(), entity);
    if (it != m_entities.end()) {
      m_entities.erase(it);
    }
    for (auto &[type, storage] : m_storages) {
      storage->remove(entity);
    }
  }

  template <typename T> void addComponent(EntityId entity, T &&component) {
    getOrCreateStorage<T>()->set(entity, std::forward<T>(component));
  }

  template <typename T> T *getComponent(EntityId entity) {
    auto *storage = getStorage<T>();
    return storage ? storage->get(entity) : nullptr;
  }

  template <typename T> const T *getComponent(EntityId entity) const {
    auto *storage = getStorage<T>();
    return storage ? storage->get(entity) : nullptr;
  }

  template <typename T> void removeComponent(EntityId entity) {
    auto *storage = getStorage<T>();
    if (storage) {
      storage->remove(entity);
    }
  }

  template <typename T> bool hasComponent(EntityId entity) const {
    auto *storage = getStorage<T>();
    return storage ? storage->has(entity) : false;
  }

  template <typename T> ComponentStorage<T> *getStorage() {
    auto it = m_storages.find(std::type_index(typeid(T)));
    if (it != m_storages.end()) {
      return static_cast<ComponentStorage<T> *>(it->second.get());
    }
    return nullptr;
  }

  template <typename T> const ComponentStorage<T> *getStorage() const {
    auto it = m_storages.find(std::type_index(typeid(T)));
    if (it != m_storages.end()) {
      return static_cast<const ComponentStorage<T> *>(it->second.get());
    }
    return nullptr;
  }

  const std::vector<EntityId> &entities() const { return m_entities; }

private:
  template <typename T> ComponentStorage<T> *getOrCreateStorage() {
    auto typeIdx = std::type_index(typeid(T));
    auto it = m_storages.find(typeIdx);
    if (it == m_storages.end()) {
      auto storage = std::make_unique<ComponentStorage<T>>();
      auto *rawPtr = storage.get();
      m_storages[typeIdx] = std::move(storage);
      return rawPtr;
    }
    return static_cast<ComponentStorage<T> *>(it->second.get());
  }

  EntityId m_nextEntityId;
  std::vector<EntityId> m_entities;
  std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>>
      m_storages;
};

} // namespace Engine
} // namespace AviQtl
