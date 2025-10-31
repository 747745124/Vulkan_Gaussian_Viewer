#version 450
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inCorner;         // per-vertex (quad), in [-1,1]
layout(location = 1) in vec3 inCenter;         // per-instance
layout(location = 2) in vec3 inColor;          // per-instance
layout(location = 3) in float inRadius;        // per-instance (unused in constant-size mode)
layout(location = 4) in vec3 inScale;          // per-instance (sigma in world, linear)

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vPosition;       // corner pos in sigma units

layout(push_constant) uniform PushConstants {
    vec2 viewport; // width, height
} pc;

void main(){
    // Transform center to camera space
    vec4 cam = ubo.view * ubo.model * vec4(inCenter, 1.0);

    // Isotropic step A: use scale (no rotation), axis length varies with depth & focal
    float z = max(1e-3, -cam.z);
    float fx = abs(ubo.proj[0][0]) * pc.viewport.x * 0.5;
    float fy = abs(ubo.proj[1][1]) * pc.viewport.y * 0.5;
    // If your PLY stores log-σ, change to: vec3 s = exp(inScale);
    vec3 s = max(inScale, vec3(1e-4));
    float ax = clamp(2.0 * sqrt(2.0) * s.x * fx / z, 1.0, 16.0);
    float ay = clamp(2.0 * sqrt(2.0) * s.y * fy / z, 1.0, 16.0);
    vec2 e1 = vec2(1.0, 0.0);
    vec2 e2 = vec2(0.0, 1.0);

    // Project center to NDC
    vec4 pos2d = ubo.proj * cam;
    vec2 vCenter = pos2d.xy / pos2d.w;

    vColor = inColor;
    vPosition = inCorner * 2.0; // in sigma units; corners -> [-2,2]

    // Offset by axes scaled by corner; convert from pixels to NDC
    vec2 offset = (inCorner.x * ax * e1 + inCorner.y * ay * e2) / pc.viewport;
    vec2 outPos = vCenter + offset * 2.0;

    gl_Position = vec4(outPos, 0.0, 1.0);
}
