varying vec2 v_uv;
uniform float u_time;
uniform float u_pitch; // Hz
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    
    // Distort uv to create tunnel
    float r = length(uv);
    float a = atan(uv.y, uv.x);
    
    // Map radius to depth
    float depth = 1.0 / (r + 0.001); // +0.001 to avoid div by zero
    
    // Get spectrum value based on angle
    float angleNorm = (a / 3.14159) * 0.5 + 0.5;
    float spec = texture2D(u_spectrum, vec2(angleNorm, 0.5)).r;
    
    // Spiral movement
    float move = depth + u_time;
    
    // Checkboard / Grid pattern modified by audio
    float grid = sin(10.0 * a + spec * 10.0) * sin(10.0 * move);
    
    vec3 color = vec3(0.0);
    
    if (grid > 0.5) {
        // Color based on depth and pitch
        color = vec3(1.0/depth, 0.5 + 0.5*sin(u_time), spec);
    }
    
    // Darken center/distance
    color *= r;

    // Pitch reaction: rotate faster if pitch is high
    if (u_pitch > 500.0) {
        color.r += 0.2;
    }

    gl_FragColor = vec4(color, 1.0);
}
