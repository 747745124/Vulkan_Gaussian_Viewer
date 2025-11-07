#version 450
layout(binding = 0) uniform MVPMatrix {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inCorner;      // per-vertex (quad), [-1, 1]
layout(location = 1) in vec3 inCenter;      // per-instance (world space)
layout(location = 2) in vec3 inColor;       // per-instance
layout(location = 3) in float inRadius;    // (unused)
layout(location = 4) in vec3 inScale;       // per-instance (already exp'd)
layout(location = 5) in vec4 inQuat;        // per-instance (normalized, (x,y,z,w))
layout(location = 6) in float inOpacity;   // per-instance

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vPosition;    // Quad-local coords [-2, 2]

layout(push_constant) uniform PushConstants {
    vec2 viewport; // width, height
} pc;

void main() {
    // --- 1. Transform center point ---
    vec4 cam = ubo.view * vec4(inCenter, 1.0);
    vec4 pos2d = ubo.proj * cam;

    // clipping
    if (pos2d.w <= 0.0) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }
    
    // --- 2. Calculate 3D covariance Vrk ---
    vec4 q = inQuat;
    float w = q.w, x = q.x, y = q.y, z = q.z;
    mat3 R = mat3(
        1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - w * z), 2.0 * (x * z + w * y),
        2.0 * (x * y + w * z), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - w * x),
        2.0 * (x * z - w * y), 2.0 * (y * z + w * x), 1.0 - 2.0 * (x * x + y * y)
    );

    mat3 S = mat3(
        inScale.x, 0.0, 0.0,
        0.0, inScale.y, 0.0,
        0.0, 0.0, inScale.z
    );

    mat3 M = S * R;
    mat3 Vrk = transpose(M) * M;

    // --- 3. Calculate projected 2D covariance ---
    float fx = abs(ubo.proj[0][0]) * pc.viewport.x * 0.5;
    float fy = abs(ubo.proj[1][1]) * pc.viewport.y * 0.5;
    float p_z = max(1e-3, -cam.z); // View-space Z (positive value)
    
    mat3 J = mat3(
        -fx / p_z, 0.0, -(fx * cam.x) / (p_z * p_z),
        0.0, fy / p_z, (fy * cam.y) / (p_z * p_z),
        0.0, 0.0, 0.0
    );
    
    mat3 W = mat3(ubo.view);
    mat3 T = transpose(W) * J;
    mat3 cov2d = transpose(T) * Vrk * T;

    // --- 4. Calculate 2D ellipse axes ---
    float mid = (cov2d[0][0] + cov2d[1][1]) / 2.0;
    float radius = length(vec2((cov2d[0][0] - cov2d[1][1]) / 2.0, cov2d[0][1]));
    float lambda1 = mid + radius;
    float lambda2 = mid - radius;

    lambda2 = max(0.0, lambda2); // prevent negative value from causing sqrt(NaN)

    vec2 diagonalVector = normalize(vec2(cov2d[0][1], lambda1 - cov2d[0][0]));
    vec2 majorAxis = min(sqrt(2.0 * lambda1), 1024.0) * diagonalVector;
    vec2 minorAxis = min(sqrt(2.0 * lambda2), 1024.0) * vec2(diagonalVector.y, -diagonalVector.x);

    // --- 5. Calculate final position and color ---
    float depthFade = clamp(pos2d.z / pos2d.w + 1.0, 0.0, 1.0);
    
    vColor = vec4(inColor, inOpacity) * depthFade;
    
    // pass to frag shader Gaussian coordinates (range [-2, 2])
    vPosition = inCorner * 2.0; 
    vec2 vCenter = pos2d.xy / pos2d.w; // NDC center
    
    vec2 offset = (vPosition.x * majorAxis + vPosition.y * minorAxis) * 2.0 / pc.viewport;

    gl_Position = vec4(vCenter + offset, 0.0, 1.0);
}