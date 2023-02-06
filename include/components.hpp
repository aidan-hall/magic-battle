#ifndef COMPONENTS_H
#define COMPONENTS_H
#include "ndspp.hpp"
#include "tecs.hpp"
#include <bitset>
#include <cstdint>
#include <functional>

struct Vec3 {
  nds::fix x;
  nds::fix y;
  nds::fix z;
};

struct Position {
  Vec3 pos;
};

struct Velocity {
  Vec3 v;
};

struct Affine {
  int32_t affine_index;
  int32_t rotation;
  int32_t scale;
};

struct SpriteInfo {
  int id;
  // Width divided by 2
  nds::fix width2;
  // Height divided by 2
  nds::fix height2;
};

struct Zombie {};

struct Following {
  Tecs::Entity target;
  nds::fix speed;
};

struct RenderingSystemTag {};

struct PhysicsSystemTag {};

struct AdminSystemTag {};
struct CleanupSystemTag {};

struct DeathMark {};

struct Health {
  int8_t value;
};

struct Collision {
  std::bitset<8> mask;          // Layers this entity collides onto.
  std::bitset<8> layer;         // Layers this entity is on.
  nds::fix radius_squared;
  std::function<void(Tecs::Coordinator&, Tecs::Entity, Tecs::Entity)> callback;
};

struct TimerCallback {
  uint32_t time;
  std::function<void(Tecs::Coordinator&, const Tecs::Entity)> callback;
};

enum CollisionLayers {
  PLAYER_LAYER = 1 << 0,
  ZOMBIE_LAYER = 1 << 1,
  PLAYER_ATTACK_LAYER = 1 << 2,
};

#endif /* COMPONENTS_H */
