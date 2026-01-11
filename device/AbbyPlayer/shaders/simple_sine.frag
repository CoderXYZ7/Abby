varying vec2 v_uv;
uniform float u_time;
uniform float u_pitch;
uniform sampler2D u_spectrum;

void main() {
    vec2 uv = v_uv;
    
    // Sample bass frequency for amplitude modulation
    float bass = texture2D(u_spectrum, vec2(0.05, 0.5)).r;
    
    // Base parameters
    float freq = 10.0 + (u_pitch * 0.005); // Modulate wave density slightly by pitch
    float amp = 0.2 + (bass * 0.1);    // Modulate height slightly by bass
    float speed = 2.0;
    
    // Calculate Sine
    // y = sin(x * freq + time) * amp
    // We center it at y=0.5
    float waveY = 0.5 + sin(uv.x * freq + u_time * speed) * amp;
    
    // Create the line
    float dist = abs(uv.y - waveY);
    
    // Glow/Thickness
    // Sharper line than oscilloscope
    float intensity = 0.01 / (dist + 0.001);
    
    vec3 color = vec3(0.0, 0.8, 1.0) * intensity; // Cyan color
    
    // Add a very subtle "echo" or ghosting with a second sine wave, slightly offset
    float waveY2 = 0.5 + sin(uv.x * (freq * 0.9) + u_time * (speed * 0.8)) * (amp * 0.8);
    float dist2 = abs(uv.y - waveY2);
    color += vec3(0.5, 0.0, 0.5) * (0.005 / (dist2 + 0.001)); // Purple ghost

    gl_FragColor = vec4(color, 1.0);
}
