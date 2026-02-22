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
    vec4 color;
} push;

void main() {
    vec3 albedo = push.color.rgb;
    
    // Normal and view vectors
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(camera.cameraPos - fragWorldPos);
    
    // MatCap-style lighting (view-space based, like Blender solid mode)
    vec3 viewNormal = normalize((camera.view * vec4(N, 0.0)).xyz);
    
    // Two-point lighting setup (key + fill, like Blender's default)
    vec3 keyLightDir = normalize(vec3(0.4, 0.7, 0.5));   // Upper-right
    vec3 fillLightDir = normalize(vec3(-0.3, 0.2, 0.4)); // Lower-left fill
    
    // Standard lambertian with clamped falloff for darker shadows
    float keyDiffuse = max(dot(viewNormal, keyLightDir), 0.0);
    float fillDiffuse = max(dot(viewNormal, fillLightDir), 0.0) * 0.25;
    
    // Combine key and fill
    vec3 keyColor = vec3(0.75, 0.74, 0.72);  // Neutral key, reduced intensity
    vec3 fillColor = vec3(0.25, 0.24, 0.22); // Dimmer warm fill
    vec3 diffuse = keyColor * keyDiffuse + fillColor * fillDiffuse;
    
    // Darker ambient for areas facing away from lights
    float ambientOcclusion = viewNormal.y * 0.5 + 0.5;  // Top-down gradient
    float cavityDarken = 1.0 - pow(1.0 - max(dot(viewNormal, vec3(0.0, 0.0, 1.0)), 0.0), 2.0) * 0.3;
    vec3 ambient = mix(vec3(0.08, 0.08, 0.09), vec3(0.18, 0.18, 0.20), ambientOcclusion) * cavityDarken;
    
    // Subtle rim for edge separation
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 rim = vec3(0.08) * fresnel;
    
    // Combine with albedo
    vec3 color = albedo * (ambient + diffuse * 0.65) + rim;
    
    // Gamma correction only (no tone mapping to preserve contrast)
    color = pow(color, vec3(1.0 / 2.2));
    
    outColor = vec4(color, 1.0);
}
