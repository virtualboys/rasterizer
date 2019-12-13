
#include "cl_renderer.hpp"
#include <OpenCL/cl.hpp>
#include <vector>
#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "model.h"

#pragma OPENCL EXTENSION cl_amd_printf : enable


//static const char source[] =
//    "kernel void add(\n"
//    "       ulong n,\n"
//    "       global const float *a,\n"
//    "       global const float *b,\n"
//    "       global float *c\n"
//    "       )\n"
//    "{\n"
//    "    size_t i = get_global_id(0);\n"
//    "    if (i < n) {\n"
//    "       c[i] = a[i] + b[i];\n"
//    "    }\n"
//    "}\n";

cl_renderer::cl_renderer() {
    
}

cl_renderer::~cl_renderer() {
    
}

bool cl_renderer::init(size_t size) {
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
        
        workSize = size;
        
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

    // Create command queue.
    queue = cl::CommandQueue(context, devices[0]);

    // Compile OpenCL program for found device.
    program = cl::Program(context, cl::Program::Sources(
            1, std::make_pair(text.c_str(), text.length())
            ));
    

    try {
        program.build(devices);
        kernel = cl::Kernel(program, "rasterizer");
    } catch (const cl::Error&) {
        std::cerr
        << "OpenCL compilation error" << std::endl
        << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[0])
        << std::endl;
    }
    std::cout << "Program at " << path << " loaded." << std::endl;
}

void cl_renderer::loadData(std::vector<int> inds, std::vector<float> verts, int nFaces, glm::mat4 viewport) {
    try {
        buffer_viewport = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            16 * sizeof(float), glm::value_ptr(viewport));
        
        buffer_verts = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            verts.size() * sizeof(float), verts.data());
        
        buffer_inds = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            inds.size() * sizeof(int), inds.data());
        
       
        workSize = nFaces;
    } catch(const cl::Error &err) {
        std::cerr
        << "OpenCL error: "
        << err.what() << "(" << err.err() << ")"
        << std::endl;
    }
}


void cl_renderer::runProgram(std::vector<int> inds, std::vector<float> verts, int nFaces, glm::mat4 viewport, unsigned char* screen, float* zBuffer, int width, int height) {
    
    try{
//        cl::Buffer buffer_viewport(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
//            16 * sizeof(float), glm::value_ptr(viewport));
//
//        cl::Buffer buffer_verts(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
//            verts.size() * sizeof(float), verts.data());
//
//        cl::Buffer buffer_inds(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
//            inds.size() * sizeof(int), inds.data());
        
        
        

        cl::Buffer buffer_z(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            width * height * sizeof(float), zBuffer);
        
        cl::Buffer buffer_screen(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            width * height * 3 * sizeof(unsigned char), screen);
        
        kernel.setArg(0, static_cast<cl_ulong>(nFaces));
        kernel.setArg(1, static_cast<cl_int>(width));
        kernel.setArg(2, static_cast<cl_int>(height));
        kernel.setArg(3, buffer_z);
        kernel.setArg(0, static_cast<cl_ulong>(nFaces));
        kernel.setArg(4, buffer_viewport);
        kernel.setArg(5, buffer_verts);
        kernel.setArg(6, buffer_inds);
//        kernel.setArg(4, buffer_viewport);
//        kernel.setArg(5, buffer_verts);
//        kernel.setArg(6, buffer_inds);
        kernel.setArg(7, buffer_screen);
        
        // Launch kernel on the compute device.
        queue.enqueueNDRangeKernel(kernel, cl::NullRange, nFaces, cl::NullRange);
        
        // Get result back to host.
        queue.enqueueReadBuffer(buffer_screen, CL_TRUE, 0, width * height * 3 * sizeof(unsigned char), screen);
    } catch(const cl::Error &err) {
        std::cerr
        << "OpenCL error: "
        << err.what() << "(" << err.err() << ")"
        << std::endl;
    }
}
