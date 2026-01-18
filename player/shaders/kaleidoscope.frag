varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263, 0.416, 0.557);
    return a + b * cos(6.28318 * (c * t + d));
}

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    
    float bass = texture2D(u_spectrum, vec2(0.05, 0.5)).r;
    
    // Number of mirrors
    float N = 6.0;
    
    // Convert to Polar
    float r = length(uv);
    float a = atan(uv.y, uv.x);
    
    // Repeat Angle
    // Add rotation
    a += u_time * 0.2;
    a = mod(a, 6.28318 / N);
    a = abs(a - 3.14159 / N);
    
    // Convert back to cartesian for texture sampling
    vec2 p = r * vec2(cos(a), sin(a));
    
    // Animate UVs
    p -= vec2(1.0 + sin(u_time), 0.0); // Offset center
    
    // Texture: Use spectrum as a "plasma" field
    float val = 0.0;
    vec2 q = p;
    float len = length(p);
    
    // Iterate to create complex pattern
    for(float i=0.0; i<3.0; i++){
        q = abs(q) / dot(q,q) - 0.5; // Kalibox fractalish
        val += length(q);
    }
    
    val = sin(val * 10.0 + u_time);
    
    // Colors
    vec3 col = palette(len + u_time + bass);
    
    // Darken lines
    col *= smoothstep(0.0, 0.2, abs(val));
    
    // Audio bump
    col += vec3(bass * 0.5); 

    gl_FragColor = vec4(col, 1.0);
}
