varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    float r = length(uv);
    float a = atan(uv.y, uv.x);
    
    float spin = u_time * 2.0;
    float spiral = a + r * 10.0 + spin;
    
    float bass = texture2D(u_spectrum, vec2(0.05, 0.5)).r;
    
    float val = sin(spiral * 5.0);
    
    // Hypnotic colors
    vec3 col = vec3(0.0);
    
    col.r = sin(val + u_time) * 0.5 + 0.5;
    col.g = sin(val + u_time + 2.0) * 0.5 + 0.5;
    col.b = sin(val + u_time + 4.0) * 0.5 + 0.5;
    
    // Eye pulsing
    float eye = 1.0 - smoothstep(0.0, 0.5 + bass*0.5, r);
    col *= eye;
    
    // Pupil
    if (r < 0.1) col = vec3(0.0);

    gl_FragColor = vec4(col, 1.0);
}
