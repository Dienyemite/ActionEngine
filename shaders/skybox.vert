#version 450

/*
 * Skybox Shader - Vertex Stage
 * 
 * Generates a fullscreen triangle with world-space ray directions
 * for procedural sky rendering
 */

layout(location = 0) out vec3 fragDirection;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 cameraPos;
    float time;
} camera;

void main() {
    // Fullscreen triangle trick: generate oversized triangle
    // Vertex 0: (-1, -1), Vertex 1: (3, -1), Vertex 2: (-1, 3)
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.9999, 1.0);  // At far plane
    
    // Reconstruct view-space ray from clip coordinates
    // Invert projection to get view-space direction
    mat4 invProj = inverse(camera.projection);
    vec4 viewDir = invProj * vec4(pos, 1.0, 1.0);
    viewDir.xyz /= viewDir.w;
    
    // Transform to world space (remove translation from view matrix)
    mat3 invView = transpose(mat3(camera.view));
    fragDirection = invView * viewDir.xyz;
}
