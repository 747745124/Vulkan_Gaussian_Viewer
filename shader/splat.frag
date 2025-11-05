#version 450
layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vPosition;
layout(location = 0) out vec4 outColor;

void main() {
    // Step 1: no Gaussian, solid quad for visibility
    outColor = vec4(vColor, 1.0);
}
