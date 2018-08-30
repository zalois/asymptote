// shader handling
// Author: Supakorn "Jamie" Ras. 


#include <GL/glew.h>

#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include <iostream>

#include "shaders.h"

GLuint createShaders(GLchar const *src, int shaderType)
{
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE)
    {
        GLint length; 

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

        std::vector<GLchar> msg(length);

        glGetShaderInfoLog(shader, length, &length, msg.data());

        for(GLchar const& cha : msg)
        {
            std::cerr << cha;
        }

        std::cerr << std::endl << "GL Compile error" << std::endl;
        std::cerr << src << std::endl;
        throw 1; 
    }
    return shader;
}

GLuint createShaderFile(std::string file, int shaderType)
{
    std::ifstream shaderFile;
    shaderFile.open(file);
    std::stringstream shaderSrc;

    if (shaderFile)
    {
        shaderSrc << shaderFile.rdbuf();
        shaderFile.close();
    }
    else
    {
        throw 1;
    }

    return createShaders(shaderSrc.str().data(), shaderType);
}