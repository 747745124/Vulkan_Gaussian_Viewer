#version 450
layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vPosition;
layout(location = 0) out vec4 outColor;

void main() {
    // Step 4: premultiplied alpha
    float A = -dot(vPosition, vPosition);
    if (A < -4.0) discard; // outside 2*sigma
    float B = exp(A) * vColor.a;
    outColor = vec4(B * vColor.rgb, B); // premultiplied alpha with per-splat opacity
}
