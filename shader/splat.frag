#version 450
layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vPosition;
layout(location = 0) out vec4 outColor;

void main(){
    float r2 = dot(vPosition, vPosition);
    if (r2 > 4.0) discard; // outside 2*sigma circle
    float alpha = exp(-0.5 * r2);
    vec3 rgb = vColor;
    outColor = vec4(rgb * alpha, alpha); // premultiplied alpha
}
