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
    vec3 V = normalize(camera.cameraPos - fragWorldPos);
    vec3 H = normalize(L + V);
    float sunIntensity = lighting.sunDirection.w;
    
    // Wrapped diffuse for soft clay-like shading (Blender MatCap style)
    float NdotL = dot(N, L);
    float wrap = 0.5;  // Wrap factor for softer falloff
    float diffuseWrap = max((NdotL + wrap) / (1.0 + wrap), 0.0);
    
    // Subtle hemisphere ambient (sky slightly brighter than ground)
    vec3 skyColor = vec3(0.45, 0.45, 0.48);   // Slightly cool
    vec3 groundColor = vec3(0.25, 0.24, 0.23); // Slightly warm
    float hemisphereBlend = N.y * 0.5 + 0.5;
    vec3 hemisphereAmbient = mix(groundColor, skyColor, hemisphereBlend);
    
    // Very subtle rim for edge definition
    float rimFactor = 1.0 - max(dot(N, V), 0.0);
    rimFactor = pow(rimFactor, 4.0) * 0.15;
    
    // Subtle specular (matte surface)
    float NdotH = max(dot(N, H), 0.0);
    float specular = pow(NdotH, 64.0) * 0.15 * max(NdotL, 0.0);
    
    // Combine lighting - balanced for clay/sculpt look
    vec3 ambient = albedo * hemisphereAmbient * 0.6;
    vec3 diffuse = albedo * vec3(0.9, 0.88, 0.85) * diffuseWrap * sunIntensity * 0.5;
    vec3 spec = vec3(1.0) * specular;
    vec3 rim = vec3(0.8) * rimFactor;
    
    vec3 color = ambient + diffuse + spec + rim;
    
    // Subtle contrast adjustment
    color = mix(vec3(0.4), color, 0.95);
    
    // Soft tone mapping
    color = color / (color + vec3(0.8));
    color *= 1.1;  // Slight exposure boost
    
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));
    
    outColor = vec4(color, 1.0);
}
