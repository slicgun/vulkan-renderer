#version 450

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
layout(location = 2) in vec2 a_texCoords;

layout(location = 0) out vec3 v_fragColor;
layout(location = 1) out vec2 v_texCoords;

layout(binding = 0) uniform UniformBufferObject

{
 mat4 model;
 mat4 view;
 mat4 proj;
} ubo;

void main()
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(a_position, 1);
    v_fragColor = a_color;
    v_texCoords = a_texCoords;
}