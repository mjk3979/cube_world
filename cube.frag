#version 440

layout(location=0) out vec4 gl_FragColor;
in vec4 v_color;
void main() {
	gl_FragColor = v_color;
}
