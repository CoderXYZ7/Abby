
varying vec2 v_uv;
uniform float u_time;
uniform float u_pitch;
uniform float u_pos;
uniform float u_duration;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv;
    
    // Sample spectrum data for this horizontal position
    float spectrumValue = texture2D(u_spectrum, vec2(uv.x, 0.5)).r;
    
    // Create frequency bars
    float barHeight = spectrumValue * 0.8;
    float bar = (uv.y < barHeight) ? 1.0 : 0.0;
    
    // Color gradient based on frequency (low=red, mid=green, high=blue)
    vec3 lowColor = vec3(1.0, 0.2, 0.2);    // Red for bass
    vec3 midColor = vec3(0.2, 1.0, 0.2);    // Green for mids
    vec3 highColor = vec3(0.2, 0.5, 1.0);   // Blue for highs
    
    vec3 color;
    if (uv.x < 0.33) {
        color = mix(lowColor, midColor, uv.x * 3.0);
    } else if (uv.x < 0.66) {
        color = mix(midColor, highColor, (uv.x - 0.33) * 3.0);
    } else {
        color = mix(highColor, vec3(0.5, 0.2, 1.0), (uv.x - 0.66) * 3.0);
    }
    
    // Apply bar intensity
    color *= bar;
    
    // Add glow effect
    float glow = exp(-abs(uv.y - barHeight) * 20.0) * spectrumValue * 0.3;
    color += glow * color;
    
    // Add grid lines for frequency divisions
    float gridLine = 0.0;
    for (float i = 0.0; i < 1.0; i += 0.1) {
        if (abs(uv.x - i) < 0.002) {
            gridLine = 0.2;
        }
    }
    color += vec3(gridLine);
    
    // Progress bar at bottom
    float progressBar = 0.0;
    if (uv.y < 0.02 && uv.x < (u_pos / u_duration)) {
        progressBar = 0.5;
    }
    color += vec3(progressBar, progressBar * 0.5, 0.0);
    
    gl_FragColor = vec4(color, 1.0);
}
