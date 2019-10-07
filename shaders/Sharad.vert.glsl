#version 440 compatibility

uniform mat4 uMatModelView;
uniform mat4 uMatProjection;
uniform float uHeightScale;
uniform float uRadius;

// inputs
layout(location = 0) in vec3  iPosition;
layout(location = 1) in vec2  iTexCoords;
layout(location = 2) in float iTime;

// outputs
out vec3  vPosition;
out vec2  vTexCoords;
out float vTime;

void main() {
    vTexCoords = iTexCoords;
    vTime      = iTime;

    float height = vTexCoords.y < 0.5 ? uRadius + 10000 * uHeightScale : uRadius - 10100 * uHeightScale;

    vPosition   = (uMatModelView * vec4(iPosition * height, 1.0)).xyz;
    gl_Position =  uMatProjection * vec4(vPosition, 1);
}