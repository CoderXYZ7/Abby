varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }

float noise(vec2 x) {
	vec2 i = floor(x);
	vec2 f = fract(x);
	float a = hash(i);
	float b = hash(i + vec2(1.0, 0.0));
	float c = hash(i + vec2(0.0, 1.0));
	float d = hash(i + vec2(1.0, 1.0));
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

void main() {
    vec2 uv = v_uv;
    
    float bass = texture2D(u_spectrum, vec2(0.1, 0.5)).r;
    
    // Fire coordinate system
    vec2 q = uv;
    q.x *= 2.0;
    q.y *= 1.5;
    q.y -= 0.2; // Move down
    
    // Noise movement
    float n = noise(q * 8.0 + vec2(0.0, -u_time * 3.0));
    
    // Shape the flame
    float t = 1.0 - uv.y; // Bottom is 1.0
    float flame = n * t * 1.5;
    
    // Center it
    float center = 1.0 - abs(uv.x - 0.5) * 3.0;
    flame *= clamp(center, 0.0, 1.0);
    
    // React to bass: boost height
    flame *= (1.0 + bass);
    
    // Color map
    vec3 col = vec3(0.0);
    if (flame > 0.2) col = vec3(0.5, 0.0, 0.0); // Dark red
    if (flame > 0.4) col = vec3(1.0, 0.2, 0.0); // Red
    if (flame > 0.6) col = vec3(1.0, 0.8, 0.0); // Yellow
    if (flame > 0.8) col = vec3(1.0, 1.0, 0.8); // White hot
    
    gl_FragColor = vec4(col, 1.0);
}
