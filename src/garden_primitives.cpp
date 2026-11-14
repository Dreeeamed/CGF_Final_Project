#include "garden_primitives.h"
#include <cmath>

namespace royal_garden {

void triangle(Vertices& v, glm::vec3 a, glm::vec3 b, glm::vec3 c, Material m,
              glm::vec2 ua, glm::vec2 ub, glm::vec2 uc) {
    const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
    const float id = float(m);
    v.insert(v.end(), {{a, n, ua, id}, {b, n, ub, id}, {c, n, uc, id}});
}

void quad(Vertices& v, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, Material m) {
    triangle(v, a, b, c, m, {0, 0}, {1, 0}, {1, 1});
    triangle(v, a, c, d, m, {0, 0}, {1, 1}, {0, 1});
}

void box(Vertices& v, glm::vec3 lo, glm::vec3 hi, Material m) {
    quad(v, {lo.x,lo.y,hi.z}, {hi.x,lo.y,hi.z}, {hi.x,hi.y,hi.z}, {lo.x,hi.y,hi.z}, m); // front +Z
    quad(v, {hi.x,lo.y,lo.z}, {lo.x,lo.y,lo.z}, {lo.x,hi.y,lo.z}, {hi.x,hi.y,lo.z}, m); // back  -Z
    quad(v, {lo.x,lo.y,lo.z}, {lo.x,lo.y,hi.z}, {lo.x,hi.y,hi.z}, {lo.x,hi.y,lo.z}, m); // left  -X
    quad(v, {hi.x,lo.y,hi.z}, {hi.x,lo.y,lo.z}, {hi.x,hi.y,lo.z}, {hi.x,hi.y,hi.z}, m); // right +X
    quad(v, {lo.x,hi.y,hi.z}, {hi.x,hi.y,hi.z}, {hi.x,hi.y,lo.z}, {lo.x,hi.y,lo.z}, m); // top   +Y
    quad(v, {lo.x,lo.y,lo.z}, {hi.x,lo.y,lo.z}, {hi.x,lo.y,hi.z}, {lo.x,lo.y,hi.z}, m); // bottom -Y
}

// Corner i sits at angle 2*pi*i/N (polar coordinates). The first corner is turned by
// half a step, so a flat side, not a sharp corner, faces the camera. No bottom face:
// prisms in the garden always stand on the ground.
void prism(Vertices& v, glm::vec3 base, float radius, float height, int sides, Material m) {
    const glm::vec3 top = base + glm::vec3(0, height, 0);
    for (int i = 0; i < sides; ++i) {
        const float a0 = 2 * pi * (i + 0.5f) / sides;
        const float a1 = 2 * pi * (i + 1.5f) / sides;
        const glm::vec3 r0(radius * std::cos(a0), 0, radius * std::sin(a0));
        const glm::vec3 r1(radius * std::cos(a1), 0, radius * std::sin(a1));
        quad(v, base + r1, base + r0, top + r0, top + r1, m); // side wall
        triangle(v, top, top + r1, top + r0, m);              // one "pie slice" of the top
    }
}

// The side normals point straight out from the axis, so the light rolls smoothly
// round it (smooth shading, L-5-2). UV.x runs 0..1 round the cylinder.
void cylinder(Vertices& v, float height, float r0, float r1, int segments, Material m) {
    const float id = float(m);
    const glm::vec3 top(0, height, 0);
    for (int i = 0; i < segments; ++i) {
        const float a = 2 * pi * i / segments, b = 2 * pi * (i + 1) / segments;
        const glm::vec3 ra(std::cos(a), 0, std::sin(a)), rb(std::cos(b), 0, std::sin(b));
        const glm::vec3 p0 = r0 * ra, p1 = r0 * rb;             // bottom ring
        const glm::vec3 p3 = top + r1 * ra, p2 = top + r1 * rb; // top ring
        // Tilt the normal a little upwards when the cylinder narrows (a cone side).
        const glm::vec3 na = glm::normalize(ra + glm::vec3(0, (r0 - r1) / height, 0));
        const glm::vec3 nb = glm::normalize(rb + glm::vec3(0, (r0 - r1) / height, 0));
        const float u0 = float(i) / segments, u1 = float(i + 1) / segments;
        v.insert(v.end(), {{p0, na, {u0, 0}, id}, {p3, na, {u0, 1}, id}, {p2, nb, {u1, 1}, id},
                           {p0, na, {u0, 0}, id}, {p2, nb, {u1, 1}, id}, {p1, nb, {u1, 0}, id}});
        triangle(v, top, p2, p3, m);  // top cap
    }
}

// A point is found from two angles: theta from the north pole down, phi round the
// vertical axis. On a sphere the normal is simply the direction from the centre.
void sphere(Vertices& v, float r, int rings, int segments, Material m) {
    const float id = float(m);
    auto dir = [&](int i, int j) {
        const float theta = pi * i / rings, phi = 2 * pi * j / segments;
        return glm::vec3(std::sin(theta) * std::cos(phi), std::cos(theta),
                         std::sin(theta) * std::sin(phi));
    };
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            const glm::vec3 a = dir(i, j), b = dir(i + 1, j), c = dir(i + 1, j + 1), d = dir(i, j + 1);
            // Order a-d-c and a-c-b is counter-clockwise seen from OUTSIDE the sphere.
            v.insert(v.end(), {{r * a, a, {0, 0}, id}, {r * d, d, {1, 0}, id}, {r * c, c, {1, 1}, id},
                               {r * a, a, {0, 0}, id}, {r * c, c, {1, 1}, id}, {r * b, b, {0, 1}, id}});
        }
    }
}

// Points use m; normals use the inverse transpose of m, which keeps them
// perpendicular to the surface even after uneven scaling.
void place(Vertices& out, const Vertices& part, const glm::mat4& m) {
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(m)));
    for (Vertex p : part) {
        p.position = glm::vec3(m * glm::vec4(p.position, 1.0f));
        p.normal = glm::normalize(normalMatrix * p.normal);
        out.push_back(p);
    }
}

// An integer hash: multiply and mix the bits so that neighbouring seeds give
// unrelated results. It is not truly random; it is a fixed, repeatable function.
float random01(unsigned seed) {
    seed = (seed << 13U) ^ seed;
    seed = seed * (seed * seed * 15731U + 789221U) + 1376312589U;
    return float(seed & 0x7fffffffU) / 2147483648.0f;
}
}
