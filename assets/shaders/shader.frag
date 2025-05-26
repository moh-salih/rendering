#version 330 core

in vec4 posColor;
out vec4 FragColor;

void main(){
    FragColor = vec4(posColor.xyz, 1.0);
}