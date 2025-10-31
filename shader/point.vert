#version 450
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 3) in vec3 inColor;

layout(location = 0) out vec3 vColor;

void main(){
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    vColor = inColor;
    gl_PointSize = 1.0; // may clamp to 1.0 if largePoints unsupported
}
