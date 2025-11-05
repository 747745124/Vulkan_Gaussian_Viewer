#version 450
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inCorner;         // per-vertex (quad), in [-1,1]
layout(location = 1) in vec3 inCenter;         // per-instance
layout(location = 2) in vec3 inColor;          // per-instance
layout(location = 3) in float inRadius;        // per-instance (unused in this step)
layout(location = 4) in vec3 inScale;          // per-instance (unused in this step)
layout(location = 5) in vec4 inQuat;           // per-instance (unused in this step)
layout(location = 6) in float inOpacity;       // per-instance (unused in this step)

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vPosition;       // not used in this step

layout(push_constant) uniform PushConstants {
    vec2 viewport; // width, height
} pc;

void main(){
    // Transform center into camera space (model assumed identity)
    vec4 cam = ubo.view * vec4(inCenter, 1.0);
    vec4 pos2d = ubo.proj * cam;

    // Clip against a slightly expanded frustum like the JS implementation
    float clip = 1.2 * pos2d.w;
    if (pos2d.z < -clip || pos2d.x < -clip || pos2d.x > clip || pos2d.y < -clip || pos2d.y > clip || pos2d.w <= 0.0) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }

    // Baseline: draw a small constant-size quad in screen space (e.g. 10 px)
    vec2 center = pos2d.xy / pos2d.w;
    float pixelSize = 10.0;
    vec2 offset = inCorner * (pixelSize / pc.viewport) * 2.0;

    vColor = inColor;
    vPosition = inCorner * 2.0; // not used in this step
    gl_Position = vec4(center + offset, 0.0, 1.0);
}
