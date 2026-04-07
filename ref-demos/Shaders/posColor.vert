#version 460

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 color;

layout (location = 0) out vec4 Color;

void main()
{
    Color = color;
    gl_Position = pos;
}