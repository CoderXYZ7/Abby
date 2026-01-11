varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

float rand(vec2 n) { 
	return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

void main() {
    vec2 uv = v_uv;
    
    float high = texture2D(u_spectrum, vec2(0.8, 0.5)).r; // High freq
    
    // Blocky distortion
    float blockSize = 10.0 + high * 50.0;
    vec2 blockUV = floor(uv * blockSize) / blockSize;
    
    float noise = rand(blockUV + u_time);
    
    // Displacement
    float disp = 0.0;
    if (noise > 0.9 && high > 0.1) {
        disp = (rand(vec2(u_time)) - 0.5) * 0.1;
    }
    
    vec2 finalUV = uv + vec2(disp, 0.0);
    
    // RGB Shift
    float r = texture2D(u_spectrum, vec2(finalUV.x + 0.01, 0.5)).r; // Just using spectrum as visual source is boring
    // Let's generate a pattern for base
    
    float stripes = sin(finalUV.y * 50.0 + u_time);
    vec3 col = vec3(stripes);
    
    // Color shift on Glitch
    if (abs(disp) > 0.001) {
        col.r = 1.0;
        col.b = 0.0;
    }
    
    // Scanline
    if (mod(uv.y * 100.0, 2.0) < 1.0) col *= 0.5;

    gl_FragColor = vec4(col, 1.0);
}
