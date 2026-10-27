#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 UV;

out vec4 FragColor;

uniform sampler2D stoneTexture;
uniform vec3 lightPos;
uniform vec3 cameraPos;

void main() {
    vec2 stoneUV = UV * 0.5;
    vec3 textureColor = texture(stoneTexture, stoneUV).rgb;

    // Keep the stone pattern, but soften its contrast.
    float brightness = dot(
        textureColor,
        vec3(0.2126, 0.7152, 0.0722)
    );

    brightness = mix(0.5, brightness, 0.35);

    vec3 stoneColor = vec3(0.42, 0.55, 0.72) * brightness;

    // Cool side lighting.
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 litColor = stoneColor * (0.25 + 0.75 * diffuse);

    // Distant surfaces gradually merge with the background.
    float viewDistance = length(cameraPos - FragPos);
    float fogAmount = smoothstep(35.0, 85.0, viewDistance);

    vec3 fogColor = vec3(0.055, 0.085, 0.14);
    vec3 finalColor = mix(litColor, fogColor, fogAmount);

    FragColor = vec4(finalColor, 1.0);
}