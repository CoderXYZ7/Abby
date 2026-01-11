varying vec2 v_uv;
uniform float u_time;
uniform float u_pitch;
uniform float u_pos;
uniform float u_duration;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv;
    // Mirror
    if (uv.x > 0.5) uv.x = 1.0 - uv.x;
    uv.x *= 2.0;

    float val = texture2D(u_spectrum, vec2(uv.x, 0.5)).r;
    float dist = abs(uv.y - 0.5) * 2.0;
    
    vec3 color = vec3(0.0);
    
    if (dist < val) {
        // Color based on pitch/time
        color = vec3(0.5 + 0.5*sin(u_time), uv.x, 0.8);
    }
    
    // Add "High Energy" flash if pitch is high
    if (u_pitch > 1000.0) {
            color += vec3(0.1, 0.0, 0.0); 
    }

    // Progress bar
    if (v_uv.y < 0.02) {
            float progress = u_pos / u_duration;
            if (v_uv.x < progress) color = vec3(1.0, 1.0, 1.0);
    }

    gl_FragColor = vec4(color, 1.0);
}
