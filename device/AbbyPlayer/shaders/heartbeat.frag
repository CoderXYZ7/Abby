varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv;
    
    // Simulate heart beat monitor trace
    float x = uv.x;
    float t = u_time * 2.0;
    
    // Beat pulse from bass
    float bass = texture2D(u_spectrum, vec2(0.05, 0.5)).r;
    float beat = smoothstep(0.4, 0.6, bass); // Trigger
    
    // Base line
    float y = 0.5;
    
    // EKG Shape
    // Create a pulse that moves across screen?
    // Or just a static shape that jumps?
    // Moving scan:
    float scanX = fract(t);
    
    // Pulse shape at specific intervals
    if (abs(uv.x - scanX) < 0.2) {
        // Add blip
        // e^(-x^2) shape
        float dx = (uv.x - scanX) * 20.0;
        y += sin(dx) * exp(-dx*dx) * (1.0 + beat);
    }
    
    float dist = abs(uv.y - y);
    float intensity = 0.005 / (dist + 0.001);
    
    // Fade trail history
    float history = 1.0 - smoothstep(0.0, 0.5, scanX - uv.x);
    if (uv.x > scanX) history = 0.0; // Clear ahead
    
    // Always show bright head
    float head = 1.0 - smoothstep(0.0, 0.02, abs(uv.x - scanX));
    
    vec3 col = vec3(0.0, 1.0, 0.2) * (intensity * (0.2 + 0.8*head + 0.5*history));

    gl_FragColor = vec4(col, 1.0);
}
