#version 330 core

in vec4 objectColor;
in vec2 TexCoord;  // Remove the duplicate declaration

uniform sampler2D texture0;
uniform bool useTexture;


out vec4 FragColor;

void main() {
    FragColor = useTexture ? texture(texture0, TexCoord) : objectColor;
}