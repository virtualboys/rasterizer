// Include standard headers
#include <stdio.h>
#include <stdlib.h>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


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
#include "gl_window.hpp"

#include "cl_renderer.hpp"

//volatile int STOP=false;
unsigned char buf[255];
int res;
int myCount=0;
int maxCount=10000;            // Number of cycles to time out serial port

Model *model        = NULL;

const int width  = 800;
const int height = 800;

Vec3f light_dir(1,1,1);
Vec3f       eye(1,1,3);
Vec3f    center(0,0,0);
Vec3f        up(0,1,0);

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
//    return 0;
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
	

    // Read the information about the image
    unsigned int imageSize=width*height*3;
    // Create a buffer
    unsigned char * data = new unsigned char [imageSize];

    float *zbuffer = new float[width*height];

    lookat(eye, center, up);
    viewport(width/8, height/8, width*3/4, height*3/4);
    projection(-1.f/(eye-center).norm());
    light_dir = proj<3>((Projection*ModelView*embed<4>(light_dir, 0.f))).normalize();

    model = new Model(argv[1]);
    
    int fd = init_serial();
    if(fd != 0) {
        init_port(&fd,9600);                  //set serial port to 9600,8,n,1
    }
    
    cl_renderer clRend;
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
    viewMat = glm::translate(viewMat, glm::vec3(0,0,-3));
    glm::mat4 projMat = glm::perspective(45.0f, (GLfloat)width / (GLfloat)height, .001f, 100000.0f);
    
    modelMat = glm::translate(modelMat, glm::vec3(0,0,1));
    
    for(int i = 0; i < width*height; i++) {
        zbuffer[i] = std::numeric_limits<float>::min();
    }
    
    clRend.loadData(model->faces_2, model->verts_2, model->normals_2, model->uvs_2, model->diffusemap_, model->nfaces(), viewportMat, data, zbuffer, width, height);
    
	    
    offsetAnimator vertAnim(.02f, -.09, .09 );
    offsetAnimator rastAnim(.001f, -.1f, .1f);
    offsetAnimator fragAnim(.003f, -8, 8);
    offsetAnimator projAnim(.0002f, 0, 1);
    offsetAnimator clearAnim(.002f, 0, 1);
    
    float rotSpeed = .01f;
    
    int numVals = 3;
    float* arduinoVals = new float[numVals];
    
    gl_window window(true);
    window.openWindow(width, height);
    window.initBuffers(width, height, data);

	do{
        if(fd != 0) {
            if(readArduinoVal(fd, arduinoVals, numVals) != -1) {
                vertAnim.setD(arduinoVals[1]);
                rastAnim.setD(arduinoVals[2]);
                fragAnim.setD(arduinoVals[2]);
                projAnim.setD(arduinoVals[0]);
                clearAnim.setD(arduinoVals[1]);
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

        modelMat = glm::rotate(modelMat,  rotSpeed * 2.0f * glm::pi<float>(), glm::vec3(0,1,0));

       
        
        projMat = glm::perspective(projAnim.getVal() * 45, (GLfloat)width / (GLfloat)height, .001f, 100000.0f);
        
//        std::cout << "valvert" << vertAnim.getVal() << std::endl;
//        std::cout << "valrast" << rastAnim.getVal() << std::endl;
//        std::cout << "valfrag" << fragAnim.getVal() << std::endl;
//        std::cout<< "offset: " << offset <<std::endl;
        
        clRend.clear(255 * clearAnim.getVal());
        clRend.setMVP(modelMat, viewMat, projMat);
        clRend.runProgram();
        
        window.drawTex(data);
	} while(!window.shouldClose());

    window.closeWindow();

	delete [] data;
    delete [] zbuffer;
    delete model;
    
    if(fd != 0) {
        close(fd);
    }

	return 0;
}

