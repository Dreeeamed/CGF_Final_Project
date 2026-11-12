#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "castle_geometry.h"
#include "hall_geometry.h"
#include "material.h"
#include "hud.h"

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
    GLuint texture = loadTexture2D("assets/textures/stone_wall/albedo.png");
    GLuint heightMap = loadTexture2D("assets/textures/stone_wall/height.png");
	GLuint normalMap = loadTexture2D("assets/textures/stone_wall/normal.png");
    if (!stoneProgram || !hallProgram || !texture || !heightMap || !normalMap) {
        glDeleteProgram(stoneProgram); glDeleteProgram(hallProgram);
        glDeleteTextures(1, &texture); glDeleteTextures(1, &heightMap); glDeleteTextures(1, &normalMap);
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
    CastleGeometry castle;
    castle.build();
    grand_hall::Hall hall;
    hall.build();
    for (GLuint program : {stoneProgram,hallProgram}) {
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program,"stoneTexture"),0);
        glUniform1i(glGetUniformLocation(program, "heightMap"), 1);
        glUniform1i(glGetUniformLocation(program, "normalMap"), 2);
    }
    Hud hud;
    if (!hud.init()) {
        std::cerr << "Could not create the on-screen text.\n";
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }

    bool insideHall=true, showForeground=false, overview=false;
    bool parallaxOn = true;// P: the headline technique on/off, for comparing
    bool wasC=false, wasO=false, wasP=false;
    // Q cycles the POM quality: how many layers the ray march and the shadow walk may use.
    struct Quality { const char* name; float minLayers, maxLayers; int shadowSteps; };
    const Quality qualities[] = { {"Low", 8.0f, 24.0f, 8}, {"Medium", 12.0f, 48.0f, 16},
                                 {"High", 24.0f, 96.0f, 32} };
    int quality = 1;
    bool vsync = !smokeTest, wasQ = false, wasV = false;
    bool showHelp = false, paused = false, wasH = false, wasSpace = false;
    float animTime = 0.0f;   // clock of the animations; stands still while paused
    float shownFps = 0.0f;   // last measured FPS, for the on-screen text
    int fpsFrames = 0;
    float fpsTime = 0.0f;
    float knightX=-6.0f, cameraX=0.0f, distance=84.0f, lookHeight=24.0f;
    float previousTime=static_cast<float>(glfwGetTime());
    int renderedFrames=0, exitCode=0;
	float lightYaw = glm::radians(60.0f), lightPitch = glm::radians(25.0f);
    std::cout << "F1 exterior | F2 hall | A/D walk | Shift faster | W/S zoom\n"
        "Up/Down look | C full hall / cutaway | O overview | P parallax | Home reset | Esc quit\n"
        "J/L turn the light | I/K raise / lower the light\n"
        "Q quality (Low / Medium / High) | V VSync on / off\n"
        "H help on screen | Space pause\n";
    const char* captures[]={"hall-centre.ppm","hall-overview.ppm","hall-entrance.ppm",
                            "hall-throne.ppm","hall-full-row.ppm","exterior.ppm"};
    while (!glfwWindowShouldClose(window)) {
        const float time=static_cast<float>(glfwGetTime());
        const float dt=std::min(time-previousTime,0.05f);
        const float frameTime = time - previousTime;
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
        if (pressed(GLFW_KEY_P) && !wasP) {
		    parallaxOn = !parallaxOn;
            std::cout << "Parallax " << (parallaxOn ? "ON" : "OFF") << '\n';
        }
        lightYaw += (float(pressed(GLFW_KEY_L)) - float(pressed(GLFW_KEY_J))) * dt * 1.2f;
		lightPitch = std::clamp(lightPitch + (float(pressed(GLFW_KEY_I)) - float(pressed(GLFW_KEY_K))) * dt * 0.8f, 0.05f, 1.5f);
        const glm::vec3 lightDir(std::cos(lightPitch) * std::sin(lightYaw), std::sin(lightPitch),std::cos(lightPitch) * std::cos(lightYaw));
        if (pressed(GLFW_KEY_Q) && !wasQ) {
            quality = (quality + 1) % 3;
            const Quality& q = qualities[quality];
            std::cout << "Quality " << q.name << ": " << q.minLayers << "-" << q.maxLayers
                << " layers, " << q.shadowSteps << " shadow steps\n";
        }
        if (pressed(GLFW_KEY_V) && !wasV) {
            vsync = !vsync;
            glfwSwapInterval(vsync ? 1 : 0);
            std::cout << "VSync " << (vsync ? "ON" : "OFF") << '\n';
        }
        if (pressed(GLFW_KEY_H) && !wasH) showHelp = !showHelp;
        if (pressed(GLFW_KEY_SPACE) && !wasSpace) paused = !paused;
        if (!paused) animTime += dt;
        wasH = pressed(GLFW_KEY_H); wasSpace = pressed(GLFW_KEY_SPACE);
        wasC = pressed(GLFW_KEY_C); wasO = pressed(GLFW_KEY_O); wasP = pressed(GLFW_KEY_P);
        wasQ = pressed(GLFW_KEY_Q); wasV = pressed(GLFW_KEY_V);
        // Twice a second: show the frame rate and the toggles in the window title.
        fpsFrames++;
        fpsTime += frameTime;
        if (fpsTime >= 0.5f) {
            const float fps = fpsFrames / fpsTime;
            shownFps = fps;
            char title[160];
            std::snprintf(title, sizeof(title), "Castle - %.0f FPS (%.1f ms) | POM %s | Quality %s | VSync %s",
                fps, 1000.0f / fps, parallaxOn ? "ON" : "OFF", qualities[quality].name,
                vsync ? "on" : "off");
            glfwSetWindowTitle(window, title);
            fpsFrames = 0;
            fpsTime = 0.0f;
        }
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
        glUniform3fv(glGetUniformLocation(program, "lightDir"), 1, glm::value_ptr(lightDir));
        glUniform3fv(glGetUniformLocation(program,"cameraPos"),1,glm::value_ptr(cameraPos));
        glUniform3f(glGetUniformLocation(program,"lightPos"),-8,12,6);
        glUniform1f(glGetUniformLocation(program,"time"),smokeTest?0.0f:animTime);
        glUniform1i(glGetUniformLocation(program, "parallaxOn"), parallaxOn);
        glUniform2f(glGetUniformLocation(program, "pomLayers"), qualities[quality].minLayers,
            qualities[quality].maxLayers);
        glUniform1i(glGetUniformLocation(program, "shadowSteps"), qualities[quality].shadowSteps);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, heightMap);
        glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, normalMap);
        glActiveTexture(GL_TEXTURE0);
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
        const glm::vec4 panel(0.02f, 0.03f, 0.05f, 0.65f), parchment(0.93f, 0.86f, 0.70f, 1.0f),
            gold(1.0f, 0.70f, 0.28f, 1.0f);
        char status[160];
        std::snprintf(status, sizeof(status), "POM: %s   Quality: %s   VSync: %s%s\n%.0f FPS (%.1f ms)   H - help",
            parallaxOn ? "ON" : "OFF", qualities[quality].name, vsync ? "on" : "off",
            paused ? "   PAUSED" : "", shownFps, shownFps > 0.0f ? 1000.0f / shownFps : 0.0f);
        const std::string info = std::string("Topic 15: 2.5D castle with parallax occlusion on walls\n")
            + "Akezhan Muzamelov, Bolatbek Bolat, Adil Dyussenov\n" + status;
        hud.begin(width, height);
        hud.rect(16, 16, hud.width(info) + 32, hud.height(info) + 24, panel);
        hud.text(32, 28, info, parchment);
        if (showHelp) {
            const std::string keys = "A / D\nShift\nW / S\nUp / Down\nO\nC\nHome\nF1 / F2\n"
                "P\nQ\nJ / L\nI / K\nSpace\nV\nH\nEsc";
            const std::string actions = "walk left / right\nwalk faster\nzoom in / out\nlook higher / lower\n"
                "overview of the hall\nfull hall / cutaway\nreset the camera\nexterior / hall\n"
                "parallax occlusion on / off\nPOM quality: Low / Medium / High\nturn the light\n"
                "raise / lower the light\npause animation\nVSync on / off\nhide this help\nquit";
            const float top = hud.height(info) + 56;
            const float column = hud.width(keys) + 32;
            hud.rect(16, top, column + hud.width(actions) + 32, hud.height(keys) + 24, panel);
            hud.text(32, top + 12, keys, gold);
            hud.text(32 + column, top + 12, actions, parchment);
        }
        hud.end();
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
    hud.destroy(); hall.destroy(); castle.destroy();
    glDeleteTextures(1,&texture);
    glDeleteTextures(1, &heightMap);
    glDeleteTextures(1, &normalMap);
    glDeleteProgram(hallProgram); glDeleteProgram(stoneProgram);
    glfwDestroyWindow(window); glfwTerminate();
    return exitCode;
}
