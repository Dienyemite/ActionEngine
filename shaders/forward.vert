#version 450

/*
 * Basic Forward Shader - Vertex Stage
 * 
 * Optimized for GTX 660:
 * - Simple vertex transformation
 * - Minimal varyings
 */

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 cameraPos;
    float time;
} camera;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;  // passed to fragment shader via push constants
} push;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    
    fragWorldPos = worldPos.xyz;
    // Compute normalMatrix in the shader to keep push constants at 80 bytes (#24)
    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    fragNormal = normalMatrix * inNormal;
    fragTexCoord = inTexCoord;
    
    gl_Position = camera.viewProjection * worldPos;
}
