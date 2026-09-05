#version 460

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inVertexColor;
layout(location = 2) in vec4 inColor;
layout(location = 3) flat in uint isTransparent;

layout(set = 2, binding = 0) uniform sampler2D uTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    float coverage = texture(uTexture, inUV).r;

    vec4 baseColor = inVertexColor * inColor;

    baseColor.a *= coverage;

    //TODO: remove isTransparent toggle completely
    //if (isTransparent == 0) baseColor.a = 1.0;

    outColor = vec4(
        baseColor.rgb * baseColor.a,
        baseColor.a);
}
