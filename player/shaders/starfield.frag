varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

// Simple hash for randomness
float hash12(vec2 p) {
	vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    
    // Audio energy for speed
    float bass = texture2D(u_spectrum, vec2(0.05, 0.5)).r;
    float m = texture2D(u_spectrum, vec2(0.5, 0.5)).r;
    float speed = 0.5 + bass * 2.0;
    
    vec3 color = vec3(0.0);
    
    // Multiple layers of stars
    for(float i=0.0; i<3.0; i++) {
        // Projection
        float z = fract(i/3.0 - u_time * 0.1 * speed);
        float fade = smoothstep(0.0, 0.1, z) * smoothstep(1.0, 0.9, z);
        
        vec2 st = uv * (1.0 + z * 4.0); // Simple perspective
        
        // Grid for stars
        vec2 id = floor(st * 10.0);
        vec2 gridUV = fract(st * 10.0) - 0.5;
        
        float n = hash12(id + i * 123.4);
        
        // Render Star
        // Only if noise > threshold
        if (n > 0.8) {
            float size = (n - 0.8) * 5.0 * fade;
            // React to mids: puff up stars
            size *= (1.0 + m * 2.0);
            
            float d = length(gridUV);
            float brightness = 0.05 / (d + 0.001);
            brightness *= size;
            
            // Color shift based on layer/z
            vec3 starColor = mix(vec3(0.5, 0.8, 1.0), vec3(1.0, 0.8, 0.6), z);
            color += starColor * brightness;
        }
    }
    
    gl_FragColor = vec4(color, 1.0);
}
