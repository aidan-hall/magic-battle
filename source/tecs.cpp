#include <algorithm>
#include <cassert>
#include "tecs.hpp"
#include <typeindex>

using namespace Tecs;

Coordinator::Coordinator() {
  // A Signature is a Component that every Entity implicitly has,
  // identifying what other Components it has.
  nextComponentId = -1;
  registerComponent<ComponentMask>();
}

Entity Coordinator::newEntity() {
  return entity_manager.allocate();
}
void Coordinator::destroyEntity(Entity e) {
  getComponent<ComponentMask>(e).reset();
  for (auto &interesting : interests.interestingSets) {
    interesting.erase(e);
  }

  recycledEntities.enqueue(e);
}

void Coordinator::queueDestroyEntity(Entity e) {
  pendingDestructions.enqueue(e);
}

void Coordinator::destroyQueued() {
  while (!pendingDestructions.empty()) {
    Entity e = pendingDestructions.front();
    pendingDestructions.dequeue();
    destroyEntity(e);
  }
}

ComponentId Coordinator::registerComponent(const std::type_index &typeIndex) {
  assert(not componentIds.contains(typeIndex));

  const auto myComponentId = nextComponentId;
  componentIds[typeIndex] = myComponentId;
  nextComponentId += 1;

  return myComponentId;
}

void Coordinator::removeComponent(Entity e, ComponentId c) {
  auto &s = getComponent<ComponentMask>(e);
  ComponentMask old = s;
  s.reset(c);

  for (InterestedId system = 0; system < interests.interestConditions.size();
       ++system) {
    const auto &interestsignature = interests.interestConditions[system];
    if (Tecs::InterestManager::isInteresting(old, interestsignature) &&
        !Tecs::InterestManager::isInteresting(s, interestsignature)) {
      auto &interesting = interests.interestingSets[system];
      interesting.erase(e);
    }
  }
}

void Coordinator::addComponent(Entity e, ComponentId c) {
  auto &s = getComponent<ComponentMask>(e);
  const auto old = s;
  s.set(c);

  for (InterestedId interest = 0;
       interest < interests.interestConditions.size(); ++interest) {
    const auto &systemSignature = interests.interestConditions[interest];
    if (Tecs::InterestManager::isInteresting(s, systemSignature) &&
        !Tecs::InterestManager::isInteresting(old, systemSignature)) {
      auto &interesting = interests.interestingSets[interest];
      interesting.insert(e);
    }
  }
}
