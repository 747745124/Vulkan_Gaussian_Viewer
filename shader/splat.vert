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

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vPosition;       // corner pos in sigma units

layout(push_constant) uniform PushConstants {
    vec2 viewport; // width, height
} pc;

void main(){
    // Transform center to camera space
    vec4 cam = ubo.view * ubo.model * vec4(inCenter, 1.0);

    // Project center to NDC
    vec4 pos2d = ubo.proj * cam;
    vec2 vCenter = pos2d.xy / pos2d.w;

    // Use a small constant sigma in pixels to match CPU "point" preview
    float sigmaPix = 2.0; // 2 px standard deviation
    float axisPix = 2.0 * sigmaPix; // quad extends to 2*sigma

    vec2 majorAxis = vec2(axisPix, 0.0);
    vec2 minorAxis = vec2(0.0, axisPix);

    vColor = inColor;
    vPosition = inCorner * 2.0; // in sigma units; corners -> [-2,2]

    // Offset by axes scaled by corner; convert from pixels to NDC
    vec2 offset = inCorner.x * majorAxis / pc.viewport + inCorner.y * minorAxis / pc.viewport;
    vec2 outPos = vCenter + offset * 2.0;

    gl_Position = vec4(outPos, 0.0, 1.0);
}
