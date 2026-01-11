varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    
    float t = u_time * 2.0;
    
    vec3 col = vec3(0.0);
    
    // Two sine waves phase shifted
    for(int i=0; i<2; i++) {
        float sign = (i==0) ? 1.0 : -1.0;
        
        // Helix path
        float y = uv.x * 2.0 + t * sign; 
        float width = 0.3 * sin(y);
        
        // Draw dots/rungs
        // Quantize X
        float xQ = floor(uv.x * 20.0)/20.0;
        
        float curveY = sign * 0.5 * sin(uv.x * 3.0 + t);
        
        float dist = abs(uv.y - curveY);
        
        // Dots
        if (dist < 0.05) {
             // Sample audio for DNA activity
             float val = texture2D(u_spectrum, vec2(abs(uv.x), 0.5)).r;
             
             // Color code
             vec3 base = (i==0) ? vec3(0.2, 0.8, 0.2) : vec3(0.2, 0.2, 0.8);
             
             col += base * (1.0 + val * 2.0) * (0.02 / dist);
        }
        
        // Rungs (connecting lines)
        // Hard to do simply in 2D shader without loop, skipping for now
    }

    gl_FragColor = vec4(col, 1.0);
}
