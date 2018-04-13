#include<stdio.h>
#include<stdlib.h>
#include<X11/X.h>
#include<X11/Xlib.h>
#include<GL/glew.h>
#include<GL/gl.h>
#include<GL/glx.h>
#include<GL/glu.h>
#include<unistd.h>
#include <assert.h>
#include <iostream>

#include <math.h>

#include <memory>
#include <map>
#include <experimental/optional>

Display                 *dpy;
Window                  root;
GLint                   att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
XVisualInfo             *vi;
Colormap                cmap;
XSetWindowAttributes    swa;
Window                  win;
GLXContext              glc;
XWindowAttributes       gwa;
XEvent                  xev;

static void checkError() {
	auto error = glGetError();
	if (error != GL_NO_ERROR) {
		std::cerr << "GL Error: " << error << std::endl;
		throw error;
	}
}

struct vec3 {
	float x;
	float y;
	float z;
};
struct vec4 {
	float x;
	float y;
	float z;
	float w;
};
typedef std::array<std::array<float, 4>, 4> mat4;
std::map<std::string, unsigned> bufferMap;
std::map<std::string, GLint> programMap;
std::map<std::string, GLint> shaderMap;

struct camera_t {
	vec3 position;
	float pitch;
	float yaw;
};

const float PI = 3.14159265358f;
static float to_radians(float degrees) {
	return (degrees / 360.f) * 2.f * PI;
}
static float to_degrees(float radians) {
	return (radians / (2 * PI)) * 360.f;
}

