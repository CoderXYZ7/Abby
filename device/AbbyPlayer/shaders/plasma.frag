varying vec2 v_uv;
uniform float u_time;
uniform float u_pitch;
uniform float u_pos;
uniform float u_duration;
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
    float freq = texture2D(u_spectrum, vec2(abs(uv.x), 0.5)).r;
    vec2 uv0 = uv;
    vec3 finalColor = vec3(0.0);
    
    // Warp time with pitch
    float t = u_time * (0.8 + u_pitch * 0.0005);

    for (float i = 0.0; i < 3.0; i++) {
        uv = fract(uv * 1.5) - 0.5;
        float d = length(uv) * exp(-length(uv0));
        vec3 col = palette(length(uv0) + i * 0.4 + t * 0.4);
        d = sin(d * 8.0 + t) / 8.0;
        d = abs(d);
        d = pow(0.01 / d, 1.2);
        finalColor += col * d;
    }
    float bass = texture2D(u_spectrum, vec2(0.05, 0.5)).r;
    finalColor *= (0.8 + bass * 0.5);
    
    // Progress bar at bottom
    if (v_uv.y < 0.02) {
            float progress = u_pos / u_duration;
            if (v_uv.x < progress) finalColor = vec3(0.0, 1.0, 0.0);
    }

    gl_FragColor = vec4(finalColor, 1.0);
}
