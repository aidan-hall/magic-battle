#ifndef SYSTEMS_H
#define SYSTEMS_H

#include "tecs-system.hpp"

Tecs::SingleEntitySetSystem::Function apply_velocity;
Tecs::SingleEntitySetSystem::Function draw_sprites;
Tecs::SingleEntitySetSystem::Function zombie_touch_movement;
Tecs::SingleEntitySetSystem::Function circular_collision_detection;

#endif /* SYSTEMS_H */
