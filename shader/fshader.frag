#version 330 core

in vec2 texcoord;    // Texture Coordinates

out vec4 fColor;    // Fragment color

uniform sampler2D texture;  

void main()
{
    fColor.rgb =  texture2D(texture, texcoord).rgb;
}
