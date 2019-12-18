#ifndef cl_renderer_hpp
#define cl_renderer_hpp

#define __CL_ENABLE_EXCEPTIONS

#include <GL/glew.h>
#include <OpenCL/cl.hpp>
#include <glm/glm.hpp>
#include "model.h"
#include "tgaimage.h"

class cl_renderer {
public:
    cl_renderer();
    ~cl_renderer();
    
    float offsetVert;
    float offsetRast;
    float offsetFrag;
    
    bool init(size_t tileSize);
    void loadProgram(std::string path);
    void loadData(std::vector<int> inds, std::vector<float> verts, std::vector<float> normals, std::vector<float> uvs, TGAImage& tex, int nFaces, glm::mat4 viewport, unsigned char* screen, GLuint screenTex, float* zBuffer, int width, int height);
    void setMVP(glm::mat4 model, glm::mat4 view, glm::mat4 proj);
    void clear();
    void runProgram();
    
private:
    size_t tileSize;
    
    size_t numFaces;
    size_t numVerts;
    size_t numPixels;
    
    unsigned char* clearColorData;
    float* clearDepthData;
    
    std::vector<cl::Device> devices;
    cl::Context context;
    cl::CommandQueue queue;
    cl::Program program;
    
    cl::Kernel vertexShader;
    cl::Kernel rasterizer;
    cl::Kernel fragmentShader;
    
    cl::Buffer buffer_mvp;
    cl::Buffer buffer_modelMat;
    cl::Buffer buffer_viewport;
    cl::Buffer buffer_verts;
    cl::Buffer buffer_vertOut;
    cl::Buffer buffer_normals;
    cl::Buffer buffer_normalsVertOut;
    cl::Buffer buffer_normalsRastOut;
    cl::Buffer buffer_uvs;
    cl::Buffer buffer_uvsRastOut;
    cl::Buffer buffer_inds;
    cl::Image2D buffer_tex;
    cl::Buffer buffer_z;
    cl::Buffer buffer_screen;
    GLuint screenTex;
    cl::Memory texMemory;
    unsigned char* screen;
    int width;
    int height;
    glm::vec3 viewDir;
};

#endif /* cl_renderer_hpp */
