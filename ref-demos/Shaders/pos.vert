#version 460

layout (location = 0) in vec4 pos;

layout (location = 0) out vec4 Color;

void main()
{
    Color = vec4(1.0f);
    gl_Position = pos;
}