varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    
    // Convert to polar
    float r = length(uv);
    float a = atan(uv.y, uv.x);
    
    // Normalize Angle -1..1 -> 0..1
    // We want the spectrum to mirror, so 0..1..0
    float angleNorm = abs(a / 3.14159);
    
    // Sample spectrum
    float val = texture2D(u_spectrum, vec2(angleNorm, 0.5)).r;
    
    // Base radius of circle
    float baseR = 0.3 + 0.1 * val; 
    
    // Glow circle
    float dist = abs(r - baseR);
    float intensity = 0.01 / (dist + 0.001);
    
    // Color rainbow
    vec3 col = vec3(0.0);
    col.r = sin(angleNorm * 6.0 + u_time) * 0.5 + 0.5;
    col.g = sin(angleNorm * 6.0 + u_time + 2.0) * 0.5 + 0.5;
    col.b = sin(angleNorm * 6.0 + u_time + 4.0) * 0.5 + 0.5;
    
    col *= intensity;
    
    // Inner fill
    if (r < baseR) {
        col += vec3(0.1, 0.0, 0.1) * val;
    }
    
    gl_FragColor = vec4(col, 1.0);
}
