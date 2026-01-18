varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

void main() {
    vec2 uv = v_uv;
    
    // Gridify
    float columns = 40.0;
    float x = floor(uv.x * columns);
    vec2 gridUV = fract(uv * vec2(columns, 20.0)); // Aspect
    
    // Rain speed depends on audio column?
    // Sample spectrum across X
    float val = texture2D(u_spectrum, vec2(x/columns, 0.5)).r;
    
    float speed = 2.0 + val * 5.0;
    float y = uv.y + u_time * speed * 0.1;
    
    // Character block random
    float id = floor(y * 20.0); // rows
    float charRand = random(vec2(x, id));
    
    // Fade trail
    float trail = fract(y * 1.0); // Repeats
    // Mask characters
    float charMask = step(0.5, charRand); 
    
    // Brightness = trail * mask
    float bright = (1.0 - trail) * charMask;
    
    // Color: Green Matrix style, but reactive
    vec3 col = vec3(0.1, 1.0, 0.2) * bright;
    
    // Highlight intense columns
    if (val > 0.6) {
        col = vec3(1.0, 1.0, 1.0) * bright; // White hot
    }
    
    // Darkness between "chars"
    if (gridUV.x > 0.8 || gridUV.y > 0.8) col *= 0.0;
    
    gl_FragColor = vec4(col, 1.0);
}