static float dot(const vec3 &a, const vec3 &b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

mat4 operator*(const mat4 &a, const mat4 &b) {
	mat4 retval;
	for (unsigned i=0; i < 4; ++i) {
		for (unsigned j=0; j<4; ++j) {
			float sum = 0.f;
			for (unsigned k=0; k<4; ++k) {
				sum += a[k][i] * b[j][k];
			}
			retval[j][i] = sum;
		}
	}
	return retval;
}

mat4 worldToClip(const camera_t &camera, bool ortho) {
	const float pitch = camera.pitch;
	const float yaw = camera.yaw;
	const vec3 &eye = camera.position;
    float cosPitch = cos(pitch);
    float sinPitch = sin(pitch);
    float cosYaw = cos(yaw);
    float sinYaw = sin(yaw);
 
    vec3 xaxis = { cosYaw, 0, -sinYaw };
    vec3 yaxis = { sinYaw * sinPitch, cosPitch, cosYaw * sinPitch };
    vec3 zaxis = { sinYaw * cosPitch, -sinPitch, cosPitch * cosYaw };
 
    // Create a 4x4 view matrix from the right, up, forward and eye position vectors
    mat4 viewMatrix = {{
        {       xaxis.x,            yaxis.x,            zaxis.x,      0 },
        {       xaxis.y,            yaxis.y,            zaxis.y,      0 },
        {       xaxis.z,            yaxis.z,            zaxis.z,      0 },
        { -dot( xaxis, eye ), -dot( yaxis, eye ), -dot( zaxis, eye ), 1 }
    }};

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
	mat4 perMatrix;
	if (ortho) {
		const float r = 1.f;
		const float l = -1.f;
		const float t = 1.f;
		const float b = -1.f;
		const float n = 0.001f;
		const float f = 100.f;
		perMatrix = {{
			{2.f / (r-l), 0, 0, 0},
			{0, 2.f/(t-b), 0, 0},
			{0, 0, -2.f / (f - n), 0},
			{-(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1}
		}};
	} else {
		const float d = 1.f/(tan(3.1415926f / 4.f));
		const float n = .001;
		const float f = 100.f;
		const float a = 16.f / 9.f;
		perMatrix = {{
			{ d/a,0,0,0},
			{ 0,d,0,0},
			{ 0,0,(n+f)/(n-f), -1},
			{ 0,0,2.f*n*f / (n-f), 0}}};
	}
     
    return perMatrix * viewMatrix;
}

static std::map<std::string, GLuint> textureMap;

void drawCuboid(const std::string &name, const vec3 &location, const vec3 &sides, const vec3 &rotation, const vec4 color, const mat4 &viewMatrix, const std::string &fragName, std::experimental::optional<std::string> textureName = {}) {
	static const GLfloat baseVertexes[][3] = {{-0.5, 0.5, -0.5}, {0.5,0.5,-0.5}, {-0.5, -0.5, -0.5}, {0.5, -0.5, -0.5},
	                          {-0.5, 0.5, 0.5}, {0.5, 0.5, 0.5}, {-0.5, -0.5, 0.5}, {0.5, -0.5, 0.5}};
	static const GLint vertexes[][3] = {{0, 1, 2}, {2, 1, 3}, {5, 4, 6}, {5, 6, 7}, {0, 4, 1}, {4, 5, 1}, {2, 3, 7}, {2, 7, 6}, {1, 5, 7}, {1, 7, 3}, {4, 0, 2}, {4, 2, 6}};
	const unsigned numVertexes = 36;
	auto it = bufferMap.find("index");
	if (it == bufferMap.end()) {
		GLuint indexArray[numVertexes];
		for (unsigned i = 0; i < numVertexes / 3; ++i) {
			indexArray[i * 3] = vertexes[i][0];
			indexArray[i * 3 + 1] = vertexes[i][1];
			indexArray[i * 3 + 2] = vertexes[i][2];
			std::cout << indexArray[i*3] << ", " << indexArray[i*3+1] << ", " << indexArray[i*3+2] << std::endl;
		}
		unsigned newBuffer;
		glGenBuffers(1, &newBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * numVertexes, indexArray, GL_STATIC_DRAW);
		bufferMap["index"] = newBuffer;
		it = bufferMap.find("index");
	}
	auto indexBuffer = it->second;
	it = bufferMap.find(name);
	if (it == bufferMap.end()) {
		GLfloat vertexArray[3 * 8];
		for (unsigned i = 0; i < 8; ++i) {
			vertexArray[i*3] = baseVertexes[i][0] * sides.x + location.x;
			vertexArray[i*3+1] = baseVertexes[i][1] * sides.y + location.y;
			vertexArray[i*3+2] = baseVertexes[i][2] * sides.z + location.z;
			std::cout << "V[" << i << "]: " << vertexArray[i*3] << ", " << vertexArray[i*3+1] << ", " << vertexArray[i*3+2] << std::endl << std::flush;
		}
		unsigned newBuffer;
		glGenBuffers(1, &newBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, newBuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 8, vertexArray, GL_STATIC_DRAW);
		bufferMap[name] = newBuffer;
		it = bufferMap.find(name);
	}
	unsigned vertexBuffer = it->second;
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	checkError();

	auto programIt = programMap.find(name);
	if (programIt == programMap.end()) {
		auto vertexShader = shaderMap["cube.vert"];
		auto fragmentShader = shaderMap[fragName];

		auto prog = glCreateProgram();
		glAttachShader(prog, vertexShader);
		glAttachShader(prog, fragmentShader);
		glLinkProgram(prog);
		programMap[name] = prog;
		programIt = programMap.find(name);
		checkError();
	}
	auto program = programIt->second;

	glUseProgram(program);
	glBindAttribLocation(program, 0, "position");
	auto loc = glGetUniformLocation(program, "viewMatrix");
	if (loc >= 0) {
		glUniformMatrix4fv(loc, 1, false, &viewMatrix[0][0]);
	}
	checkError();

	loc = glGetUniformLocation(program, "x_rot");
	glUniform1f(loc, rotation.x);

	loc = glGetUniformLocation(program, "y_rot");
	glUniform1f(loc, rotation.y);

	loc = glGetUniformLocation(program, "z_rot");
	glUniform1f(loc, rotation.z);

	loc = glGetUniformLocation(program, "uColor");
	glUniform4fv(loc, 1, &color.x);

	if (textureName) {
		auto texture = textureMap[*textureName];
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		loc = glGetUniformLocation(program, "shadowTexture");
		glUniform1i(loc, 0);
		checkError();
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glDrawElements(GL_TRIANGLES, numVertexes, GL_UNSIGNED_INT, 0);
	checkError();
}

static vec3 main_rotation = {0.f, 0.f, 0.f};

void draw(camera_t camera) {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDrawBuffer(GL_BACK);
	mat4 viewMatrix;
	if (true) {
		viewMatrix = worldToClip(camera, false);
	} else {
		camera_t shadowCamera;
		shadowCamera.position.x = 0.f;
		shadowCamera.position.y = -1.f;
		shadowCamera.position.z = 0.f;
		shadowCamera.yaw = 0.f;
		shadowCamera.pitch = to_radians(90.f);
		viewMatrix = worldToClip(shadowCamera, true);
	}
	drawCuboid("ground", {0.f, -1.f, 0.f}, {5.f, .1f, 5.f}, {0.f, 0.f, 0.f}, {0.f, 100.f/255.f, 0.f, 1.f}, viewMatrix, "ground.frag", {"shadow"});
	drawCuboid("ship", {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, main_rotation, {0.f, 1.f, 1.f, 1.f}, viewMatrix, "cube.frag");
} 

static std::map<std::string, GLuint> framebufferMap;
void drawShadow() {
	auto it = framebufferMap.find("shadow");
	if (it == framebufferMap.end()) {
		GLuint newFramebuffer;
		glGenFramebuffers(1, &newFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, newFramebuffer);
		checkError();
		framebufferMap["shadow"] = newFramebuffer;
		GLuint newTexture;
		glGenTextures(1, &newTexture);
		glBindTexture(GL_TEXTURE_2D, newTexture);
		checkError();
		textureMap["shadow"] = newTexture;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		checkError();

		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1024, 1024, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
		checkError();
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, newTexture, 0);
		checkError();
		it = framebufferMap.find("shadow");
	}
	glBindFramebuffer(GL_FRAMEBUFFER, it->second);
	glViewport(0, 0, 1024, 1024);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	checkError();


	camera_t shadowCamera;
	shadowCamera.position.x = 0.f;
	shadowCamera.position.y = -3.f;
	shadowCamera.position.z = 0.f;
	shadowCamera.yaw = 0.f;
	shadowCamera.pitch = to_radians(90.f);
	auto viewMatrix = worldToClip(shadowCamera, true);

	drawCuboid("ship", {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, main_rotation, {1.f, 0.f, 0.f, 1.f}, viewMatrix, "cube.frag");
}

std::unique_ptr<char[]> readfile(const std::string &filepath, GLint &length) {
	FILE *f = fopen(filepath.c_str(), "rb");
	if (f) {
		fseek (f, 0, SEEK_END);
		length = ftell (f);
		fseek (f, 0, SEEK_SET);
		auto buffer = std::make_unique<char[]>(length);
		fread (buffer.get(), 1, length, f);
		fclose (f);
		return buffer;
	} else {
		throw 1;
	}
}

void reportShaderCompileErrors(GLint shader) {
	GLint logLength;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
	std::unique_ptr<GLchar[]> log = std::make_unique<GLchar[]>(logLength+1);
	glGetShaderInfoLog(shader, logLength+1, &logLength, log.get());
	std::cerr << "Compile error:" << std::endl << log.get() << std::endl << std::flush;
}

GLint compileShaderProgram(const std::string &shaderFilepath, GLenum type) {
	GLint length;
	auto contents = readfile(shaderFilepath, length);
	auto shader = glCreateShader(type);
	auto contentArray = contents.get();

	glShaderSource(shader, 1, &contentArray, &length);
	glCompileShader(shader);

	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled != GL_TRUE) {
		reportShaderCompileErrors(shader);
		throw 1;
	}
	return shader;
}

int main(int argc, char *argv[]) {

	dpy = XOpenDisplay(NULL);

	if(dpy == NULL) {
		printf("\n\tcannot connect to X server\n\n");
		exit(0);
	}

	root = DefaultRootWindow(dpy);

	vi = glXChooseVisual(dpy, 0, att);

	if(vi == NULL) {
		printf("\n\tno appropriate visual found\n\n");
		exit(0);
	} 
	else {
		printf("\n\tvisual %p selected\n", (void *)vi->visualid); /* %p creates hexadecimal output like in glxinfo */
	}


	cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);

	swa.colormap = cmap;
	swa.event_mask = ExposureMask | KeyPressMask;

	win = XCreateWindow(dpy, root, 0, 0, 1920, 1080, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
	XMapWindow(dpy, win);
	XStoreName(dpy, win, "VERY SIMPLE APPLICATION");

	glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);

	glXMakeCurrent(dpy, win, glc);

	GLenum err=glewInit();
	if(err!=GLEW_OK) {
		// Problem: glewInit failed, something is seriously wrong.
		std::cout << "glewInit failed: " << glewGetErrorString(err) << std::endl;
		exit(1);
	}

	glDisable(GL_DEPTH_TEST); 
	camera_t camera;
	camera.position.x = 0;
	camera.position.y = 0;
	camera.position.z = 3;
	camera.pitch = to_radians(0);
	camera.yaw = to_radians(0);
	shaderMap["cube.vert"] = compileShaderProgram("cube.vert", GL_VERTEX_SHADER);
	shaderMap["cube.frag"] = compileShaderProgram("cube.frag", GL_FRAGMENT_SHADER);
	shaderMap["ground.frag"] = compileShaderProgram("ground.frag", GL_FRAGMENT_SHADER);
	XSelectInput(dpy, win, ExposureMask | KeyPressMask);
	auto x11_fd = ConnectionNumber(dpy);
	timeval tv;
	fd_set in_fds;

	while(1) {
		FD_ZERO(&in_fds);
		FD_SET(x11_fd, &in_fds);
        tv.tv_usec = 16000;
        tv.tv_sec = 0;
		int num_ready = select(x11_fd + 1, &in_fds, NULL, NULL, &tv);

		while(XPending(dpy)) {
			XNextEvent(dpy, &xev);
			if(xev.type == Expose) {
				XGetWindowAttributes(dpy, win, &gwa);
			}

			else if(xev.type == KeyPress) {
				std::cout << "Keycode: " << xev.xkey.keycode << std::endl;
				switch(xev.xkey.keycode) {
					case 36: // return
						glXMakeCurrent(dpy, None, NULL);
						glXDestroyContext(dpy, glc);
						XDestroyWindow(dpy, win);
						XCloseDisplay(dpy);
						exit(0);
						break;
					case 25: // w
						main_rotation.z += to_radians(5.f);
						break;
					case 39: // s
						main_rotation.z -= to_radians(5.f);
						break;
					case 38: // a
						main_rotation.x -= to_radians(5.f);
						break;
					case 40: // d
						main_rotation.x += to_radians(5.f);
						break;
				}
				std::cout << "Rotation: " << to_degrees(main_rotation.x) << ", " << to_degrees(main_rotation.z) << std::endl;
			}
		}
		//camera.yaw += to_radians(.4f);
		//std::cout << "Yaw: " << to_degrees(camera.yaw) << std::endl;
		if (camera.yaw > to_radians(360)) {
			camera.yaw -= to_radians(360.f);
		}
		if (camera.pitch > to_radians(360)) {
			camera.pitch -= to_radians(360.f);
		}
		drawShadow();
		glViewport(0, 0, gwa.width, gwa.height);
		draw(camera); 
		glXSwapBuffers(dpy, win);
	}
}
