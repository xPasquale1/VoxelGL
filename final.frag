#version 430 core

precision highp float;

in vec4 gl_FragCoord;

uniform sampler2D albedo;
uniform sampler2D lighting;
uniform vec2 windowSize;

out vec4 fragColor;

void main(){
    vec2 uv = gl_FragCoord.xy/windowSize;
    vec4 light = texture(lighting, uv);
    vec2 centered_uv = uv*2.0-vec2(1.0);
    centered_uv.x *= windowSize.x/windowSize.y;
    if(length(centered_uv) < 0.005) light.rgb = vec3(0, 1, 0);
    fragColor = vec4(light.rgb, 1.0);
}
