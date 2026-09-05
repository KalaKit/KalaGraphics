#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inVertexColor;
layout(location = 3) in vec4 inColor;
layout(location = 4) flat in uint isTransparent;

layout(set = 2, binding = 0) uniform sampler2D uTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 baseColor = texture(
        uTexture, inUV)
        * inVertexColor
        * inColor;

    if (isTransparent == 0) baseColor.w = 1.0;

    vec3 normal = normalize(inNormal);

    //basic surface visualization for unlit geometry
    float depth =
        abs(normal.x) * 0.25
        + abs(normal.y) * 0.5
        + abs(normal.z) * 0.25;

    float brightness = 0.35 + depth * 0.65;

    baseColor.rgb *= brightness;

    outColor = vec4(
        baseColor.rgb * baseColor.a,
        baseColor.a);
}
