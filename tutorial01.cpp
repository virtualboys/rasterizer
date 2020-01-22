// Include standard headers
#include <stdio.h>
#include <stdlib.h>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
GLFWwindow* window;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <common/shader.hpp>
#include <common/texture.hpp>

#include <vector>
#include <limits>
#include <iostream>
#include <cstdlib>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <limits>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"
#include "offsetAnimator.hpp"

#include "cl_renderer.hpp"

volatile int STOP=false;
unsigned char buf[255];
int res;
int myCount=0;
int maxCount=10000;            // Number of cycles to time out serial port

using namespace glm;


Model *model        = NULL;

const int width  = 800;
const int height = 800;

Vec3f light_dir(1,1,1);
Vec3f       eye(1,1,3);
Vec3f    center(0,0,0);
Vec3f        up(0,1,0);

struct Shader : public IShader {
    mat<2,3,float> varying_uv;  // triangle uv coordinates, written by the vertex shader, read by the fragment shader
    mat<4,3,float> varying_tri; // triangle coordinates (clip coordinates), written by VS, read by FS
    mat<3,3,float> varying_nrm; // normal per vertex to be interpolated by FS
    mat<3,3,float> ndc_tri;     // triangle in normalized device coordinates

    virtual Vec4f vertex(int iface, int nthvert) {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        varying_nrm.set_col(nthvert, proj<3>((Projection*ModelView).invert_transpose()*embed<4>(model->normal(iface, nthvert), 0.f)));
        Vec4f gl_Vertex = Projection*ModelView*embed<4>(model->vert(iface, nthvert));

        varying_tri.set_col(nthvert, gl_Vertex);

        ndc_tri.set_col(nthvert, proj<3>(gl_Vertex/gl_Vertex[3]));

        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color) {
        Vec3f bn = (varying_nrm*bar).normalize();
        Vec2f uv = varying_uv*bar;

//        mat<3,3,float> A;
//        A[0] = ndc_tri.col(1) - ndc_tri.col(0);
//        A[1] = ndc_tri.col(2) - ndc_tri.col(0);
//        A[2] = bn;
//
//        mat<3,3,float> AI = A.invert();

//        Vec3f i = AI * Vec3f(varying_uv[0][1] - varying_uv[0][0], varying_uv[0][2] - varying_uv[0][0], 0);
//        Vec3f j = AI * Vec3f(varying_uv[1][1] - varying_uv[1][0], varying_uv[1][2] - varying_uv[1][0], 0);
//
//        mat<3,3,float> B;
//        B.set_col(0, i.normalize());
//        B.set_col(1, j.normalize());
//        B.set_col(2, bn);

        Vec3f n = bn;// (B*model->normal(uv)).normalize();

        float diff = std::max(0.f, n*light_dir);
        color = model->diffuse(uv)*diff;
        //std::cout<<"frag"<<std::endl;
        //color[0] = 255;
        return false;
    }
};

void init_port(int *fd, unsigned int baud)
{
   struct termios options;
   tcgetattr(*fd,&options);
   switch(baud)
   {
           case 9600: cfsetispeed(&options,B9600);
                 cfsetospeed(&options,B9600);
                 break;
           case 19200: cfsetispeed(&options,B19200);
                 cfsetospeed(&options,B19200);
                 break;
           case 38400: cfsetispeed(&options,B38400);
                 cfsetospeed(&options,B38400);
                 break;
           default:cfsetispeed(&options,B9600);
                 cfsetospeed(&options,B9600);
                 break;
   }
   options.c_cflag |= (CLOCAL | CREAD);
   options.c_cflag &= ~PARENB;
   options.c_cflag &= ~CSTOPB;
   options.c_cflag &= ~CSIZE;
   options.c_cflag |= CS8;
   tcsetattr(*fd,TCSANOW,&options);
}

int init_serial() {
    return 0;
    std::cout<<"OK HERE"<<std::endl;
    int fd;
//    fd = open("/dev/tty.usbmodem14201", O_RDWR | O_NOCTTY | O_NDELAY); // List usbSerial devices using Terminal ls /dev/tty.*
    fd = open("/dev/tty.usbmodem14101", O_RDWR | O_NOCTTY | O_NDELAY);
    if(fd == -1) {                        // Check for port errors
        fd = open("/dev/tty.usbmodem14201", O_RDWR | O_NOCTTY | O_NDELAY); // List usbSerial devices using
        if(fd == -1) {
           std::cout << fd;
           perror("Unable to open serial port\n");
           return (0);
        }
     }
     
     std::cout << "Serial Port is open\n";
    return fd;
}

