#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 UV;
out vec4 FragColor;

uniform sampler2D stoneTexture;
uniform vec3 lightPos;

void main() {
    vec3 stoneColor = texture(stoneTexture, UV).rgb;
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diffuse = max(dot(normal, lightDir), 0.0);

    // Only two simple components: ambient + diffuse.
    vec3 result = stoneColor * (0.40 + 0.60 * diffuse);
    FragColor = vec4(result, 1.0);
}

// TODO for final: replace this simple surface shader with POM if required.
