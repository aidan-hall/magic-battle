#ifndef TECS_H
#define TECS_H

#include <bitset>
#include <cassert>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <set>
#include <span>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "unusual_circular_queue.hpp"

// 'Inspired' by https://austinmorlan.com/posts/entity_component_system

namespace Tecs {

using Entity = std::size_t;
using ComponentId = std::uint8_t;
// This value means a Signature should fit in a long/64-bit integer.
static constexpr std::size_t MAX_COMPONENTS = 64;
using ComponentMask = std::bitset<MAX_COMPONENTS>;
struct InterestCondition {
  ComponentMask include;
  ComponentMask exclude = 0;
};

inline InterestCondition
componentsSignature(const std::vector<ComponentId> &components,
                    const std::vector<ComponentId> &excluded_components = {}) {
  InterestCondition s;
  for (auto &c : components) {
    s.include.set(c);
  }
  for (auto &c : excluded_components) {
    s.exclude.set(c);
  }
  return s;
}

using InterestedId = std::size_t;

struct InterestManager {
  InterestedId nextInterested = 0;
  std::vector<std::set<Entity>> interestingSets;
  std::vector<InterestCondition> interestConditions;
  std::unordered_map<InterestedId, size_t> interestCount;

  InterestedId
  registerInterests(const std::vector<InterestCondition> &conditions) {
    const InterestedId interest_id = nextInterested;
    interestCount[interest_id] = conditions.size();
    for (auto condition : conditions) {
      interestingSets.push_back({});
      interestConditions.push_back(condition);
      nextInterested++;
    }
    return interest_id;
  }

  // Whether an entity with the given ComponentMask would be interesting under
  // the given condition.
  static inline bool isInteresting(const ComponentMask &mask,
                                   const InterestCondition &condition) {
    return ((mask & condition.include) == condition.include) &&
           ((mask & condition.exclude) == 0);
  }

  std::span<std::set<Entity>> interestsOf(InterestedId id) {
    return std::span{&interestingSets[id], interestCount.at(id)};
  }
};

// struct System;
// template <class T>
// concept system = std::derived_from<System, T>;

struct Coordinator {

  std::unordered_map<std::type_index, ComponentId> componentIds;

  Entity nextEntity = 0;
  unusual::circular_queue<Entity, 200> recycledEntities;
  unusual::circular_queue<Entity, 5> pendingDestructions;

  ComponentId nextComponentId;

  InterestManager interests;

  Coordinator();

  Entity newEntity();

  // DO NOT CALL IN A SYSTEM
  void destroyEntity(Entity e);

  // Queues Entity for destruction with next call to destroyQueued(). "Safe" in
  // Systems.
  void queueDestroyEntity(Entity e);

  // DO NOT CALL IN A SYSTEM
  void destroyQueued();

  ComponentId registerComponent(const std::type_index &typeIndex);

  template <typename Component> inline ComponentId registerComponent() {
    const auto id = registerComponent(std::type_index(typeid(Component)));
    // Static method-local variables might not be instance-local: Remove
    // previous data.
    getComponents<Component>().clear();
    return id;
  }

  template <typename Component> inline ComponentId componentId() {
    const auto c = componentIds.at(std::type_index(typeid(Component)));
    return c;
  }

  void addComponent(Entity e, ComponentId c);

  template <typename Component> inline void addComponent(Entity e) {
    addComponent(e, componentId<Component>());
  }

  // Terminating case for variadic addComponents
  inline auto addComponents(Entity e) {
    std::ignore = e;
    return std::tuple<>{};
  }

  // Variadic template to add components.
  template <typename Component, typename... OtherComponents>
  inline auto addComponents(Entity e, Component value,
                            OtherComponents... others) {
    addComponent<Component>(e);
    auto &component = getComponent<Component>(e);
    component = value;
    return std::tuple_cat(std::tuple<Component &>{component},
                          addComponents(e, others...));
  }

  template <typename Component, typename... OtherComponents>
  inline ComponentMask componentMask() {
    ComponentMask mask;
    if constexpr (sizeof...(OtherComponents) > 0) {
      mask = componentMask<OtherComponents...>();
    } else {
      mask = {};
    }
    mask.set(componentId<Component>());
    return mask;
  }

  void removeComponent(Entity e, ComponentId c);

  template <typename Component> inline void removeComponent(Entity e) {
    removeComponent(e, componentId<Component>());
  }

  template <typename Component> inline std::vector<Component> &getComponents() {
    // The ugly foundation upon which this entire 'type safe' ECS framework is
    // based.
    static std::vector<Component> components;
    return components;
  }

  template <typename Component> inline Component &getComponent(Entity e) {
    // TODO: Add assertion that Entity exists.
    assert(e < nextEntity);
    assert(hasComponent<Component>(e));

    std::vector<Component> &components = getComponents<Component>();
    if (components.size() <= e) {
      components.resize(e + 1);
    }

    return components[e];
  }

  template <typename Component> inline bool hasComponent(Entity e) {
    return getComponent<ComponentMask>(e).test(componentId<Component>());
  }
};

// Every Entity implicitly has a Signature.
template <> inline bool Coordinator::hasComponent<ComponentMask>(Entity e) {
  std::ignore = e;
  return true;
}

// This cannot be float-based, due to the high required precision(?).
using Duration = std::chrono::duration<double>;
using TimePoint =
    std::chrono::time_point<std::chrono::high_resolution_clock, Duration>;

} // namespace Tecs

#endif // TECS_H
