
#version 330 core

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Tangent;

out vec3 v_Color;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform vec3 u_Color;

void main()
{
    v_Color = u_Color;
    gl_Position = u_Projection * u_View * u_Model * vec4(a_Pos, 1.0);
}