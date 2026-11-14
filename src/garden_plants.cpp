#include "garden_plants.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace royal_garden {
namespace {
// Short names for the three basic transformations (L-4-1 Transformations),
// so that a chain of them reads like the lecture's formulas.
glm::mat4 T(float x, float y, float z) { return glm::translate(glm::mat4(1), glm::vec3(x, y, z)); }
glm::mat4 S(float x, float y, float z) { return glm::scale(glm::mat4(1), glm::vec3(x, y, z)); }
glm::mat4 S(float s) { return S(s, s, s); }
glm::mat4 RotY(float degrees) { return glm::rotate(glm::mat4(1), glm::radians(degrees), glm::vec3(0, 1, 0)); }
glm::mat4 RotZ(float degrees) { return glm::rotate(glm::mat4(1), glm::radians(degrees), glm::vec3(0, 0, 1)); }
glm::mat4 RotX(float degrees) { return glm::rotate(glm::mat4(1), glm::radians(degrees), glm::vec3(1, 0, 0)); }
}

PlantKit::PlantKit() {
    cylinder(limb, 1.0f, 1.0f, shape.shrink, 10, Bark);  // top radius = the child's bottom radius
    sphere(leafBall, 1.0f, 6, 10, Leaf);
    sphere(violetBall, 1.0f, 6, 10, LeafViolet);
    sphere(hedgeBall, 1.0f, 5, 10, Hedge);
    box(hedgeBlock, {0, 0, -0.5f}, {1, 1, 0.5f}, Hedge);
    cylinder(stem, 1.0f, 1.0f, 0.6f, 5, Stem);
    sphere(lavenderPetal, 1.0f, 4, 6, PetalLavender);
    sphere(whitePetal, 1.0f, 4, 6, PetalWhite);
    sphere(heart, 1.0f, 4, 6, FlowerHeart);
}

// ---------------------------------------------------------------------------
// Trees (L-9 Hierarchical Modeling)
// ---------------------------------------------------------------------------

void PlantKit::tree(Vertices& v, const glm::mat4& instance) const {
    growBranch(v, instance, shape.trunkLength, shape.trunkRadius, shape.levels, 0);
}

// One branch and everything that grows from it.
// `m` is the parent's matrix: it moves this branch's local coordinates into the
// world. Passing it to each child as a NEW matrix works like PushMatrix (the
// parent's copy is untouched); returning from the call is PopMatrix (L-9 p32-33, p41).
void PlantKit::growBranch(Vertices& v, const glm::mat4& m, float length, float radius,
                          int level, int index) const {
    // 1. This branch: the unit limb scaled to its length and thickness.
    place(v, limb, m * S(radius, length, radius));

    // 2. The tip of this branch, in its own coordinates: (0, length, 0).
    const glm::mat4 tip = m * T(0, length, 0);

    // 3. A leaf node of the tree: leaves. Leaves hang with gravity, not along the
    //    twig, so we keep only two things from the tip's matrix:
    //    - its position: the last column of the matrix, tip[3];
    //    - the tree's size: the length of a column (rotations do not change lengths).
    if (level == 0) {
        const glm::vec3 tipPos(tip[3]);
        const float size = glm::length(glm::vec3(m[0]));
        const glm::mat4 hang = T(tipPos.x, tipPos.y, tipPos.z) * S(size);
        const unsigned seed = unsigned(std::abs(tipPos.x) * 37.0f + std::abs(tipPos.z) * 91.0f
                                       + tipPos.y * 13.0f) + unsigned(index);
        // A wide, flat cluster ...
        place(v, leafBall, hang * T(0, -0.1f, 0) * S(1.4f, 0.7f, 1.4f));
        // ... and five strands hanging from its rim, like a weeping willow.
        for (int j = 0; j < 5; ++j) {
            const float angle = 72.0f * j + 40.0f * random01(seed + j);
            const float drop = 0.9f + 0.8f * random01(seed * 3 + j);   // strand length, metres
            const bool violet = random01(seed * 7 + j) < 0.22f;         // about 1 strand in 5
            place(v, violet ? violetBall : leafBall,
                  hang * RotY(angle) * T(1.0f, -0.35f - drop * 0.5f, 0) * S(0.22f, drop * 0.5f, 0.22f));
        }
        return;
    }

    // 4. Children: spin round the branch (Y), lean out (Z), grow from the tip.
    //    Outer levels lean more, so the twigs arc over and down.
    const float lean = shape.spread + shape.droop * float(shape.levels - level);
    for (int k = 0; k < shape.children; ++k) {
        const float turn = 360.0f * k / shape.children + 25.0f * level;
        const glm::mat4 child = tip * RotY(turn) * RotZ(lean);
        growBranch(v, child, length * shape.shrink, radius * shape.shrink, level - 1, k);
    }
}

