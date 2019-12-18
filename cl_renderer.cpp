#include <GL/glew.h>
#include "cl_renderer.hpp"
#include <OpenCL/cl.hpp>
#include <vector>
#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "model.h"

#pragma OPENCL EXTENSION cl_amd_printf : enable

cl_renderer::cl_renderer() {
    
}

cl_renderer::~cl_renderer() {
    
}

bool cl_renderer::init(size_t tileSize) {
    try {
        // Get list of OpenCL platforms.
        std::vector<cl::Platform> platform;
        cl::Platform::get(&platform);

        if (platform.empty()) {
            std::cerr << "OpenCL platforms not found." << std::endl;
            return 1;
        }

        // Get first available GPU device which supports double precision.
        
        for(auto p = platform.begin(); devices.empty() && p != platform.end(); p++) {
            std::vector<cl::Device> pldev;

            try {
                p->getDevices(CL_DEVICE_TYPE_GPU, &pldev);

                for(auto d = pldev.begin(); devices.empty() && d != pldev.end(); d++) {
                    if (!d->getInfo<CL_DEVICE_AVAILABLE>()) continue;

                    std::string ext = d->getInfo<CL_DEVICE_EXTENSIONS>();

                    //if (
                      //  ext.find("cl_khr_fp64") == std::string::npos &&
                        //ext.find("cl_amd_fp64") == std::string::npos
                       //) continue;

                    devices.push_back(*d);
                    context = cl::Context(devices);
                }
            } catch(...) {
                devices.clear();
                return false;
            }
        }
        
        if (devices.empty()) {
            std::cerr << "GPUs not found." << std::endl;
            return false;
        }

        std::cout << devices[0].getInfo<CL_DEVICE_NAME>() << std::endl;
        std::cout <<devices[0].getInfo<CL_DEVICE_EXTENSIONS>()<<std::endl;
//        if(devices[0].getInfo<CL_DEVICE_EXTENSIONS>().find("cl_khr_gl_sharing") == std::string::npos) {
//            std::cerr<<"Device interop not supported"<<std::endl;
//        }
        
        this->tileSize = tileSize;
        
        return true;
        
    } catch(const cl::Error &err) {
        std::cerr
        << "OpenCL error: "
        << err.what() << "(" << err.err() << ")"
        << std::endl;
        return false;
    }
}

void cl_renderer::loadProgram(std::string path) {
    //std::string source;
    std::string line, text;
    std::ifstream in(path);
    if (in.is_open()) {
        while(std::getline(in, line)) {
            text += line + "\n";
        }
        in.close();
    }
    else {
        std::cerr << "Unable to open " << path << std::endl;
        return;
    }

    queue = cl::CommandQueue(context, devices[0]);

    program = cl::Program(context, cl::Program::Sources(
            1, std::make_pair(text.c_str(), text.length())
            ));
    
    try {
        program.build(devices);
        rasterizer = cl::Kernel(program, "rasterizer");
        fragmentShader = cl::Kernel(program, "fragment");
        vertexShader = cl::Kernel(program, "vertex");

    } catch (const cl::Error&) {
        std::cerr
        << "OpenCL compilation error" << std::endl
        << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[0])
        << std::endl;
    }
    std::cout << "Program at " << path << " loaded." << std::endl;
}

void cl_renderer::setMVP(glm::mat4 model, glm::mat4 view, glm::mat4 proj) {
    auto viewDirHomo = view * glm::vec4(0,0,1,1);
    viewDir = glm::vec3(viewDirHomo);
    glm::mat4 mvp = proj * view * model;
    queue.enqueueWriteBuffer(buffer_mvp, true, 0, 16 * sizeof(float), glm::value_ptr(mvp));
    queue.enqueueWriteBuffer(buffer_modelMat, true, 0, 16 * sizeof(float), glm::value_ptr(model));
}

