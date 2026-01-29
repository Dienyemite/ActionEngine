#version 450

/*
 * FXAA 3.11 - Fast Approximate Anti-Aliasing
 * 
 * Based on NVIDIA FXAA by Timothy Lottes
 * Optimized for performance on GTX 660 class hardware
 * 
 * Quality preset: Medium-High (good balance of quality/performance)
 */

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneTexture;

layout(push_constant) uniform FXAAParams {
    vec2 texelSize;     // 1.0 / textureSize
    float qualitySubpix;     // 0.75 default - amount of sub-pixel aliasing removal
    float qualityEdgeThreshold;  // 0.166 default - minimum local contrast to apply AA
} params;

// Luminance calculation (perceptual)
float Luminance(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 texelSize = params.texelSize;
    
    // Sample center and neighbors
    vec3 rgbM  = texture(sceneTexture, uv).rgb;
    vec3 rgbNW = texture(sceneTexture, uv + vec2(-1.0, -1.0) * texelSize).rgb;
    vec3 rgbNE = texture(sceneTexture, uv + vec2( 1.0, -1.0) * texelSize).rgb;
    vec3 rgbSW = texture(sceneTexture, uv + vec2(-1.0,  1.0) * texelSize).rgb;
    vec3 rgbSE = texture(sceneTexture, uv + vec2( 1.0,  1.0) * texelSize).rgb;
    
    // Convert to luminance
    float lumM  = Luminance(rgbM);
    float lumNW = Luminance(rgbNW);
    float lumNE = Luminance(rgbNE);
    float lumSW = Luminance(rgbSW);
    float lumSE = Luminance(rgbSE);
    
    // Find min/max luma around the center
    float lumMin = min(lumM, min(min(lumNW, lumNE), min(lumSW, lumSE)));
    float lumMax = max(lumM, max(max(lumNW, lumNE), max(lumSW, lumSE)));
    
    // Compute local contrast
    float lumRange = lumMax - lumMin;
    
    // Early exit if contrast is below threshold (no edge detected)
    if (lumRange < max(0.0312, lumMax * params.qualityEdgeThreshold)) {
        outColor = vec4(rgbM, 1.0);
        return;
    }
    
    // Sample additional neighbors for better edge detection
    vec3 rgbN = texture(sceneTexture, uv + vec2( 0.0, -1.0) * texelSize).rgb;
    vec3 rgbS = texture(sceneTexture, uv + vec2( 0.0,  1.0) * texelSize).rgb;
    vec3 rgbW = texture(sceneTexture, uv + vec2(-1.0,  0.0) * texelSize).rgb;
    vec3 rgbE = texture(sceneTexture, uv + vec2( 1.0,  0.0) * texelSize).rgb;
    
    float lumN = Luminance(rgbN);
    float lumS = Luminance(rgbS);
    float lumW = Luminance(rgbW);
    float lumE = Luminance(rgbE);
    
    // Compute edge direction
    float edgeHorz = abs((lumNW + lumNE) - 2.0 * lumN) +
                     abs((lumW  + lumE ) - 2.0 * lumM) * 2.0 +
                     abs((lumSW + lumSE) - 2.0 * lumS);
    
    float edgeVert = abs((lumNW + lumSW) - 2.0 * lumW) +
                     abs((lumN  + lumS ) - 2.0 * lumM) * 2.0 +
                     abs((lumNE + lumSE) - 2.0 * lumE);
    
    bool isHorizontal = edgeHorz >= edgeVert;
    
    // Select the two neighboring pixels along the edge
    float lum1 = isHorizontal ? lumN : lumW;
    float lum2 = isHorizontal ? lumS : lumE;
    
    // Compute gradients along edge
    float gradient1 = lum1 - lumM;
    float gradient2 = lum2 - lumM;
    
    bool is1Steepest = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));
    
    // Step perpendicular to edge
    float stepLength = isHorizontal ? texelSize.y : texelSize.x;
    float lumaLocalAverage = 0.0;
    
    if (is1Steepest) {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (lum1 + lumM);
    } else {
        lumaLocalAverage = 0.5 * (lum2 + lumM);
    }
    
    // Shift UV in perpendicular direction by half a pixel
    vec2 currentUV = uv;
    if (isHorizontal) {
        currentUV.y += stepLength * 0.5;
    } else {
        currentUV.x += stepLength * 0.5;
    }
    
    // Explore along the edge in both directions
    vec2 offset = isHorizontal ? vec2(texelSize.x, 0.0) : vec2(0.0, texelSize.y);
    
    vec2 uv1 = currentUV - offset;
    vec2 uv2 = currentUV + offset;
    
    float lumaEnd1 = Luminance(texture(sceneTexture, uv1).rgb) - lumaLocalAverage;
    float lumaEnd2 = Luminance(texture(sceneTexture, uv2).rgb) - lumaLocalAverage;
    
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;
    
    // Continue exploring if we haven't found the edge endpoints
    // Quality: 12 iterations (high quality - smooth edges)
    const int ITERATIONS = 12;
    const float QUALITY[12] = float[](1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);
    
    for (int i = 0; i < ITERATIONS && !reachedBoth; i++) {
        if (!reached1) {
            uv1 -= offset * QUALITY[i];
            lumaEnd1 = Luminance(texture(sceneTexture, uv1).rgb) - lumaLocalAverage;
            reached1 = abs(lumaEnd1) >= gradientScaled;
        }
        if (!reached2) {
            uv2 += offset * QUALITY[i];
            lumaEnd2 = Luminance(texture(sceneTexture, uv2).rgb) - lumaLocalAverage;
            reached2 = abs(lumaEnd2) >= gradientScaled;
        }
        reachedBoth = reached1 && reached2;
    }
    
    // Compute distances to edge endpoints
    float dist1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float dist2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);
    
    bool isDir1 = dist1 < dist2;
    float distFinal = min(dist1, dist2);
    float edgeLength = dist1 + dist2;
    
    // Compute sub-pixel offset
    float pixelOffset = -distFinal / edgeLength + 0.5;
    
    // Check if the center luma is on the correct side of the edge
    bool isLumaCenterSmaller = lumM < lumaLocalAverage;
    bool correctVariation = ((isDir1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
    
    float finalOffset = correctVariation ? pixelOffset : 0.0;
    
    // Sub-pixel anti-aliasing (optional, controlled by qualitySubpix)
    float lumaAverage = (1.0 / 12.0) * (2.0 * (lumN + lumS + lumW + lumE) + (lumNW + lumNE + lumSW + lumSE));
    float subPixelOffset = clamp(abs(lumaAverage - lumM) / lumRange, 0.0, 1.0);
    subPixelOffset = (-2.0 * subPixelOffset + 3.0) * subPixelOffset * subPixelOffset;
    float subPixelOffsetFinal = subPixelOffset * subPixelOffset * params.qualitySubpix;
    
    finalOffset = max(finalOffset, subPixelOffsetFinal);
    
    // Apply the offset and sample
    vec2 finalUV = uv;
    if (isHorizontal) {
        finalUV.y += finalOffset * stepLength;
    } else {
        finalUV.x += finalOffset * stepLength;
    }
    
    vec3 finalColor = texture(sceneTexture, finalUV).rgb;
    outColor = vec4(finalColor, 1.0);
}
