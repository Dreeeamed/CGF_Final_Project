#pragma once
// Zones C and D: trees, flowers, bushes and hedges.
#include "garden_primitives.h"

namespace royal_garden {

// The shape of one kind of tree. Every number is relative to the parent part.
struct TreeShape {
    float trunkLength = 5.0f;   // metres, before the instance is scaled
    float trunkRadius = 0.35f;
    int levels = 3;             // trunk -> branches -> twigs -> leaf clusters
    int children = 3;           // branches growing from each tip
    float shrink = 0.68f;       // a child is this much shorter and thinner
    float spread = 34.0f;       // degrees between the first branches and the trunk
    float droop = 16.0f;        // extra lean per level: outer twigs bend down (weeping)
};

enum class FlowerColour { Lavender, White };

// The "symbol table" of L-9 Hierarchical Modeling p4: every prototype part is
// built ONCE here, in its own local coordinates. The functions below only place
// copies of these parts with instance transformations.
class PlantKit {
public:
    PlantKit();
    // A whole tree. `instance` places it: translate, turn, scale.
    void tree(Vertices& v, const glm::mat4& instance) const;
    // A flower standing on the origin of `instance`; `seed` picks its height and tilt.
    void flower(Vertices& v, const glm::mat4& instance, FlowerColour colour, unsigned seed) const;
    // A round bush made of a few overlapping leaf balls.
    void bush(Vertices& v, const glm::mat4& instance, unsigned seed) const;
    // A clipped hedge from `from` to `to` on the ground (XZ), any direction.
    void hedge(Vertices& v, glm::vec2 from, glm::vec2 to, float height, float depth) const;

private:
    void growBranch(Vertices& v, const glm::mat4& m, float length, float radius,
                    int level, int index) const;
    TreeShape shape;
    Vertices limb;          // 1 m long, radius 1 at the bottom: scaled for every branch
    Vertices leafBall;      // unit sphere of leaves
    Vertices violetBall;    // the same, violet
    Vertices hedgeBall;     // unit sphere, dark clipped-hedge leaves
    Vertices hedgeBlock;    // unit cube (0..1 in X and Y, -0.5..0.5 in Z)
    Vertices stem;          // 1 m long thin stem
    Vertices lavenderPetal; // unit sphere, flattened into a petal when placed
    Vertices whitePetal;
    Vertices heart;         // the gold centre of a flower
};
}
