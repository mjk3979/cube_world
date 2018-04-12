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

#include <memory>
#include <map>

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

struct vec3 {
	float x;
	float y;
	float z;
};
std::map<std::string, unsigned> bufferMap;
std::map<std::string, GLint> programMap;
std::map<std::string, GLint> shaderMap;

struct camera_t {
	vec3 position;
	float pitch;
	float yaw;
};

camera_t camera;

const float PI = 3.14159265358;
static float to_radians(float degrees) {
	return (degrees / 360.f) * 2.f * PI;
}
static float to_degrees(float radians) {
	return (radians / (2 * PI)) * 360.f;
}

void drawCuboid(const std::string &name, const vec3 &location, const vec3 &sides, const vec3 &rotation) {
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

	auto programIt = programMap.find(name);
	if (programIt == programMap.end()) {
		auto vertexShader = shaderMap["cube.vert"];
		auto fragmentShader = shaderMap["cube.frag"];

		auto prog = glCreateProgram();
		glAttachShader(prog, vertexShader);
		glAttachShader(prog, fragmentShader);
		glLinkProgram(prog);
		programMap[name] = prog;
		programIt = programMap.find(name);
	}
	auto program = programIt->second;

	glUseProgram(program);
	glBindAttribLocation(program, 0, "position");
	auto loc = glGetUniformLocation(program, "eye");
	glUniform3f(loc, camera.position.x, camera.position.y, camera.position.z);

	loc = glGetUniformLocation(program, "pitch");
	glUniform1f(loc, camera.pitch);

	loc = glGetUniformLocation(program, "yaw");
	glUniform1f(loc, camera.yaw);

	loc = glGetUniformLocation(program, "x_rot");
	glUniform1f(loc, rotation.x);

	loc = glGetUniformLocation(program, "y_rot");
	glUniform1f(loc, rotation.y);

	loc = glGetUniformLocation(program, "z_rot");
	glUniform1f(loc, rotation.z);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glDrawElements(GL_TRIANGLES, numVertexes, GL_UNSIGNED_INT, 0);
	auto error = glGetError();
	if (error != GL_NO_ERROR) {
		std::cerr << "GL Error: " << error << std::endl;
		throw error;
	}
}

static vec3 main_rotation = {0.f, 0.f, 0.f};

void draw() {
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDrawBuffer(GL_BACK);
	drawCuboid("ship", {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, main_rotation);
	glFlush();
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
	camera.position.x = 0;
	camera.position.y = 0;
	camera.position.z = 2;
	camera.pitch = to_radians(0);
	camera.yaw = to_radians(0);
	shaderMap["cube.vert"] = compileShaderProgram("cube.vert", GL_VERTEX_SHADER);
	shaderMap["cube.frag"] = compileShaderProgram("cube.frag", GL_FRAGMENT_SHADER);
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
				glViewport(0, 0, gwa.width, gwa.height);
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
		draw(); 
		glXSwapBuffers(dpy, win);
	}
}
