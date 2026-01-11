varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    
    // Polar coordinates
    float r = length(uv);
    float a = atan(uv.y, uv.x);
    // Normalize angle 0..1
    float aNorm = (a + 3.14159) / (2.0 * 3.14159);
    
    // Sweep line
    float sweepSpeed = 2.0;
    float sweepPos = fract(u_time * 0.5); // 0..1 loop
    
    // Distance from sweep line
    float dist = abs(aNorm - sweepPos);
    // Handle wrap around
    if (dist > 0.5) dist = 1.0 - dist;
    
    // Fade trail
    float trail = smoothstep(0.3, 0.0, dist);
    
    // Grid rings
    float grid = 0.0;
    if (fract(r * 5.0) < 0.05) grid = 0.5;
    
    // Detect "blips" based on spectrum
    // Sample spectrum at this angle
    float val = texture2D(u_spectrum, vec2(r, 0.5)).r;
    float blip = 0.0;
    if (val > 0.5 && trail > 0.1) {
        blip = 1.0 * trail;
    }
    
    vec3 color = vec3(0.0, 0.2, 0.0); // Base green
    color += vec3(0.0, 0.8, 0.0) * grid * 0.5; // Grid
    color += vec3(0.0, 1.0, 0.0) * trail * 0.5; // Sweep
    color += vec3(1.0, 1.0, 1.0) * blip; // Blips
    
    // Vignette
    color *= smoothstep(1.0, 0.9, r);

    gl_FragColor = vec4(color, 1.0);
}
