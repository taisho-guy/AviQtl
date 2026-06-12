#pragma once
#include "entity_id.hpp"
#include <any>
#include <memory>
#include <unordered_map>

namespace AviQtl {
namespace Engine {

class IComponentStorage {
public:
  virtual ~IComponentStorage() = default;
  virtual void remove(EntityId entity) = 0;
};

template <typename T> class ComponentStorage : public IComponentStorage {
public:
  void set(EntityId entity, const T &component) {
    m_components[entity] = component;
  }

  void set(EntityId entity, T &&component) {
    m_components[entity] = std::move(component);
  }

  T *get(EntityId entity) {
    auto it = m_components.find(entity);
    if (it != m_components.end()) {
      return &it->second;
    }
    return nullptr;
  }

  const T *get(EntityId entity) const {
    auto it = m_components.find(entity);
    if (it != m_components.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void remove(EntityId entity) override { m_components.erase(entity); }

  bool has(EntityId entity) const {
    return m_components.find(entity) != m_components.end();
  }

  const std::unordered_map<EntityId, T> &all() const { return m_components; }

  std::unordered_map<EntityId, T> &all() { return m_components; }

private:
  std::unordered_map<EntityId, T> m_components;
};

} // namespace Engine
} // namespace AviQtl
