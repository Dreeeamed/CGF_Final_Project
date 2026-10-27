#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in float aMaterial;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 FragPos;
out vec3 Normal;
out vec2 UV;
flat out int Material;
void main() {
    vec4 world = model * vec4(aPosition,1.0);
    FragPos = world.xyz;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    UV = aUV;
    Material = int(aMaterial + 0.5);
    gl_Position = projection * view * world;
}
