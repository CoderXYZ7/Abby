varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    float r = length(uv);
    float a = atan(uv.y, uv.x);
    // Normalize ang
    float aNorm = (a + 3.14159) / 6.28318;
    
    // Bars
    float bars = 32.0;
    float aStep = floor(aNorm * bars) / bars;
    
    // Mirror spectrum
    float specIdx = abs(aStep - 0.5) * 2.0;
    float val = texture2D(u_spectrum, vec2(specIdx, 0.5)).r;
    
    // Bar length
    float len = 0.3 + val * 0.5;
    
    vec3 col = vec3(0.0);
    
    if (r > 0.3 && r < len) {
        // Gap between bars
        if (fract(aNorm * bars) < 0.8) {
             // Rainbow
             col = vec3(aNorm, 1.0 - aNorm, 0.5 + 0.5*sin(u_time));
        }
    }
    
    gl_FragColor = vec4(col, 1.0);
}
