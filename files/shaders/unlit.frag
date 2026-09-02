#version 460

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 2) flat in uint isTransparent;

layout(set = 2, binding = 0) uniform sampler2D uTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 baseColor = texture(uTexture, inUV) * inColor;
    if (isTransparent == 0) baseColor.w = 1.0;
    outColor = vec4(baseColor.rgb * baseColor.a, baseColor.a);
}