// ---------------------------------------------------------------------------
// Flowers, bushes and hedges
// ---------------------------------------------------------------------------

// Flower hierarchy: stem -> head -> 5 petals + heart.
void PlantKit::flower(Vertices& v, const glm::mat4& instance, FlowerColour colour,
                      unsigned seed) const {
    const float height = 0.45f + 0.35f * random01(seed);        // 0.45 .. 0.80 m
    const float lean = -8.0f + 16.0f * random01(seed + 1);      // stems are never quite straight
    const glm::mat4 base = instance * RotZ(lean);
    place(v, stem, base * S(0.02f, height, 0.02f));

    // The head sits on the stem's tip and turns 40 degrees towards the camera (+Z),
    // so the side view sees the open flower, not just its edge.
    const float headSize = 1.0f + 0.4f * random01(seed + 2);
    const glm::mat4 head = base * T(0, height, 0) * RotX(40.0f) * S(headSize);
    const Vertices& petal = colour == FlowerColour::Lavender ? lavenderPetal : whitePetal;
    const float twist = 72.0f * random01(seed + 3);
    for (int k = 0; k < 5; ++k)   // 5 petals, 72 degrees apart; each is a flattened sphere
        place(v, petal, head * RotY(twist + 72.0f * k) * T(0.11f, 0, 0) * S(0.12f, 0.025f, 0.065f));
    place(v, heart, head * S(0.045f, 0.03f, 0.045f));
}

// A bush: four leaf balls round a centre, and one on top.
void PlantKit::bush(Vertices& v, const glm::mat4& instance, unsigned seed) const {
    for (int k = 0; k < 4; ++k) {
        const float angle = 90.0f * k + 50.0f * random01(seed + k);
        const float r = 0.55f + 0.25f * random01(seed * 5 + k);
        place(v, leafBall, instance * RotY(angle) * T(0.45f, 0.45f, 0) * S(r, r * 0.8f, r));
    }
    place(v, leafBall, instance * T(0, 0.8f, 0) * S(0.6f, 0.5f, 0.6f));
}

// A hedge is built along its own local X axis, then turned to point from
// `from` to `to`. rotate about Y by angle a sends local X to (cos a, 0, -sin a),
// so a = atan2(-dz, dx) makes local X point along the direction (dx, dz).
void PlantKit::hedge(Vertices& v, glm::vec2 from, glm::vec2 to, float height, float depth) const {
    const glm::vec2 d = to - from;
    const float length = glm::length(d);
    const glm::mat4 frame = T(from.x, 0, from.y)
        * glm::rotate(glm::mat4(1), std::atan2(-d.y, d.x), glm::vec3(0, 1, 0));
    const float body = height * 0.85f;
    place(v, hedgeBlock, frame * S(length, body, depth));
    // A rounded, clipped top: a row of squashed leaf balls along the upper edge.
    const int balls = std::max(2, int(length / 0.7f));
    const float step = length / balls;
    for (int i = 0; i < balls; ++i)
        place(v, hedgeBall, frame * T(step * (i + 0.5f), body, 0)
                                  * S(step * 0.75f, height * 0.2f, depth * 0.55f));
}
}
