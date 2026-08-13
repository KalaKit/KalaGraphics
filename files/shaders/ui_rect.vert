#version 460

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(push_constant) uniform Rect
{
    vec2 offset;
    vec2 size;
} rectData;

layout(set = 0, binding = 0) uniform CameraUBO
{
    mat4 projectionMatrix;
} camera;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;

void main()
{
    vec2 pos =
        inPosition
        * rectData.size
        + rectData.offset;

    gl_Position =
        camera.projectionMatrix
        * vec4(pos, 0.0, 1.0);

    outUV = inUV;
    outColor = inColor;
}