void cl_renderer::loadData(std::vector<int> inds, std::vector<float> verts, std::vector<float> normals, std::vector<float> uvs, TGAImage& tex, int nFaces, glm::mat4 viewport, unsigned char* screen, GLuint screenTex, float* zBuffer, int width, int height) {
    try {
        numFaces = nFaces;
        numVerts = verts.size() / 3;
        numPixels = width * height;
        
        this->width = width;
        this->height = height;
        this->screen = screen;
        this->screenTex = screenTex;
        
        clearColorData = new unsigned char[numPixels * 3];
        std::fill_n(clearColorData, numPixels * 3, 0);
        clearDepthData = new float[numPixels];
        std::fill_n(clearDepthData, numPixels, std::numeric_limits<float>::min());

        buffer_mvp = cl::Buffer(context, CL_MEM_READ_ONLY, 16 * sizeof(float));
        
        buffer_modelMat = cl::Buffer(context, CL_MEM_READ_ONLY, 16 * sizeof(float));
        
        buffer_viewport = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            16 * sizeof(float), glm::value_ptr(viewport));
        
        buffer_verts = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            verts.size() * sizeof(float), verts.data());
        
        buffer_vertOut = cl::Buffer(context, CL_MEM_READ_WRITE, 4 * numVerts * sizeof(float));
        
        buffer_normals = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, normals.size() * sizeof(float), normals.data());
        
        buffer_normalsVertOut = cl::Buffer(context, CL_MEM_READ_WRITE, 3 * numVerts * sizeof(float));
        
        buffer_normalsRastOut = cl::Buffer(context, CL_MEM_READ_WRITE, width * height * 3 * sizeof(float));
        
        buffer_uvs = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, uvs.size() * sizeof(float), uvs.data());
        
        buffer_uvsRastOut = cl::Buffer(context, CL_MEM_READ_WRITE, width * height * 2 * sizeof(float));
        
        buffer_inds = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            inds.size() * sizeof(int), inds.data());
        
        buffer_z = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            width * height * sizeof(float), zBuffer);
        
        buffer_screen = cl::Buffer(context, CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
            width * height * 3 * sizeof(unsigned char), screen);
        
        buffer_tex = cl::Image2D(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cl::ImageFormat(CL_RGB, CL_SNORM_INT8), tex.get_width(), tex.get_height(), 0, (void*)tex.buffer());
        
//        texMemory = cl::ImageGL(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, screenTex,NULL);
//        
//        texMemory = clCreateFromGLTexture(context, CL_MEM_WRITE_ONLY, (cl_GLenum)GL_TEXTURE_2D, (cl_GLint)0,(cl_GLint)screenTex,NULL);

       
    } catch(const cl::Error &err) {
        std::cerr
        << "OpenCL error: "
        << err.what() << "(" << err.err() << ")"
        << std::endl;
    }
}

void cl_renderer::clear() {
    queue.enqueueWriteBuffer(buffer_screen, true, 0, numPixels * 3 * sizeof(unsigned char), clearColorData);
    queue.enqueueWriteBuffer(buffer_z, true, 0, numPixels * sizeof(float), clearDepthData);
}

void cl_renderer::runProgram() {
    
    try {
        int i = 0;
        vertexShader.setArg(i++, static_cast<cl_ulong>(numVerts));
        vertexShader.setArg(i++, offsetVert);
        vertexShader.setArg(i++, buffer_mvp);
        vertexShader.setArg(i++, buffer_modelMat);
        vertexShader.setArg(i++, buffer_verts);
        vertexShader.setArg(i++, buffer_normals);
        vertexShader.setArg(i++, buffer_vertOut);
        vertexShader.setArg(i++, buffer_normalsVertOut);
        
        queue.enqueueNDRangeKernel(vertexShader, cl::NullRange, numVerts, cl::NullRange);
        
        i=0;
        rasterizer.setArg(i++, static_cast<cl_ulong>(numFaces));
        rasterizer.setArg(i++, static_cast<cl_int>(width));
        rasterizer.setArg(i++, static_cast<cl_int>(height));
        rasterizer.setArg(i++, offsetRast);
        rasterizer.setArg(i++, buffer_z);
        rasterizer.setArg(i++, buffer_viewport);
        rasterizer.setArg(i++, buffer_vertOut);
        rasterizer.setArg(i++, buffer_normalsVertOut);
        rasterizer.setArg(i++, buffer_uvs);
        rasterizer.setArg(i++, buffer_inds);
        rasterizer.setArg(i++, buffer_screen);
        rasterizer.setArg(i++, buffer_normalsRastOut);
        rasterizer.setArg(i++, buffer_uvsRastOut);
        
        queue.enqueueNDRangeKernel(rasterizer, cl::NullRange, numFaces, cl::NullRange);
        
        i=0;
        fragmentShader.setArg(i++, static_cast<cl_int>(width));
        fragmentShader.setArg(i++, static_cast<cl_int>(height));
        fragmentShader.setArg(i++, offsetFrag);
        fragmentShader.setArg(i++, glm::value_ptr(viewDir));
        fragmentShader.setArg(i++, buffer_z);
        fragmentShader.setArg(i++, buffer_normalsRastOut);
        fragmentShader.setArg(i++, buffer_uvsRastOut);
        fragmentShader.setArg(i++, buffer_tex);
        fragmentShader.setArg(i++, buffer_screen);
        
        queue.enqueueNDRangeKernel(fragmentShader, cl::NullRange, numPixels, cl::NullRange);
        
        queue.enqueueReadBuffer(buffer_screen, CL_TRUE, 0, width * height * 3 * sizeof(unsigned char), screen);
    } catch(const cl::Error &err) {
        std::cerr
        << "OpenCL error: "
        << err.what() << "(" << err.err() << ")"
        << std::endl;
    }
}
