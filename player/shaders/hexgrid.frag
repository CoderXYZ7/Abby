varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

// Hexagon SDF
// p is coords, r is radius
float hexDist(vec2 p) {
    p = abs(p);
    float c = dot(p, normalize(vec2(1.0, 1.73)));
    return max(c, p.x);
}

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    // Tiling
    uv *= 4.0;
    
    // Skew for Hex grid
    vec2 r = vec2(1.0, 1.73);
    vec2 h = r * 0.5;
    
    vec2 a = mod(uv, r) - h;
    vec2 b = mod(uv - h, r) - h;
    
    vec2 gv = dot(a, a) < dot(b, b) ? a : b;
    
    // ID for cell
    vec2 id = uv - gv;
    
    // Sample spectrum based on ID
    // Normalize ID to 0..1 range approximately
    vec2 normID = clamp((id + 4.0) / 8.0, 0.0, 1.0);
    float val = texture2D(u_spectrum, vec2(length(normID)*0.5, 0.5)).r;
    
    float dist = hexDist(gv);
    float width = 0.05;
    
    // Outline
    float glow = 0.01 / abs(dist - 0.45);
    
    // Fill if loud
    float fill = smoothstep(0.45, 0.4, dist) * val;
    
    vec3 col = vec3(0.1, 0.5, 1.0) * glow;
    col += vec3(1.0, 0.2, 0.5) * fill;
    
    gl_FragColor = vec4(col, 1.0);
}
