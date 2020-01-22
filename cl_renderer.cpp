#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "cl_renderer.hpp"
#include <OpenCL/cl.hpp>
#include <vector>
#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "model.h"

#define __CL_ENABLE_EXCEPTIONS
#pragma OPENCL EXTENSION cl_amd_printf : enable

const int NUM_POLYS_PER_TILE = 10;
const int TILE_SIZE = 8;

cl_renderer::cl_renderer() {
//    modelMat = new glm::mat4();
//    mvpMat = new glm::mat4();
}

cl_renderer::~cl_renderer() {
    delete modelMat;
    delete mvpMat;
}

bool cl_renderer::init() {
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

        std::cout << "device name: " << devices[0].getInfo<CL_DEVICE_NAME>() << std::endl;
        std::cout << "device extensions: " << devices[0].getInfo<CL_DEVICE_EXTENSIONS>()<<std::endl;
        std::cout << "device global mem size: " << devices[0].getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>()<<std::endl;
        std::cout << "device max mem size: " << devices[0].getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>()<<std::endl;
        std::cout << "device max const buffer size: " << devices[0].getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>()<<std::endl;
//        if(devices[0].getInfo<CL_DEVICE_EXTENSIONS>().find("cl_khr_gl_sharing") == std::string::npos) {
//            std::cerr<<"Device interop not supported"<<std::endl;
//        }
        
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

    queue = cl::CommandQueue(context, devices[0], CL_QUEUE_PROFILING_ENABLE);

    program = cl::Program(context, cl::Program::Sources(
            1, std::make_pair(text.c_str(), text.length())
            ));
    
    try {
        program.build(devices);
        rasterizer = cl::Kernel(program, "tileRasterizer");
        tiler = cl::Kernel(program, "tiler");
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

void cl_renderer::loadData(std::vector<int> inds, std::vector<float> verts, std::vector<float> normals, std::vector<float> uvs, TGAImage& tex, int nFaces, glm::mat4 viewport, unsigned char* screen, GLuint screenTex, float* zBuffer, int width, int height) {
    try {
        if(width % TILE_SIZE != 0 || height % TILE_SIZE != 0) {
            std::cerr << "Screen dimensions must be multiple of tile size: " << TILE_SIZE << std::endl;
        }
        
        numFaces = nFaces;
        numVerts = verts.size() / 3;
        numPixels = width * height;
        
        std::cout<<"num faces: " <<numFaces << std::endl;
        
        this->width = width;
        this->height = height;
        this->screen = screen;
        this->screenTex = screenTex;
        
        clearColorData = new unsigned char[numPixels * 3];
        std::fill_n(clearColorData, numPixels * 3, 0);
        clearDepthData = new float[numPixels];
        std::fill_n(clearDepthData, numPixels, std::numeric_limits<float>::min());
        
        tilesX = width / TILE_SIZE;
        tilesY = height / TILE_SIZE;
        numTiles = tilesX * tilesY;
        
        buffer_tilePolys = cl::Buffer(context, CL_MEM_READ_WRITE, numTiles *
                                      (NUM_POLYS_PER_TILE + 1) * sizeof(int));
        
        std::cout<< "tilepolys size: " <<numTiles *
        (NUM_POLYS_PER_TILE + 1) * sizeof(int)<< std::endl;

        buffer_mvp = cl::Buffer(context, CL_MEM_READ_WRITE, 16 * sizeof(float));
        cl::Buffer(context, CL_MEM_READ_WRITE, 4026531843);
        
        
        buffer_modelMat = cl::Buffer(context, CL_MEM_READ_WRITE, 16 * sizeof(float));
        
        buffer_viewport = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            16 * sizeof(float), glm::value_ptr(viewport));
        
        buffer_verts = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            verts.size() * sizeof(float), verts.data());
        
        std::cout<< "vert size: " <<verts.size() * sizeof(float) << std::endl;
        
        buffer_vertOut = cl::Buffer(context, CL_MEM_READ_WRITE, 4 * numVerts * sizeof(float));
        
        buffer_normals = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, normals.size() * sizeof(float), normals.data());
        
        buffer_normalsVertOut = cl::Buffer(context, CL_MEM_READ_WRITE, 3 * numVerts * sizeof(float));
        
        buffer_normalsRastOut = cl::Buffer(context, CL_MEM_READ_WRITE, width * height * 3 * sizeof(float));
        
        buffer_uvs = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, uvs.size() * sizeof(float), uvs.data());
        
        buffer_uvsRastOut = cl::Buffer(context, CL_MEM_READ_WRITE, width * height * 2 * sizeof(float));
        
        buffer_inds = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            inds.size() * sizeof(int), inds.data());
        
        buffer_z = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            width * height * sizeof(float), zBuffer);
        
        std::cout<< "z size: " <<width * height * sizeof(float)<< std::endl;
        
        buffer_screen = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            width * height * 3 * sizeof(unsigned char), screen);
        
        
        std::cout<< "screen size: " <<width * height * 3 * sizeof(unsigned char)<< std::endl;
        
        buffer_tex = cl::Image2D(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, cl::ImageFormat(CL_RGB, CL_SNORM_INT8), tex.get_width(), tex.get_height(), 0, (void*)tex.buffer());
        
