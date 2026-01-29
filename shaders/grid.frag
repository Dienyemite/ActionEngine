#version 450

/*
 * Infinite Grid Shader - Fragment Stage
 * 
 * Renders an infinite grid on the XZ plane with:
 * - Major lines (every 10 units) - orange/red
 * - Minor lines (every 1 unit) - darker
 * - Distance-based fade
 * - Proper depth for object occlusion
 */

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNearPoint;
layout(location = 2) in vec3 fragFarPoint;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 cameraPos;
    float time;
} camera;

// Grid settings
const float MINOR_GRID_SIZE = 1.0;      // Minor grid every 1 unit
const float MAJOR_GRID_SIZE = 10.0;     // Major grid every 10 units
const float LINE_WIDTH = 0.02;          // Line thickness
const float FADE_DISTANCE = 150.0;      // Distance at which grid fades

// Colors (Godot-style orange)
const vec3 MAJOR_COLOR = vec3(0.85, 0.35, 0.15);   // Orange for major lines
const vec3 MINOR_COLOR = vec3(0.5, 0.25, 0.1);     // Darker orange for minor
const vec3 X_AXIS_COLOR = vec3(0.9, 0.2, 0.2);     // Red for X axis
const vec3 Z_AXIS_COLOR = vec3(0.2, 0.4, 0.9);     // Blue for Z axis

// Compute grid line intensity using screen-space derivatives
vec4 Grid(vec3 worldPos, float gridSize, vec3 color) {
    vec2 coord = worldPos.xz / gridSize;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float alpha = 1.0 - min(line, 1.0);
    return vec4(color, alpha);
}

void main() {
    // Ray-plane intersection with Y=0 plane
    vec3 rayDir = fragFarPoint - fragNearPoint;
    float t = -fragNearPoint.y / rayDir.y;
    
    // Discard if plane is behind camera or intersection is invalid
    if (t < 0.0) {
        discard;
    }
    
    // World position on the grid plane
    vec3 worldPos = fragNearPoint + t * rayDir;
    
    // Compute depth for proper occlusion
    vec4 clipPos = camera.viewProjection * vec4(worldPos, 1.0);
    float depth = clipPos.z / clipPos.w;
    gl_FragDepth = depth * 0.5 + 0.5;  // Convert from [-1,1] to [0,1]
    
    // Distance from camera for fading
    float distFromCamera = length(worldPos - camera.cameraPos);
    float fade = 1.0 - smoothstep(FADE_DISTANCE * 0.5, FADE_DISTANCE, distFromCamera);
    
    if (fade <= 0.0) {
        discard;
    }
    
    // Compute minor grid
    vec4 minorGrid = Grid(worldPos, MINOR_GRID_SIZE, MINOR_COLOR);
    
    // Compute major grid
    vec4 majorGrid = Grid(worldPos, MAJOR_GRID_SIZE, MAJOR_COLOR);
    
    // Blend major over minor
    vec4 gridColor = minorGrid;
    gridColor = mix(gridColor, majorGrid, majorGrid.a);
    
    // Highlight X and Z axes
    float axisWidth = 0.08;  // Slightly thicker axis lines
    vec2 derivative = fwidth(worldPos.xz);
    
    // X axis (red line along X when Z ≈ 0)
    float xAxis = 1.0 - smoothstep(0.0, axisWidth * derivative.y * 2.0, abs(worldPos.z));
    
    // Z axis (blue line along Z when X ≈ 0)
    float zAxis = 1.0 - smoothstep(0.0, axisWidth * derivative.x * 2.0, abs(worldPos.x));
    
    // Apply axis colors
    if (zAxis > 0.0) {
        gridColor.rgb = mix(gridColor.rgb, Z_AXIS_COLOR, zAxis * 0.9);
        gridColor.a = max(gridColor.a, zAxis);
    }
    if (xAxis > 0.0) {
        gridColor.rgb = mix(gridColor.rgb, X_AXIS_COLOR, xAxis * 0.9);
        gridColor.a = max(gridColor.a, xAxis);
    }
    
    // Apply distance fade
    gridColor.a *= fade;
    
    // Final output with some minimum visibility
    outColor = vec4(gridColor.rgb, gridColor.a * 0.8);
}
