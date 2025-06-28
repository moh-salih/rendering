#version 330 core

// Regular attributes
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aTexCoord;

// Instance matrix attributes
layout(location = 3) in vec4 instanceMatrix0;
layout(location = 4) in vec4 instanceMatrix1;
layout(location = 5) in vec4 instanceMatrix2;
layout(location = 6) in vec4 instanceMatrix3;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform bool useInstancing;
uniform float time;
uniform vec3 mousePos;

out vec4 objectColor;
out vec2 TexCoord;

void main() {
    mat4 instanceMatrix = mat4(instanceMatrix0, instanceMatrix1, instanceMatrix2, instanceMatrix3);
    mat4 world = useInstancing ? instanceMatrix * model : model; 

    vec4 worldPos = world * vec4(aPos, 1.0);
    gl_Position = projection * view * worldPos;
    
    TexCoord = aTexCoord;
    

    objectColor = vec4(1.0, 0.0, 0.0, 1.0);
}