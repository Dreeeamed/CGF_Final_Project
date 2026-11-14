#pragma once
// The small solids everything in the garden is built from (solid modeling),
// plus the two tools that combine them: `place` and `random01`.
#include "garden_scene.h"
#include <glm/glm.hpp>
#include <vector>

namespace royal_garden {
using Vertices = std::vector<Vertex>;
constexpr float pi = 3.14159265359f;

// One flat triangle. Corners counter-clockwise seen from the front (right-hand rule).
void triangle(Vertices& v, glm::vec3 a, glm::vec3 b, glm::vec3 c, Material m,
              glm::vec2 ua = {0, 0}, glm::vec2 ub = {1, 0}, glm::vec2 uc = {0.5f, 1});
// Two triangles, a-b-c and a-c-d.
void quad(Vertices& v, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, Material m);
// An axis-aligned box from its minimum corner `lo` to its maximum corner `hi`.
void box(Vertices& v, glm::vec3 lo, glm::vec3 hi, Material m);
// A regular N-sided polygon pushed up into a solid, standing on `base`.
void prism(Vertices& v, glm::vec3 base, float radius, float height, int sides, Material m);
// A tapered tube up local +Y from the origin: radius r0 at the bottom, r1 at the top.
void cylinder(Vertices& v, float height, float r0, float r1, int segments, Material m);
// A sphere of radius r round the origin.
void sphere(Vertices& v, float r, int rings, int segments, Material m);

// Copy a part built in its own local coordinates into `out`, moved by matrix m
// (instance transformation, L-9 Hierarchical Modeling p3).
void place(Vertices& out, const Vertices& part, const glm::mat4& m);

// A repeatable "random" number in [0, 1) made from an integer seed.
// The same seed always gives the same number, so the garden looks the same every run.
float random01(unsigned seed);
}