int readArduinoVal(int fd, float* vals, int numVals) {
    char buffer[150];

    if(fd == -1) {
        std::cout<<"CLOSED!!"<<std::endl;
        return -1;
    }
    ssize_t length = read(fd, &buffer, sizeof(buffer));
    if (length == -1)
    {
        std::cerr << "Error reading from serial port" << std::endl;
        return -1;
    }
    else if (length == 0)
    {
        std::cerr << "No more data" << std::endl;
        return -1;
    }
    else
    {
        buffer[length] = '\0';
        
        std::string sBuf = std::string(buffer);
        int endInd = sBuf.find_last_of('X');
        if(endInd == -1) {
            return -1;
        }
        
        sBuf = sBuf.substr(0, endInd);
        int startInd = sBuf.find_last_of('X');
        if(startInd == -1) {
            return -1;
        }
        
        sBuf = sBuf.substr(startInd+1, endInd - startInd);
        sBuf += 'Y';
        
        for(int i = 0; i < numVals; i++) {
            int ind = sBuf.find_first_of('Y');
            if(ind == -1) {
                return -1;
            }
            std::string sVal = sBuf.substr(0,ind);
            int iVal = std::stoi(sVal);
            vals[i] = 1.0f - (iVal / 500.0f);
            
            std::cout<<"val" << i <<": " << vals[i] << ", ";
            sBuf = sBuf.substr(ind+1);
        }
        
        std::cout << std::endl;
        return 1;
    }
}