//        texMemory = cl::ImageGL(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, screenTex,NULL);
//        
//        texMemory = clCreateFromGLTexture(context, CL_MEM_WRITE_ONLY, (cl_GLenum)GL_TEXTURE_2D, (cl_GLint)0,(cl_GLint)screenTex,NULL);

       
    } catch(const cl::Error &err) {
        std::cerr
        << "OpenCL error: "
        << err.what() << "(" << err.err() << ")"
        << std::endl;
    }
    std::cout << "Loaded data..." << std::endl;
}

void cl_renderer::setMVP(glm::mat4 model, glm::mat4 view, glm::mat4 proj) {
    try{
        auto viewDirHomo = view * glm::vec4(0,0,1,1);
        viewDir = glm::vec3(viewDirHomo);
        auto mvp = proj * view * model;
        mvpMat = (const float*)glm::value_ptr(mvp);
        modelMat = (const float*)glm::value_ptr(model);
        queue.finish();
        queue.enqueueWriteBuffer(buffer_mvp, true, 0, 16 * sizeof(float), mvpMat);
        queue.enqueueWriteBuffer(buffer_modelMat, true, 0, 16 * sizeof(float), modelMat);
        
        queue.finish();
//        std::cout<<mvp[0][0]<<std::endl;
    } catch(const cl::Error &err) {
        std::cerr
        << "OpenCL error: "
        << err.what() << "(" << err.err() << ")"
        << std::endl;
    } catch(const std::exception& ex) {
        std::cerr
        << "Other error: "
        << ex.what() << "(" <<  ")"
        << std::endl;
    }
}

void cl_renderer::clear(unsigned char val) {
    try {
        queue.finish();
    //    queue.enqueueWriteBuffer(buffer_screen, true, 0, numPixels * 3 * sizeof(unsigned char), clearColorData);
        queue.enqueueFillBuffer<unsigned char>(buffer_screen, val, 0, numPixels * 3 * sizeof(unsigned char), NULL, 0);
    //    queue.enqueueWriteBuffer(buffer_z, true, 0, numPixels * sizeof(float), clearDepthData);
        queue.enqueueFillBuffer<float>(buffer_z, 0, 0, numPixels * sizeof(float), NULL, 0);
        queue.finish();
    } catch(const cl::Error &err) {
        std::cerr
        << "OpenCL error: "
        << err.what() << "(" << err.err() << ")"
        << std::endl;
    }
}

