varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    float r = length(uv);
    
    float bass = texture2D(u_spectrum, vec2(0.05, 0.5)).r;
    
    // Ripple calculation
    // Sin wave expanding from center
    float freq = 20.0;
    float speed = 5.0;
    
    // Modulate amplitude by bass
    float amp = 0.05 + bass * 0.1;
    
    float ripple = sin(r * freq - u_time * speed);
    
    // Distort UV
    vec2 distUV = uv + (uv/r) * ripple * amp;
    
    // Color map based on distorted radius
    float d = length(distUV);
    
    vec3 col = vec3(0.0);
    // Rings
    col += vec3(0.1, 0.5, 1.0) * (1.0 - smoothstep(0.0, 0.1, abs(ripple)));
    
    // Center glow
    col += vec3(1.0, 1.0, 1.0) * (1.0/d) * 0.05;

    gl_FragColor = vec4(col, 1.0);
}
