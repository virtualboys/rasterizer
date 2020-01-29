
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "gl_window.hpp"

#include <iostream>

#include <common/shader.hpp>
#include <common/texture.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

gl_window::gl_window(bool printFPS) {
    m_printFPS = printFPS;
    if(printFPS) {
        m_previousTime = glfwGetTime();
        m_frameCount = 0;
    }
}

gl_window::~gl_window() {
    // Cleanup VBO and shader
    glDeleteBuffers(1, &m_vertexBuffer);
    glDeleteBuffers(1, &m_uvBuffer);
    glDeleteProgram(m_shaderProgramId);
    glDeleteTextures(1, &m_textureId);
    glDeleteVertexArrays(1, &m_vertexArrayId);
}

int gl_window::openWindow(int width, int height) {
    m_width = width;
    m_height = height;
    
    if (!glfwInit()) {
       fprintf( stderr, "Failed to initialize GLFW\n" );
       return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Open a window and create its OpenGL context
    m_window = glfwCreateWindow( m_width, m_height, "!!!  rasterizer :0  !!!", NULL, NULL);
    if( m_window == NULL ) {
       fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
       glfwTerminate();
       return -1;
    }
    glfwMakeContextCurrent(m_window);

    // Initialize GLEW
    glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK) {
       fprintf(stderr, "Failed to initialize GLEW\n");
       return -1;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(m_window, GLFW_STICKY_KEYS, GL_TRUE);
    
    return 0;
}

void gl_window::closeWindow() {
    glfwTerminate();
}

void gl_window::initBuffers(int texWidth, int texHeight, unsigned char *data) {
    m_texWidth = texWidth;
    m_texHeight = texHeight;
    
    // Dark blue background
    glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);

    glGenVertexArrays(1, &m_vertexArrayId);
    glBindVertexArray(m_vertexArrayId);

    // Create and compile our GLSL program from the shaders
    m_shaderProgramId = LoadShaders( "TransformVertexShader.vertexshader", "TextureFragmentShader.fragmentshader" );

    // Get a handle for our "MVP" uniform
    m_mvpMatUniLoc = glGetUniformLocation(m_shaderProgramId, "MVP");

    glm::mat4 GLMProjection = glm::ortho(-1.0f,1.0f,-1.0f,1.0f,0.0f,100.0f);    // Camera matrix
    
    glm::mat4 GLMView       = glm::lookAt(
                                glm::vec3(0,0,-1), // Camera is at (4,3,3), in World Space
                                glm::vec3(0,0,0), // and looks at the origin
                                glm::vec3(0,1,0)  // Head is up (set to 0,-1,0 to look upside-down)
                           );
    // Model matrix : an identity matrix (model will be at the origin)
    glm::mat4 GLMModel      = glm::mat4(1.0f);
    // Our ModelViewProjection : multiplication of our 3 matrices
    m_mvpMat        = GLMProjection * GLMView * GLMModel; // Remember, matrix multiplication is the other way around

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

    m_vertexBuffer;
    glGenBuffers(1, &m_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

    m_uvBuffer;
    glGenBuffers(1, &m_uvBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_uvBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_uv_buffer_data), g_uv_buffer_data, GL_STATIC_DRAW);


    // Create one OpenGL texture
//    GLuint textureID;
    glGenTextures(1, &m_textureId);
    // Return the ID of the texture we just created
//    GLuint Texture = textureID;
    
    // Get a handle for our "myTextureSampler" uniform
    m_texUniLoc  = glGetUniformLocation(m_shaderProgramId, "myTextureSampler");

    
    // "Bind" the newly created texture : all future texture functions will modify this texture
    glBindTexture(GL_TEXTURE_2D, m_textureId);

    // Give the image to OpenGL
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, m_width, m_height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void gl_window::drawTex(unsigned char* data) {
    // "Bind" the newly created texture : all future texture functions will modify this texture
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    //        glTexSubImage2D(GL_TEXTURE_2D, 0,0,0,width, height,GL_BGR,GL_UNSIGNED_BYTE, frame.buffer());
    glTexSubImage2D(GL_TEXTURE_2D, 0,0,0,m_texWidth, m_texHeight,GL_BGR,GL_UNSIGNED_BYTE, data);

    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use our shader
    glUseProgram(m_shaderProgramId);

    // Send our transformation to the currently bound shader,
    // in the "MVP" uniform
    glUniformMatrix4fv(m_mvpMatUniLoc, 1, GL_FALSE, &m_mvpMat[0][0]);

    // Bind our texture in Texture Unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    // Set our "myTextureSampler" sampler to use Texture Unit 0
    glUniform1i(m_texUniLoc, 0);

    // 1rst attribute buffer : vertices
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
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
    glBindBuffer(GL_ARRAY_BUFFER, m_uvBuffer);
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
    glfwSwapBuffers(m_window);
    glfwPollEvents();
    
    if(m_printFPS) {
        m_frameCount++;
        double newTime = glfwGetTime();
        if(newTime-m_previousTime > 1.0) {
            std::cout<<"fps: "<<m_frameCount / (newTime-m_previousTime)<<std::endl;
            m_frameCount = 0;
            m_previousTime = newTime;
        }
    }
}


int gl_window::getWindowWidth() {
    return m_width;
}

int gl_window::getWindowHeight() {
    return m_height;
}

bool gl_window::shouldClose() {
    // Check if the ESC key was pressed or the window was closed
    return !(glfwGetKey(m_window, GLFW_KEY_ESCAPE ) != GLFW_PRESS
             && glfwWindowShouldClose(m_window) == 0);
}

