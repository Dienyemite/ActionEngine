#version 450

/*
 * Basic Forward Shader - Fragment Stage
 * 
 * Simplified version without textures - uses vertex color/normals for shading
 */

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// Camera data
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 cameraPos;
    float time;
} camera;

// Lighting data - using vec4 for proper std140 alignment
layout(set = 0, binding = 1) uniform LightingUBO {
    vec4 sunDirection;      // xyz = direction, w = intensity
    vec4 sunColor;          // xyz = color, w = ambient intensity  
    vec4 ambientColor;      // xyz = ambient color, w = padding
} lighting;

// Push constants include object color
layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 normalMatrix;
    vec4 color;
} push;

void main() {
    vec3 albedo = push.color.rgb;
    
    // Normal and lighting vectors
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-lighting.sunDirection.xyz);
    float sunIntensity = lighting.sunDirection.w;
    
    // Simple diffuse lighting
    float NdotL = max(dot(N, L), 0.0);
    
    // Ambient + diffuse
    vec3 ambient = albedo * lighting.ambientColor.xyz * 0.3;
    vec3 diffuse = albedo * lighting.sunColor.xyz * NdotL * sunIntensity;
    
    vec3 color = ambient + diffuse;
    
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));
    
    outColor = vec4(color, 1.0);
}
