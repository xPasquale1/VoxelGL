#version 430 core

precision highp float;

in vec4 gl_FragCoord;

uniform sampler2D albedo;
uniform sampler2D lighting;
uniform vec2 windowSize;

out vec4 fragColor;

void main(){
    vec2 uv = gl_FragCoord.xy/windowSize;
    // vec3 base_color = texture(albedo, uv).rgb;
    vec4 light = texture(lighting, uv);
    // vec3 color = mix(base_color, light.rgb, light.a);
    fragColor = vec4(light.rgb, 1.0);
}