int main( int argc, char** argv )
{
    if (2>argc) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    

// Initialise GLFW
	if( !glfwInit() )
	{
		fprintf( stderr, "Failed to initialize GLFW\n" );
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow( width, height, "Tutorial 05 - Textured Cube", NULL, NULL);
	if( window == NULL ){
		fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

	// Dark blue background
	glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS); 

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// Create and compile our GLSL program from the shaders
	GLuint programID = LoadShaders( "TransformVertexShader.vertexshader", "TextureFragmentShader.fragmentshader" );

	// Get a handle for our "MVP" uniform
	GLuint MatrixID = glGetUniformLocation(programID, "MVP");

	glm::mat4 GLMProjection = glm::ortho(-1.0f,1.0f,-1.0f,1.0f,0.0f,100.0f);	// Camera matrix
	
	glm::mat4 GLMView       = glm::lookAt(
								glm::vec3(0,0,-1), // Camera is at (4,3,3), in World Space
								glm::vec3(0,0,0), // and looks at the origin
								glm::vec3(0,1,0)  // Head is up (set to 0,-1,0 to look upside-down)
						   );
	// Model matrix : an identity matrix (model will be at the origin)
	glm::mat4 GLMModel      = glm::mat4(1.0f);
	// Our ModelViewProjection : multiplication of our 3 matrices
	glm::mat4 MVP        = GLMProjection * GLMView * GLMModel; // Remember, matrix multiplication is the other way around

	// Our vertices. Tree consecutive floats give a 3D vertex; Three consecutive vertices give a triangle.
	// A cube has 6 faces with 2 triangles each, so this makes 6*2=12 triangles, and 12*3 vertices
	static const GLfloat g_vertex_buffer_data[] = { 
		-1.0f,-1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		 1.0f,-1.0f, 1.0f,
		 1.0f, 1.0f, 1.0f,
		 1.0f,-1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
	};

	// Two UV coordinatesfor each vertex. They were created with Blender.
	static const GLfloat g_uv_buffer_data[] = { 
		0, 0, 
		0, 1, 
		1, 0, 
		1, 1, 
		1, 0, 
		0, 1, 
	};

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

	GLuint uvbuffer;
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_uv_buffer_data), g_uv_buffer_data, GL_STATIC_DRAW);


    // Read the information about the image
	unsigned int imageSize=width*height*3; 
	// Create a buffer
	unsigned char * data = new unsigned char [imageSize];

    // Create one OpenGL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    // Return the ID of the texture we just created
    GLuint Texture = textureID;
    
    // Get a handle for our "myTextureSampler" uniform
    GLuint texUniformLoc  = glGetUniformLocation(programID, "myTextureSampler");

    
    // "Bind" the newly created texture : all future texture functions will modify this texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Give the image to OpenGL
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    float *zbuffer = new float[width*height];

    TGAImage frame(width, height, TGAImage::RGB);
    lookat(eye, center, up);
    viewport(width/8, height/8, width*3/4, height*3/4);
    projection(-1.f/(eye-center).norm());
    light_dir = proj<3>((Projection*ModelView*embed<4>(light_dir, 0.f))).normalize();

    model = new Model(argv[1]);
    Shader shader;
    
    int fd = init_serial();
    if(fd != 0) {
        init_port(&fd,9600);                  //set serial port to 9600,8,n,1
    }
    
    cl_renderer clRend;
    size_t N = width * height * 3;
    clRend.init();
    clRend.loadProgram("../tutorial01_first_window/testProg.cl");
    
    glm::mat4 viewportMat;
    viewportMat[3][0] = width/2.f;
    viewportMat[3][1] = height/2.f;
    viewportMat[3][2] = 1.f;
    viewportMat[0][0] = width/2.f;
    viewportMat[1][1] = height/2.f;
    viewportMat[2][2] = 0;
    
    glm::mat4 modelMat;
    glm::mat4 viewMat;
    viewMat = glm::translate(viewMat, vec3(0,0,-2));
    glm::mat4 projMat = glm::perspective(45.0f, (GLfloat)width / (GLfloat)height, .001f, 100000.0f);
    
    modelMat = glm::translate(modelMat, vec3(0,0,1));
    
    for(int i = 0; i < width*height; i++) {
        zbuffer[i] = std::numeric_limits<float>::min();
    }
    
    clRend.loadData(model->faces_2, model->verts_2, model->normals_2, model->uvs_2, model->diffusemap_, model->nfaces(), viewportMat, data, textureID, zbuffer, width, height);
    //clRend.offset = 0;
    
	bool increasing = false;
	double previousTime = glfwGetTime();
	int frameCount = 0;
    
    offsetAnimator vertAnim(.02f, -.04, .04);
    offsetAnimator rastAnim(.001f, -4, 4);
    offsetAnimator fragAnim(.003f, -8, 8);
    offsetAnimator projAnim(.0002f, 0, 1);
    offsetAnimator clearAnim(.002f, 0, 1);
    
    float rotSpeed = .01f;
    
    int numVals = 3;
    float* arduinoVals = new float[numVals];

	do{
        if(fd != 0) {
            if(readArduinoVal(fd, arduinoVals, numVals) != -1) {
                vertAnim.setD(arduinoVals[0]);
                rastAnim.setD(arduinoVals[1]);
                fragAnim.setD(arduinoVals[2]);
                projAnim.setD(arduinoVals[0]);
                clearAnim.setD(arduinoVals[0]);
            } else {
                std::cout<<"could not read val" <<std::endl;
            }

        } else {
            vertAnim.update();
            rastAnim.update();
            fragAnim.update();
            projAnim.update();
            clearAnim.update();
        }
        
        clRend.offsetVert = vertAnim.getVal();
        clRend.offsetRast = rastAnim.getVal();
        clRend.offsetFrag = fragAnim.getVal();
//        
////        offset = 3.0f * (val -.5f);
//        modelMat = glm::rotate(modelMat,  rotSpeed * 2.0f * glm::pi<float>(), glm::vec3(0,1,0));

       
        
        //projMat = glm::perspective(projAnim.getVal() * 45, (GLfloat)width / (GLfloat)height, .001f, 100000.0f);
        
//        std::cout << "valvert" << vertAnim.getVal() << std::endl;
//        std::cout << "valrast" << rastAnim.getVal() << std::endl;
//        std::cout << "valfrag" << fragAnim.getVal() << std::endl;
//        std::cout<< "offset: " << offset <<std::endl;
        
        clRend.clear(255 * clearAnim.getVal());
        clRend.setMVP(modelMat, viewMat, projMat);
        clRend.runProgram();
        
		frameCount++;
		double newTime = glfwGetTime();
		if(newTime-previousTime > 1.0) {
			std::cout<<"fps: "<<frameCount / (newTime-previousTime)<<std::endl;
            clRend.printCLRuntimes();
			frameCount = 0;
        	previousTime = newTime;
		}

		if(increasing) {
			up[0] += .01f;	
			if(up[0] > 1) {
				increasing = false;
			}
		} else {
			up[0] -= .01f;
			if(up[0] < -1) {
				increasing = true;
			}
		}
		
//		lookat(eye, center, up);
//	    for (int i=width*height; i--; zbuffer[i] = -std::numeric_limits<float>::max());
//		for (int i=0; i<model->nfaces(); i++) {
//            for (int j=0; j<3; j++) {
//                shader.vertex(i, j);
//            }
//            triangle(shader.varying_tri, shader, frame, zbuffer);
//        }
	    //frame.flip_vertically(); // to place the origin in the bottom left corner of the image
	    //frame.write_tga_file("framebuffer.tga");


		// for(int x = 0; x < width; x++) {
		// 	for(int y = 0; y < height; y++) {
		// 		int ind = 3 * (x + y * width);
		// 		data[ind] = frame.buffer()
		// 	}
		// }

		
		// "Bind" the newly created texture : all future texture functions will modify this texture
		glBindTexture(GL_TEXTURE_2D, textureID);
//		glTexSubImage2D(GL_TEXTURE_2D, 0,0,0,width, height,GL_BGR,GL_UNSIGNED_BYTE, frame.buffer());
        glTexSubImage2D(GL_TEXTURE_2D, 0,0,0,width, height,GL_BGR,GL_UNSIGNED_BYTE, data);




		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use our shader
		glUseProgram(programID);

		// Send our transformation to the currently bound shader, 
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture);
		// Set our "myTextureSampler" sampler to use Texture Unit 0
		glUniform1i(texUniformLoc, 0);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(
			0,                  // attribute. No particular reason for 0, but must match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : UVs
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glVertexAttribPointer(
			1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
			2,                                // size : U+V => 2
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// Draw the triangle !
		glDrawArrays(GL_TRIANGLES, 0, 12*3); // 12*3 indices starting at 0 -> 12 triangles

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
		   glfwWindowShouldClose(window) == 0 );

	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &uvbuffer);
	glDeleteProgram(programID);
	glDeleteTextures(1, &Texture);
	glDeleteVertexArrays(1, &VertexArrayID);

	delete [] data;
    delete [] zbuffer;
    delete model;
    //close(fd);


	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

