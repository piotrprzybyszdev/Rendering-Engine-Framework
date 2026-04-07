#version 460

layout(set = 0, binding = 0) uniform MatrixBuffer {
	mat4x4 u_CameraProjection;
	mat4x4 u_CameraView;
	mat4x4 u_ModelTransform;
};

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 color;

layout (location = 0) out vec4 Color;

void main()
{
    Color = color;
    gl_Position = u_CameraProjection * u_CameraView * u_ModelTransform * pos;
}