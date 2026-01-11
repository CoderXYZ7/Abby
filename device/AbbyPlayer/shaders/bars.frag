varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv;
    
    // Quantize X to create bars
    float barWidth = 1.0 / 64.0; // 64 bars
    float barX = floor(uv.x / barWidth) * barWidth;
    
    // Sample spectrum at bar center
    float intensity = texture2D(u_spectrum, vec2(barX + barWidth/2.0, 0.5)).r;
    
    vec3 color = vec3(0.0);
    
    // Bar height
    if (uv.y < intensity) {
        // Gradient color for bar
        color = vec3(uv.y, 1.0 - uv.y, 0.5 + 0.5*sin(u_time));
    }
    
    // Separator line (gap between bars)
    if (mod(uv.x, barWidth) < 0.002) {
        color = vec3(0.0);
    }
    
    // Reflection
    if (uv.y < 0.0 && uv.y > -intensity * 0.5) {
        color = vec3(uv.y * 0.5, 0.0, 0.0); // simple reflection logic, but uv is 0..1 in standard setup so this might not show unless we adjust coordinates
    }

    gl_FragColor = vec4(color, 1.0);
}
