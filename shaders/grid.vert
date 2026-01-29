#version 450

/*
 * Infinite Grid Shader - Vertex Stage
 * 
 * Renders an infinite grid on the XZ plane (Y=0)
 * Uses fullscreen triangle with world-space reconstruction
 */

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNearPoint;
layout(location = 2) out vec3 fragFarPoint;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 cameraPos;
    float time;
} camera;

// Unproject point from clip space to world space
vec3 UnprojectPoint(float x, float y, float z) {
    mat4 invViewProj = inverse(camera.viewProjection);
    vec4 unprojected = invViewProj * vec4(x, y, z, 1.0);
    return unprojected.xyz / unprojected.w;
}

void main() {
    // Fullscreen triangle
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);
    
    // Unproject near and far plane points for ray-plane intersection
    fragNearPoint = UnprojectPoint(pos.x, pos.y, 0.0);  // Near plane
    fragFarPoint = UnprojectPoint(pos.x, pos.y, 1.0);   // Far plane
    fragWorldPos = fragNearPoint;
}
