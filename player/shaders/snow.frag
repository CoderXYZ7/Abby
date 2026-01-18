varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

float hash(float n) { return fract(sin(n)*43758.5453); }

void main() {
    vec2 uv = v_uv;
    // Aspect ratio correction approximation
    uv.x *= 1.6;
    
    float bass = texture2D(u_spectrum, vec2(0.05, 0.5)).r;
    float wind = u_time * 0.5 + bass * 0.1;
    
    vec3 col = vec3(0.05, 0.05, 0.1); // Night sky
    
    for(float i=0.0; i<30.0; i++) {
        // Random pos
        float x = hash(i);
        float y = hash(i * 1.23);
        float z = hash(i * 2.34); // depth/size
        
        // Animate Y
        float fallSpeed = 0.2 + z * 0.3;
        float curY = fract(y - u_time * fallSpeed);
        
        // Wiggle X
        float curX = fract(x + sin(u_time + z*10.0) * 0.1 * (1.0+bass));
        
        // Draw flake
        float d = length(uv - vec2(curX * 1.6, curY));
        float size = 0.005 + z * 0.005;
        
        // Flash on beat?
        float brightness = 0.8 + 0.2 * sin(u_time * 10.0 + i);
        
        float intensity = smoothstep(size, size*0.5, d);
        col += vec3(intensity * brightness);
    }
    
    gl_FragColor = vec4(col, 1.0);
}
