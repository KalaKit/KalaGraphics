
#version 330 core

in vec4 v_Color;

out vec4 FragColor;

uniform vec4 u_Tint;

void main()
{
    FragColor = v_Color * u_Tint;
}