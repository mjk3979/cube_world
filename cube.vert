#version 440
layout(location=0) in vec3 position;

out vec4 v_color;
out vec2 v_texCoord;

uniform mat4 viewMatrix;
uniform float x_rot;
uniform float y_rot;
uniform float z_rot;
uniform vec4 uColor;

mat3 rotation() {
	float s[3];
	s[0] = sin(x_rot);
	s[1] = sin(y_rot);
	s[2] = sin(z_rot);
	float c[3];
	c[0] = cos(x_rot);
	c[1] = cos(y_rot);
	c[2] = cos(z_rot);
    return mat3(vec3(c[0] * c[1], c[0] * s[1] * s[2] - c[2] * s[0], s[0] * s[2] + c[0] * c[2] * s[1])
	          , vec3(c[1] * s[0], c[0] * c[2] + s[0] * s[1] * s[2], c[2] * s[0] * s[1] - c[0] * s[2])
			  , vec3(-s[1], c[1] * s[2], c[1] * c[2]));
}

void main() {
	vec3 worldPosition = rotation() * position;
	gl_Position = viewMatrix * vec4(worldPosition, 1.0f);
	v_texCoord = vec2(worldPosition.x * 0.5 + 0.5, worldPosition.z * 0.5 + 0.5);
	v_color = uColor;
}
