#version 330 core

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec2 vTexcoord;
layout (location = 2) in vec3 vNormal;

out vec2 texcoord;

uniform mat4 MVP;         // Model View Projection


void main()
{
    gl_Position = MVP * vec4(vPosition, 1.0); // NDC Coordinates
    texcoord = vTexcoord;   // UV Coordinates to frag
}
