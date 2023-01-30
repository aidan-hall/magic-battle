#include "tecs.hpp"
#include <span>
#include <functional>

namespace Tecs {
  struct SingleEntitySetSystem {
    std::function<void(Coordinator &, const std::unordered_set<Entity> &)> run;
  };

  struct MultipleEntitySetSystem {
    std::function<void(Coordinator &,
      const std::span<const std::unordered_set<Entity>>)>
    run;
  };

  struct PerEntitySystem {
    std::function<void(Coordinator &, const Entity)> run;
  };

  struct InterestedClient {
    InterestedId id;
  };

  void registerSystemComponents(Coordinator&);
  void runSystems(Coordinator&, const InterestedId);

  InterestedId makeSystemInterest(Coordinator&, const ComponentMask& mask, const ComponentMask& exclude = {});
}
