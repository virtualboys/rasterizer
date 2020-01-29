
#ifndef gl_window_hpp
#define gl_window_hpp

#include <glm/glm.hpp>

class gl_window {
public:
    gl_window(bool printFPS);
    ~gl_window();
    
    int openWindow(int width, int height);
    void closeWindow();
    void initBuffers(int texWidth, int texHeight, unsigned char* data);
    
    int getWindowWidth();
    int getWindowHeight();
    
    bool shouldClose();
    void drawTex(unsigned char* data);
    
private:
    int m_width;
    int m_height;
    int m_texWidth;
    int m_texHeight;
    
    bool m_printFPS;
    double m_previousTime;
    int m_frameCount;

    
    GLFWwindow* m_window;
    
    GLuint m_vertexArrayId;
    GLuint m_shaderProgramId;
    GLuint m_textureId;
    GLuint m_vertexBuffer;
    GLuint m_uvBuffer;
    
    GLuint m_mvpMatUniLoc;
    GLuint m_texUniLoc;
    
    glm::mat4 m_mvpMat;
};

#endif /* gl_window_hpp */
