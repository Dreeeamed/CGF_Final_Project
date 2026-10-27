#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "castle_geometry.h"
#include "hall_geometry.h"
#include "material.h"

namespace {
void setMatrix(GLuint program, const char* name, const glm::mat4& matrix) {
    glUniformMatrix4fv(glGetUniformLocation(program,name),1,GL_FALSE,glm::value_ptr(matrix));
}

// Save the actual OpenGL framebuffer for visual review, before swapping buffers.
bool captureFrame(const std::filesystem::path& path, int width, int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream file(path,std::ios::binary);
    if (!file) return false;
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y=height-1; y>=0; --y)
        file.write(reinterpret_cast<const char*>(pixels.data()+y*width*3),width*3);
    return bool(file);
}
}

int main(int argc, char* argv[]) {
    const bool capture = argc == 3 && std::string(argv[1]) == "--capture-dir";
    const bool smokeTest = capture || (argc == 2 && std::string(argv[1]) == "--smoke-test");
    const std::filesystem::path captureDir = capture ? argv[2] : "";
    if (capture) {
        std::error_code error;
        std::filesystem::create_directories(captureDir,error);
        if (error) { std::cerr << "Could not create capture directory.\n"; return 1; }
    }
    glfwSetErrorCallback([](int code,const char* description) {
        std::cerr << "GLFW error " << code << ": " << description << '\n';
    });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES,4);
    if (smokeTest) glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    GLFWwindow* window=glfwCreateWindow(1440,900,"Castle - The Grand Hall (2.5D)",nullptr,nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glfwSwapInterval(smokeTest ? 0 : 1);
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n';

    GLuint stoneProgram=createShaderProgram();
    GLuint hallProgram=createShaderProgram("shaders/hall.vert","shaders/hall.frag");
    GLuint texture=loadTexture2D("assets/textures/stone_wall/albedo.png");
    if (!stoneProgram || !hallProgram || !texture) {
        glDeleteProgram(stoneProgram); glDeleteProgram(hallProgram); glDeleteTextures(1,&texture);
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
    CastleGeometry castle;
    castle.build();
    grand_hall::Hall hall;
    hall.build();
    for (GLuint program : {stoneProgram,hallProgram}) {
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program,"stoneTexture"),0);
    }

    bool insideHall=true, showForeground=false, overview=false;
    bool wasC=false, wasO=false;
    float knightX=-6.0f, cameraX=0.0f, distance=84.0f, lookHeight=24.0f;
    float previousTime=static_cast<float>(glfwGetTime());
    int renderedFrames=0, exitCode=0;
    std::cout << "F1 exterior | F2 hall | A/D walk | Shift faster | W/S zoom\n"
                 "Up/Down look | C full hall / cutaway | O overview | Home reset | Esc quit\n";
    const char* captures[]={"hall-centre.ppm","hall-overview.ppm","hall-entrance.ppm",
                            "hall-throne.ppm","hall-full-row.ppm","exterior.ppm"};
    while (!glfwWindowShouldClose(window)) {
        const float time=static_cast<float>(glfwGetTime());
        const float dt=std::min(time-previousTime,0.05f);
        previousTime=time;
        glfwPollEvents();
        auto pressed=[&](int key) { return glfwGetKey(window,key)==GLFW_PRESS; };
        if (pressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window,true);
        if (pressed(GLFW_KEY_F1) && insideHall) {
            insideHall=false; overview=false; cameraX=0; distance=14; lookHeight=3.5f;
        }
        if (pressed(GLFW_KEY_F2) && !insideHall) {
            insideHall=true; cameraX=knightX; distance=84; lookHeight=24;
        }
        if (pressed(GLFW_KEY_C) && !wasC) showForeground=!showForeground;
        if (pressed(GLFW_KEY_O) && !wasO && insideHall) {
            overview=!overview;
            distance=overview ? 154.0f : 84.0f;
            lookHeight=overview ? 26.0f : 24.0f;
        }
        wasC=pressed(GLFW_KEY_C); wasO=pressed(GLFW_KEY_O);
        if (pressed(GLFW_KEY_HOME) && insideHall) {
            overview=false; showForeground=false; distance=84; lookHeight=24;
        }
        const float direction=float(pressed(GLFW_KEY_D))-float(pressed(GLFW_KEY_A));
        if (insideHall) {
            knightX=std::clamp(knightX+direction*(pressed(GLFW_KEY_LEFT_SHIFT)?18.0f:7.0f)*dt,
                               -105.0f,87.0f);
            const float targetX=overview ? 0.0f : std::clamp(knightX,-78.0f,78.0f);
            cameraX+=(targetX-cameraX)*(1.0f-std::exp(-5.0f*dt));
        } else cameraX=std::clamp(cameraX+direction*3.0f*dt,-11.0f,11.0f);
        distance+=(float(pressed(GLFW_KEY_S))-float(pressed(GLFW_KEY_W)))*dt*(insideHall?28.0f:4.0f);
        lookHeight+=(float(pressed(GLFW_KEY_UP))-float(pressed(GLFW_KEY_DOWN)))*dt*(insideHall?12.0f:6.0f);
        distance=insideHall?std::clamp(distance,24.0f,160.0f):std::clamp(distance,6.0f,18.0f);
        lookHeight=insideHall?std::clamp(lookHeight,0.0f,52.0f):std::clamp(lookHeight,-5.0f,25.0f);

        // Deterministic views cover both scenes and the foreground toggle.
        if (smokeTest) {
            insideHall=true; cameraX=0; knightX=-6; distance=84; lookHeight=24; showForeground=false;
            if (capture) {
                if (renderedFrames==1) { distance=154; lookHeight=26; }
                if (renderedFrames==2) { cameraX=-89; knightX=-98; distance=40; lookHeight=10; }
                if (renderedFrames==3) { cameraX=88; knightX=85; distance=42; lookHeight=12; }
                if (renderedFrames==4) showForeground=true;
                if (renderedFrames==5) { insideHall=false; distance=14; lookHeight=3.5f; }
            } else {
                if (renderedFrames==1) showForeground=true;
                if (renderedFrames==2) { insideHall=false; distance=14; lookHeight=3.5f; }
            }
        }

        int width,height;
        glfwGetFramebufferSize(window,&width,&height);
        // Minimised window: sleep until the next event instead of spinning the CPU.
        if (width==0 || height==0) { glfwWaitEvents(); continue; }
        glViewport(0,0,width,height);
        glClearColor(0.025f,0.042f,0.072f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        const glm::vec3 cameraPos(cameraX,insideHall?lookHeight+12.0f:4.0f,distance);
        const glm::mat4 view=glm::lookAt(cameraPos,glm::vec3(cameraX,lookHeight,0),glm::vec3(0,1,0));
        const glm::mat4 projection=glm::perspective(glm::radians(50.0f),float(width)/height,0.1f,500.0f);
        const GLuint program=insideHall?hallProgram:stoneProgram;
        glUseProgram(program);
        setMatrix(program,"model",glm::mat4(1));
        setMatrix(program,"view",view);
        setMatrix(program,"projection",projection);
        glUniform3fv(glGetUniformLocation(program,"cameraPos"),1,glm::value_ptr(cameraPos));
        glUniform3f(glGetUniformLocation(program,"lightPos"),-8,12,6);
        glUniform1f(glGetUniformLocation(program,"time"),smokeTest?0.0f:time);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,texture);
        if (insideHall) {
            hall.architecture.draw();
            if (showForeground) hall.foreground.draw();
            setMatrix(program,"model",glm::translate(glm::mat4(1),glm::vec3(knightX,0.06f,0)));
            hall.knight.draw();
            setMatrix(program,"model",glm::mat4(1));
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            hall.lightShafts.draw();
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        } else castle.draw();
        if (capture && !captureFrame(captureDir/captures[renderedFrames],width,height)) {
            std::cerr << "Could not save capture.\n"; exitCode=1; break;
        }
        glfwSwapBuffers(window);
        if (smokeTest) {
            const GLenum error=glGetError();
            if (error!=GL_NO_ERROR) {
                std::cerr << "OpenGL smoke test failed: " << error << '\n'; exitCode=1; break;
            }
            if (++renderedFrames==(capture?6:3)) {
                std::cout << "Smoke test passed: " << renderedFrames << " frames rendered.\n";
                break;
            }
        }
    }
    hall.destroy(); castle.destroy();
    glDeleteTextures(1,&texture);
    glDeleteProgram(hallProgram); glDeleteProgram(stoneProgram);
    glfwDestroyWindow(window); glfwTerminate();
    return exitCode;
}
