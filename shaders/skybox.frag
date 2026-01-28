#version 450

/*
 * Skybox Shader - Fragment Stage
 * 
 * Procedural sky with:
 * - Atmospheric gradient
 * - Sun disk
 * - Simple clouds (optional)
 */

layout(location = 0) in vec3 fragDirection;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 cameraPos;
    float time;
} camera;

layout(set = 0, binding = 1) uniform LightingUBO {
    vec3 sunDirection;
    float sunIntensity;
    vec3 sunColor;
    float ambientIntensity;
    vec3 ambientColor;
    float padding;
} lighting;

// Procedural sky colors
const vec3 SKY_HORIZON = vec3(0.6, 0.75, 0.9);
const vec3 SKY_ZENITH = vec3(0.2, 0.4, 0.8);
const vec3 GROUND_COLOR = vec3(0.3, 0.25, 0.2);

void main() {
    vec3 dir = normalize(fragDirection);
    
    // Vertical gradient from ground to zenith
    float elevation = dir.y;
    
    // Sky color based on elevation
    vec3 skyColor;
    if (elevation > 0.0) {
        // Above horizon: blend from horizon to zenith
        float t = pow(elevation, 0.5);  // Pow for smoother gradient
        skyColor = mix(SKY_HORIZON, SKY_ZENITH, t);
    } else {
        // Below horizon: fade to ground color
        float t = pow(-elevation, 0.5);
        skyColor = mix(SKY_HORIZON, GROUND_COLOR, t);
    }
    
    // Sun disk
    vec3 sunDir = normalize(-lighting.sunDirection);
    float sunAngle = dot(dir, sunDir);
    
    // Sun disk (sharp core with soft glow)
    float sunDisk = smoothstep(0.9995, 0.9998, sunAngle);  // Tight sun disk
    float sunGlow = pow(max(sunAngle, 0.0), 64.0) * 0.5;  // Soft glow around sun
    
    vec3 sunContribution = lighting.sunColor * (sunDisk + sunGlow) * lighting.sunIntensity;
    skyColor += sunContribution;
    
    // Horizon haze (atmospheric scattering effect)
    float horizonHaze = 1.0 - abs(elevation);
    horizonHaze = pow(horizonHaze, 3.0) * 0.3;
    skyColor = mix(skyColor, SKY_HORIZON * 1.2, horizonHaze);
    
    // Simple fog at the horizon for depth
    float fogFactor = smoothstep(0.1, -0.05, elevation);
    skyColor = mix(skyColor, SKY_HORIZON * 0.9, fogFactor * 0.3);
    
    outColor = vec4(skyColor, 1.0);
}
