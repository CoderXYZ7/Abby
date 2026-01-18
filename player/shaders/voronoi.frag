varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

vec2 hash2( vec2 p ) {
    return fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453);
}

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    uv *= 2.0; // Zoom out
    
    float bass = texture2D(u_spectrum, vec2(0.05, 0.5)).r;
    
    // Grid cells
    vec2 i_st = floor(uv);
    vec2 f_st = fract(uv);
    
    float m_dist = 1.0;  // min distance
    vec2 m_point;        // closest point
    
    // Check 9 neighbor cells
    for (int y= -1; y <= 1; y++) {
        for (int x= -1; x <= 1; x++) {
            vec2 neighbor = vec2(float(x),float(y));
            vec2 point = hash2(i_st + neighbor);
            
            // Animate point
            // Bass makes them excited
            point = 0.5 + 0.5*sin(u_time * (1.0+bass) + 6.2831*point);
            
            vec2 diff = neighbor + point - f_st;
            float dist = length(diff);
            
            if( dist < m_dist ) {
                m_dist = dist;
                m_point = point;
            }
        }
    }
    
    // Cell color
    vec3 col = vec3(0.0);
    col += m_dist; // gradient
    
    // Isolate cell center ?
    // col += 1.0 - step(0.02, m_dist);
    
    // Grid lines
    // Not easy in simple Voronoi, but we can color based on distance
    
    // Invert for "cells"
    col = vec3(1.0 - m_dist);
    
    // Tint red/organic
    col *= vec3(1.0, 0.2, 0.1);
    
    // Pulse
    col *= (0.5 + bass);

    gl_FragColor = vec4(col, 1.0);
}
