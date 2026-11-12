#version 330 core
// Every pixel of a letter (or of the panel) gets one colour; alpha < 1 is see-through.
uniform vec4 colour;
out vec4 FragColor;

void main() {
    FragColor = colour;
}
