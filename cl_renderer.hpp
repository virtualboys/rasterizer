#ifndef cl_renderer_hpp
#define cl_renderer_hpp

#define __CL_ENABLE_EXCEPTIONS

#include <OpenCL/cl.hpp>
#include <glm/glm.hpp>
#include "model.h"

class cl_renderer {
public:
    cl_renderer();
    ~cl_renderer();
    bool init(size_t size);
    void loadProgram(std::string path);
    void loadData(std::vector<int> inds, std::vector<float> verts, int nFaces, glm::mat4 viewport);
    void runProgram(std::vector<int> inds, std::vector<float> verts, int nFaces, glm::mat4 viewport, unsigned char* screen, float* zBuffer, int width, int height);
    
private:
    size_t workSize;
    std::vector<cl::Device> devices;
    cl::Context context;
    cl::CommandQueue queue;
    cl::Program program;
    cl::Kernel kernel;
    cl::Buffer buffer_viewport;
    cl::Buffer buffer_verts;
    cl::Buffer buffer_inds;
};

#endif /* cl_renderer_hpp */
