#pragma once
// Zone A and E decorations: benches, lanterns and gothic pillars.
#include "garden_primitives.h"

namespace royal_garden {

// A gothic stone bench. Local coordinates: the seat runs along X (2.1 m), the
// sitter looks towards +Z, the bench stands on y = 0. `instance` places it.
void bench(Vertices& v, const glm::mat4& instance);

// A gothic lantern on an iron post, about 2.7 m tall, standing on y = 0.
void lantern(Vertices& v, const glm::mat4& instance);
}
