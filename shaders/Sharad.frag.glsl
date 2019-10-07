#version 440 compatibility

uniform sampler2DRect uDepthBuffer;
uniform sampler2D uSharadTexture;
uniform float uAmbientBrightness;
uniform float uTime;
uniform float uSceneScale;
uniform float uFarClip;
uniform vec2 uViewportPos;

// inputs
in vec3  vPosition;
in vec2  vTexCoords;
in float vTime;

// outputs
layout(location = 0) out vec4 oColor;

void main() {
    if (vTime > uTime) {
        discard;
    }

    float sharadDistance  = length(vPosition);
    float surfaceDistance = texture(uDepthBuffer, gl_FragCoord.xy - uViewportPos).r * uFarClip;

    if (sharadDistance < surfaceDistance) {
        discard;
    }

    float val = texture(uSharadTexture, vTexCoords).r;
    val = mix(1, val, clamp((uTime - vTime), 0, 1));

    oColor.r = pow(val,  0.5);
    oColor.g = pow(val,  2.0);
    oColor.b = pow(val, 10.0);
    oColor.a = 1.0 - clamp((sharadDistance - surfaceDistance) * uSceneScale / 30000, 0.1, 1.0);

    gl_FragDepth = sharadDistance / uFarClip;
}