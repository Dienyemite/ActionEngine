#version 450

/*
 * FXAA Post-Process - Vertex Stage
 * 
 * Fullscreen triangle for post-processing
 */

layout(location = 0) out vec2 fragTexCoord;

void main() {
    // Fullscreen triangle trick
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);
    
    // Convert from [-1,1] to [0,1] UV space
    fragTexCoord = pos * 0.5 + 0.5;
}
