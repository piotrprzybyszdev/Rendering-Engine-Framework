#version 460

layout (location = 0) in vec4 Color;

layout (location = 0) out vec4 uFragColor;

void main()
{
    uFragColor = vec4(Color.rgb, 0.4f);
}