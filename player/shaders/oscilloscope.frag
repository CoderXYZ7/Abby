varying vec2 v_uv;
uniform float u_time;
uniform float u_duration;
uniform float u_pos;
uniform sampler2D u_spectrum;

// CRT Distortion
vec2 warp(vec2 uv) {
    vec2 delta = uv - 0.5;
    float delta2 = dot(delta.xy, delta.xy);
    float delta4 = delta2 * delta2;
    float deltaOffset = delta2 * 0.2;
    
    return uv + delta * deltaOffset;
}

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() {
    // 1. CRT Warp
    vec2 uv = warp(v_uv);
    
    // Black out edges of CRT
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    // 2. Grid (Reticle)
    float gridWidth = 0.005;
    float gridBrightness = 0.15;
    
    // Major lines
    float gX = step(0.98, fract(uv.x * 10.0));
    float gY = step(0.98, fract(uv.y * 8.0));
    vec3 gridColor = vec3(0.0, 0.4, 0.0) * (gX + gY) * gridBrightness;
    
    // Center axes
    float cX = 1.0 - smoothstep(0.0, 0.005, abs(uv.x - 0.5));
    float cY = 1.0 - smoothstep(0.0, 0.005, abs(uv.y - 0.5));
    gridColor += vec3(0.0, 0.6, 0.0) * (cX + cY) * 0.2;

    // 3. Trace (Spectrum Line)
    // Map frequency (X) to Height (Y)
    float val = texture2D(u_spectrum, vec2(uv.x, 0.5)).r;
    
    // Scale amplitude
    val = val * 0.8; 
    
    // Center it vertically? Or assume bottom up? Oscilloscopes usually center AC signals.
    // FFT produces 0..1 magnitude. Let's make it look like a symmetric wave or just positive.
    // A nice effect for FFT is mirroring it so it looks like a pulse in the center.
    // Let's try mirroring X first to make it look central.
    
    float symX = abs(uv.x - 0.5) * 2.0;
    float symVal = texture2D(u_spectrum, vec2(symX, 0.5)).r;
    
    // Create a "noisy" line
    float lineY = 0.5 + symVal * 0.4 * sin(uv.x * 100.0 + u_time * 10.0); // Faking symmetry modulation
    // Actually, just raw FFT looks cleaner for "Spectrum Analyzer Mode"
    // Let's stick to a raw FFT line trace for clarity but green and glowing.
    
    float dist = abs(uv.y - (0.1 + val * 0.8)); // Offset slightly up
    
    // Glow calculation
    float intensity = 0.004 / (dist + 0.001);
    
    // Beam color (Phosphor Green)
    vec3 beamColor = vec3(0.2, 1.0, 0.4) * intensity;
    
    // Add jitter/noise
    beamColor *= (0.9 + 0.1 * rand(uv * u_time));
    
    // 4. Scanlines
    float scanline = sin(uv.y * 400.0 + u_time * 5.0) * 0.1;
    
    // 5. Final Composition
    vec3 finalColor = gridColor + beamColor;
    
    // Apply scanline darken
    finalColor -= scanline;
    
    // Vignette
    float vig = (0.5 - abs(uv.x - 0.5)) * (0.5 - abs(uv.y - 0.5));
    finalColor *= (0.5 + 10.0 * vig); // brighten center

    // Progress bar (Subtle blip at bottom)
    if (uv.y < 0.02 && uv.x < (u_pos/u_duration)) {
         finalColor += vec3(0.0, 0.5, 0.0);
    }

    gl_FragColor = vec4(finalColor, 1.0);
}
