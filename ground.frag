#version 440

layout(location=0) out vec4 gl_FragColor;
in vec4 v_color;
in vec2 v_texCoord;
uniform sampler2D shadowTexture;
void main() {
	gl_FragColor = v_color;
	gl_FragColor.rgb *= 1.f - texture(shadowTexture, v_texCoord).r;
}
