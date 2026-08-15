#version 460

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;

layout(set = 2, binding = 0) uniform sampler2D uTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(uTexture, inUV) * inColor;
}
