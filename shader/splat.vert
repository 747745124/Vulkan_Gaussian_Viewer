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

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vPosition;       // not used in this step

layout(push_constant) uniform PushConstants {
    vec2 viewport; // width, height
} pc;

void main(){
    // Transform center into camera space (model assumed identity)
    vec4 cam = ubo.view * vec4(inCenter, 1.0);
    vec4 pos2d = ubo.proj * cam;

    // Keep only behind-camera reject for safety
    if (pos2d.w <= 0.0) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }

    // Revert to Step 5: isotropic Gaussian size from depth & focal
    vec2 center = pos2d.xy / pos2d.w;
    float z = max(1e-3, -cam.z);
    float fx = abs(ubo.proj[0][0]) * pc.viewport.x * 0.5;
    float fy = abs(ubo.proj[1][1]) * pc.viewport.y * 0.5;
    float s = max(max(inScale.x, inScale.y), inScale.z);
    float sigmaX = s * fx / z;
    float sigmaY = s * fy / z;

    float depthFade = clamp(pos2d.z / pos2d.w + 1.0, 0.0, 1.0);
    vColor = vec4(inColor * depthFade, inOpacity);
    vPosition = inCorner * 2.0; // Gaussian domain units
    vec2 offset = vec2(vPosition.x * sigmaX, vPosition.y * sigmaY) / pc.viewport * 2.0;
    gl_Position = vec4(center + offset, 0.0, 1.0);
}