void cl_renderer::runProgram() {
    
    try {
        
        int i = 0;
        vertexShader.setArg(i++, static_cast<cl_ulong>(numVerts));
//        vertexShader.setArg(i++, offsetVert);
        vertexShader.setArg(i++, 0);
        vertexShader.setArg(i++, buffer_mvp);
        vertexShader.setArg(i++, buffer_modelMat);
        vertexShader.setArg(i++, buffer_verts);
        vertexShader.setArg(i++, buffer_normals);
        vertexShader.setArg(i++, buffer_vertOut);
        vertexShader.setArg(i++, buffer_normalsVertOut);

        queue.enqueueNDRangeKernel(vertexShader, cl::NullRange, numVerts, cl::NullRange, NULL, &vertEvent);
        
        queue.enqueueFillBuffer<int>(buffer_tilePolys, 0, 0, numTiles *
                                     (NUM_POLYS_PER_TILE + 1) * sizeof(int), NULL, 0);
        
        queue.finish();
        std::cout<<"done vert"<<std::endl;
        
        i=0;
        tiler.setArg(i++, static_cast<cl_ulong>(numFaces));
        tiler.setArg(i++, static_cast<cl_int>(width));
        tiler.setArg(i++, static_cast<cl_int>(height));
        tiler.setArg(i++, static_cast<cl_int>(tilesX));
        tiler.setArg(i++, static_cast<cl_int>(tilesY));
        tiler.setArg(i++, static_cast<cl_int>(TILE_SIZE));
        tiler.setArg(i++, static_cast<cl_int>(NUM_POLYS_PER_TILE));
        tiler.setArg(i++, buffer_viewport);
        tiler.setArg(i++, buffer_vertOut);
        tiler.setArg(i++, buffer_inds);
        tiler.setArg(i++, buffer_tilePolys);

        queue.enqueueNDRangeKernel(tiler, cl::NullRange, numFaces, cl::NullRange, NULL, &tileEvent);
        queue.finish();
        std::cout<<"done tiler"<<std::endl;
        
        
        i=0;
        rasterizer.setArg(i++, static_cast<cl_ulong>(numFaces));
        rasterizer.setArg(i++, static_cast<cl_int>(width));
        rasterizer.setArg(i++, static_cast<cl_int>(height));
        rasterizer.setArg(i++, static_cast<cl_int>(tilesX));
        rasterizer.setArg(i++, static_cast<cl_int>(tilesY));
        rasterizer.setArg(i++, static_cast<cl_int>(TILE_SIZE));
        rasterizer.setArg(i++, static_cast<cl_int>(NUM_POLYS_PER_TILE));
//        rasterizer.setArg(i++, offsetRast);
        rasterizer.setArg(i++, 0);
        rasterizer.setArg(i++, buffer_z);
        rasterizer.setArg(i++, buffer_viewport);
        rasterizer.setArg(i++, buffer_vertOut);
        rasterizer.setArg(i++, buffer_normalsVertOut);
        rasterizer.setArg(i++, buffer_uvs);
        rasterizer.setArg(i++, buffer_inds);
        rasterizer.setArg(i++, buffer_tilePolys);
        rasterizer.setArg(i++, buffer_screen);
        rasterizer.setArg(i++, buffer_normalsRastOut);
        rasterizer.setArg(i++, buffer_uvsRastOut);
        
        queue.enqueueNDRangeKernel(rasterizer, cl::NullRange, numTiles, cl::NullRange, NULL, &rastEvent);
        queue.finish();
        std::cout<<"done rast"<<std::endl;
        
        i=0;
        fragmentShader.setArg(i++, static_cast<cl_int>(width));
        fragmentShader.setArg(i++, static_cast<cl_int>(height));
//        fragmentShader.setArg(i++, offsetFrag);
        fragmentShader.setArg(i++, 0);
        fragmentShader.setArg(i++, glm::value_ptr(viewDir));
        fragmentShader.setArg(i++, buffer_z);
        fragmentShader.setArg(i++, buffer_normalsRastOut);
        fragmentShader.setArg(i++, buffer_uvsRastOut);
        fragmentShader.setArg(i++, buffer_tex);
        fragmentShader.setArg(i++, buffer_screen);
        
//        queue.enqueueNDRangeKernel(fragmentShader, cl::NullRange, numPixels, cl::NullRange, NULL, &fragEvent);
//
//        queue.enqueueReadBuffer(buffer_screen, CL_TRUE, 0, width * height * 3 * sizeof(unsigned char), screen);
        
        queue.finish();
        
        
    } catch(const cl::Error &err) {
        std::cerr
        << "OpenCL error: "
        << err.what() << "(" << err.err() << ")"
        << std::endl;
    }
}

double cl_renderer::getEventTime(cl::Event event) {
    return 0;
    cl_ulong time_start;
    cl_ulong time_end;

    event.getProfilingInfo(CL_PROFILING_COMMAND_START, &time_start);
    event.getProfilingInfo(CL_PROFILING_COMMAND_END, &time_end);

    return (time_end-time_start) * 0.000001;
}

void cl_renderer::printCLRuntimes() {
    return;
    std::cout<< "times: " << getEventTime(vertEvent) << ", " << getEventTime(tileEvent) << ", " << getEventTime(rastEvent) << ", " << getEventTime(fragEvent) << std::endl;
}
