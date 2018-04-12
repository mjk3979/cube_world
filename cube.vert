#version 440
layout(location=0) in vec3 position;

uniform vec3 eye;
uniform float pitch;
uniform float yaw;
uniform float x_rot;
uniform float y_rot;
uniform float z_rot;

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

mat4 worldToClip() {
    float cosPitch = cos(pitch);
    float sinPitch = sin(pitch);
    float cosYaw = cos(yaw);
    float sinYaw = sin(yaw);
 
    vec3 xaxis = { cosYaw, 0, -sinYaw };
    vec3 yaxis = { sinYaw * sinPitch, cosPitch, cosYaw * sinPitch };
    vec3 zaxis = { sinYaw * cosPitch, -sinPitch, cosPitch * cosYaw };
 
    // Create a 4x4 view matrix from the right, up, forward and eye position vectors
    mat4 viewMatrix = {
        vec4(       xaxis.x,            yaxis.x,            zaxis.x,      0 ),
        vec4(       xaxis.y,            yaxis.y,            zaxis.y,      0 ),
        vec4(       xaxis.z,            yaxis.z,            zaxis.z,      0 ),
        vec4( -dot( xaxis, eye ), -dot( yaxis, eye ), -dot( zaxis, eye ), 1 )
    };

	/*
	const float left = -3.f;
	const float right = 3.f;
	const float bottom = -3.f;
	const float top = 3.f;
	const float far = 100.f;
	const float near = .01f;
	mat4 perMatrix = mat4(
		vec4(near / right, 0.f, 0.f, 0.f),
		vec4(0.f, near / top, 0.f, 0.f),
		vec4(0.f, 0.f, -(far + near) / (far - near), -1.f),
		vec4(0.f, 0.f, -2.f * near * far / (far - near), 0.f));
	*/
	const float d = 1.f/(tan(3.1415926f / 4.f));
	const float n = .001;
	const float f = 100.f;
	const float a = 16.f / 9.f;
	mat4 perMatrix = mat4(
		vec4(d/a,0,0,0),
		vec4(0,d,0,0),
		vec4(0,0,(n+f)/(n-f), -1),
		vec4(0,0,2.f*n*f / (n-f), 0));
     
    return perMatrix * viewMatrix;
}

void main() {
	gl_Position = worldToClip() * vec4(rotation() * position, 1.0f);
}
