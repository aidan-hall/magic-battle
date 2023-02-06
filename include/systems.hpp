#ifndef SYSTEMS_H
#define SYSTEMS_H

#include "tecs-system.hpp"

Tecs::SingleEntitySetSystem::Function apply_velocity;
Tecs::SingleEntitySetSystem::Function draw_sprites;
Tecs::SingleEntitySetSystem::Function following_ai;
Tecs::SingleEntitySetSystem::Function circular_collision_detection;

Tecs::PerEntitySystem::Function sprite_id_reclamation;
Tecs::PerEntitySystem::Function affine_index_reclamation;

Tecs::PerEntitySystem::Function affine_rendering;

#endif /* SYSTEMS_H */
