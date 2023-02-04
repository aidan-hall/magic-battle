#ifndef COMPONENTS_H
#define COMPONENTS_H
#include "ndspp.hpp"

struct Vec3 {
  nds::fix x;
  nds::fix y;
  nds::fix z;
};

struct Position {
  Vec3 pos;
  int32_t rotation;
};

struct Velocity {
  Vec3 v;
};

struct SpriteInfo {
  int id;
  // Width divided by 2
  nds::fix width2;
  // Height divided by 2
  nds::fix height2;
};

struct Zombie {};

struct RenderingSystemTag {};

struct PhysicsSystemTag {};

#endif /* COMPONENTS_H */
