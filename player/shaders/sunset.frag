varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv;
    
    vec3 col = vec3(0.0);
    
    // Sky Gradient
    vec3 top = vec3(0.2, 0.0, 0.4); // Purple
    vec3 bot = vec3(0.8, 0.2, 0.5); // Pink
    col = mix(bot, top, uv.y);
    
    // Sun
    vec2 sunPos = vec2(0.5, 0.4);
    float d = length(uv - sunPos);
    if (d < 0.25) {
        // Sun gradient
        col = mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.5), uv.y * 2.0);
        
        // Sun Stripes (Cuts)
        float stripe = sin(uv.y * 100.0 + u_time);
        if (stripe > 0.5 && uv.y < 0.4) {
            // Cutout
            // transparent? No, blend with background grid?
            // Just darken 
             col *= 0.0; // Black lines
        }
    }
    
    // Grid floor
    if (uv.y < 0.4) {
        col = vec3(0.1, 0.0, 0.2); // Ground
        
        // Perspective Grid
        // 3D projection approx
        float horizon = 0.4;
        float z = 0.1 / (horizon - uv.y);
        
        // Moving grid
        float speed = 2.0;
        float gridZ = fract(z + u_time * speed);
        float gridX = fract(uv.x * z * 5.0);
        
        // Lines
        if (gridZ < 0.1 || gridX < 0.05) {
            col += vec3(0.0, 0.8, 1.0); // Cyan grid
        }
    }
    
    // Retro Mounts (Spectrum)
    if (uv.y > 0.4 && uv.y < 0.6) {
        // Simple mountains
        // Sample spectrum
        float val = texture2D(u_spectrum, vec2(uv.x, 0.5)).r * 0.3;
        if (uv.y < 0.4 + val) {
            col = vec3(0.0); // Silhouette
        }
    }

    gl_FragColor = vec4(col, 1.0);
}
