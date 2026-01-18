varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

// --- Simplex NoiseUtils ---
vec3 permute(vec3 x) { return mod(((x*34.0)+1.0)*x, 289.0); }

float snoise(vec2 v){
  const vec4 C = vec4(0.211324865405187, 0.366025403784439,
           -0.577350269189626, 0.024390243902439);
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v -   i + dot(i, C.xx);
  vec2 i1;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  vec4 x12 = x0.xyxy + C.xxzz;
  x12.xy -= i1;
  i = mod(i, 289.0);
  vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
  + i.x + vec3(0.0, i1.x, 1.0 ));
  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;
  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}

// --- Organic FBM ---
float fbm(vec2 p) {
    float total = 0.0;
    float persistence = 0.5;
    float scale = 1.0;
    
    // 3 Octaves for smoothness
    for(int i=0; i<3; i++) {
        total += snoise(p * scale) * persistence;
        scale *= 2.0;
        persistence *= 0.5;
    }
    return total;
}

void main() {
    vec2 uv = v_uv * 2.0 - 1.0;
    
    // Very gentle movement
    // Time is the main driver
    float t = u_time * 0.15;
    
    // Audio Integration: 
    // We average the lowest frequencies to get a very stable "Bass Presence" value
    // This value will only affect the brightness/color shift, NOT the geometry, to prevent twitching.
    float bass = 0.0;
    for(float i=0.0; i<0.1; i+=0.01) {
        bass += texture2D(u_spectrum, vec2(i, 0.5)).r;
    }
    bass /= 10.0;
    // Smoothstep to ignore low noise floor
    float energy = smoothstep(0.1, 0.6, bass); 
    
    // Domain Warping Logic for Liquid feel
    vec2 p = uv;
    
    // Layer 1
    vec2 q = vec2(0.0);
    q.x = fbm(p + vec2(0.0, 0.0) + t);
    q.y = fbm(p + vec2(5.2, 1.3) - t);
    
    // Layer 2
    vec2 r = vec2(0.0);
    r.x = fbm(p + 4.0*q + vec2(t, t));
    r.y = fbm(p + 4.0*q + vec2(0.0, t));
    
    // Final
    float f = fbm(p + 4.0*r);
    
    // Coloring
    // Deep organic colors: Dark Teal -> Blue -> Soft White
    vec3 col = mix(vec3(0.0, 0.1, 0.2), vec3(0.0, 0.3, 0.4), clamp(f*f*4.0, 0.0, 1.0));
    
    // Mix in warmer tones based on 'r' (the warped coordinate)
    col = mix(col, vec3(0.3, 0.05, 0.1), clamp(length(q), 0.0, 1.0));
    
    // Highlights modulated by audio ENERGY
    // When music is loud, the liquid glows brighter, but doesn't jerk around.
    vec3 glowColor = vec3(0.2, 0.8, 1.0);
    col = mix(col, glowColor, clamp(length(r.x), 0.0, 1.0) * energy);
    
    // Soft Vignette
    col *= 1.2 - dot(uv, uv);
    
    gl_FragColor = vec4(col, 1.0);
}
