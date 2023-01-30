#include "tecs-system.hpp"
#include "tecs.hpp"

namespace Tecs {

void registerSystemComponents(Coordinator &ecs) {
  ecs.registerComponent<InterestedClient>();
  ecs.registerComponent<SingleEntitySetSystem>();
  ecs.registerComponent<PerEntitySystem>();
  ecs.registerComponent<MultipleEntitySetSystem>();
}

void runSystems(Coordinator &ecs, const InterestedId interested_id) {
  auto interesting_sets = ecs.interests.interestsOf(interested_id);
  assert(interesting_sets.size() == 3);

  // Per Entity Systems:
  for (const auto system : interesting_sets[0]) {
    const auto run = ecs.getComponent<PerEntitySystem>(system).run;
    for (const auto entity : ecs.interests.interestsOf(
             ecs.getComponent<InterestedClient>(system).id)[0]) {
      run(ecs, entity);
    }
  }

  // Single Entity Set Systems:
  for (const auto system : interesting_sets[1]) {
    ecs.getComponent<SingleEntitySetSystem>(system).run(
        ecs, ecs.interests.interestsOf(
                 ecs.getComponent<InterestedClient>(system).id)[0]);
  }

  // Multiple Entity Set Systems:
  for (const auto system : interesting_sets[2]) {
    ecs.getComponent<MultipleEntitySetSystem>(system).run(
        ecs, ecs.interests.interestsOf(
                 ecs.getComponent<InterestedClient>(system).id));
  }
}

InterestedId makeSystemInterest(Coordinator &ecs, const ComponentMask &mask,
                                const ComponentMask &exclude) {
  return ecs.interests.registerInterests(
      {{ecs.componentMask<PerEntitySystem>() | mask, exclude},
       {ecs.componentMask<SingleEntitySetSystem>() | mask, exclude},
       {ecs.componentMask<MultipleEntitySetSystem>() | mask, exclude}});
}

} // namespace Tecs
