varying vec2 v_uv;
uniform float u_time;
uniform sampler2D u_spectrum;

float rand(float n){return fract(sin(n) * 43758.5453123);}

void main() {
    vec2 uv = v_uv;
    
    // Parallax Layers of "Buildings"
    vec3 col = vec3(0.05, 0.0, 0.1); // Sky
    
    float bass = texture2D(u_spectrum, vec2(0.1, 0.5)).r;
    
    for(float i=1.0; i<=3.0; i++) {
        float speed = i * 0.2;
        float x = uv.x + u_time * speed;
        
        // Building height
        float id = floor(x * 10.0);
        float h = rand(id + i * 100.0) * 0.5 + 0.1;
        
        // Bounce with audio
        if (i==1.0) h += bass * 0.2;
        
        // Draw
        if (uv.y < h) {
             // Silhouette color
             float shade = i / 3.0;
             vec3 bCol = vec3(0.0);
             
             // Windows
             vec2 winUV = fract(vec2(x * 10.0, uv.y * 20.0));
             if (winUV.x > 0.4 && winUV.y > 0.4 && rand(id + floor(uv.y*20.0)) > 0.5) {
                 bCol = vec3(1.0, 1.0, 0.0) * shade; // Lights
             } else {
                 bCol = vec3(0.1, 0.0, 0.2) * shade;
             }
             col = mix(col, bCol, 1.0);
        }
    }

    gl_FragColor = vec4(col, 1.0);
}
