#pragma once

// C / C++
#include <iostream>
#include <string>
#include <fstream>
#include <math.h>

// GLEW
#include <GL/glew.h>

// GLFW
#include <GLFW/glfw3.h>

// Tesseract
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

// OpenCV
#include <opencv2/opencv.hpp>

// FTGL
#include <FTGL/ftgl.h>

// JSONCPP
#include <json/json.h>

#include "PoseEstimation.h"

#define VERTEX_SHADER_PATH "../shader/vshader.vert"
#define FRAGMENT_SHADER_PATH "../shader/fshader.frag"
#define META_JSON_PATH "../meta.json"
#define TESSERACT_DATA_PATH "../jpn_tess"
#define FTGL_FONT_PATH "../font/MSMINCHO.TTF"

#define WINDOW_HEIGHT 480
#define WINDOW_WIDTH 640

#define DRAW_ALL_LINES 1

/* PI */
#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif
// Read File text
std::string readFile(std::string filepath)
{
    std::string res, line;
    std::ifstream fin(filepath);
    if (!fin.is_open())
    {
        throw std::invalid_argument("No such file " + filepath);
    }
    while (std::getline(fin, line))
    {
        res += line + '\n';
    }
    fin.close();
    return res;
}

/* getShaderProgram
* Create and Compile Vertex/Frag Shaders
* @param fshader : path to frag shader
* @param vshader : path to vert shader
* @return binded shader program
*/
GLuint getShaderProgram(std::string fshader, std::string vshader)
{
    // read into string
    std::string vSource = readFile(vshader);
    std::string fSource = readFile(fshader);
    const char* vpointer = vSource.c_str();
    const char* fpointer = fSource.c_str();

    GLint success;
    GLchar infoLog[512];

    // Compile VertexShader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, (const GLchar**)(&vpointer), NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);   // Detect errors
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        throw std::invalid_argument("Fail to compile vertex shader " + std::string(infoLog));
    }

    // Compile FragShader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, (const GLchar**)(&fpointer), NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);   // 错误检测
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        throw std::invalid_argument("Fail to compile fragment shader " + std::string(infoLog));
    }

    // Link shader to program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Clean
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Init Tesseract API for Character Recognition
tesseract::TessBaseAPI* initTesseract(std::string whitelist) {
    tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();
    // Set DATA_PATH to traindata path
    if (api->Init(TESSERACT_DATA_PATH, "jpn", tesseract::OcrEngineMode::OEM_TESSERACT_ONLY)) {
        throw std::invalid_argument("Fail to load jpn tesseract traindata.");
    }

    api->SetVariable("user_defined_dpi", "300");
    // block numbers and some special characters
    api->SetVariable("tessedit_char_blacklist", "0123456789!@#$%^&*()_+-=");
    api->SetVariable("tessedit_char_whitelist", whitelist.c_str());
    // set Page Segmentation mode to detect just for single char (kanji)
    api->SetPageSegMode(tesseract::PSM_SINGLE_CHAR);

    return api;
}

// Callback from slider control
static void on_trackbar(int pos, void* slider_value) {
    *(static_cast<int*>(slider_value)) = pos;
}

// Init OpenGL Rendering Pipleline
void initGL() {
    // Use fixed 
    glUseProgram(0);

    // For texturing the OpenCV WebCam feed as background in OpenGL
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // For glTexImage2D​ -> Define the texture image representation
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // Turn the texture coordinates from OpenCV to the texture coordinates OpenGL
    glPixelZoom(1.0, -1.0);

    // Enable and set colors
    glEnable(GL_COLOR_MATERIAL);
    glClearColor(0, 0, 0, 1.0);

    // Enable and set depth parameters
    glEnable(GL_DEPTH_TEST);
    glClearDepth(1.0);
}

void setUpFrustum(GLFWwindow* window, int width, int height) {
    // Set a whole-window viewport
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float ratio = (GLfloat)width / (GLfloat)height;
    float near = 0.01f, far = 100.f;

    // The camera should be calibrated -> a calibration results in the projection matrix -> then load the matrix
    // -> into GL_PROJECTION
    // -> adjustment of FOV 45 is needed for each camera
    float top = tan((double)(45.0 * M_PI / 360.0f)) * near;
    float bottom = -top;
    float left = ratio * bottom;
    float right = ratio * top;
    glFrustum(left, right, bottom, top, near, far);
}

// Init FTGL font for rendering Japanese words in 3D Context
FTFont* initFTGL() {
    // Use Mincho font by default
    FTFont* font = new FTExtrudeFont(FTGL_FONT_PATH);

    if (font->Error()) {
        throw std::invalid_argument("No such font file!");
    }

    // Set font size, depth (z-depth) and encoding
    font->FaceSize(1.5);
    font->Depth(0.1);
    font->CharMap(ft_encoding_unicode);
    return font;
}

/* getVirtualPose
* Calculate the RT-Pose for a virtual marker
* Virtual means it should not exist in real world
* @param corners : vector of 4 corners points 
* @return RT-Pose of the virtualMarker
*/
cv::Mat getVirtualPose(std::vector<cv::Point2f> corners) {
    cv::Point2f targetCorners[4];
    targetCorners[0].x = -0.5; targetCorners[0].y = -0.5;
    targetCorners[1].x = 99.5; targetCorners[1].y = -0.5;
    targetCorners[2].x = 99.5; targetCorners[2].y = 99.5;
    targetCorners[3].x = -0.5; targetCorners[3].y = 99.5;
    cv::Mat homographyMatrix(cv::Size(3, 3), CV_32FC1);
    homographyMatrix = cv::getPerspectiveTransform(corners.data(), targetCorners);
    for (int i = 0; i < 4; i++) {
        corners[i].x -= (WINDOW_WIDTH / 2);
        corners[i].y = -corners[i].y + (WINDOW_WIDTH / 2);
    }
    float resultMatrix[16];
    estimateSquarePose(resultMatrix, corners.data(), 0.041);
    cv::Mat resPose = (cv::Mat_<float>(4, 4) << resultMatrix[0], resultMatrix[1], resultMatrix[2], resultMatrix[3],
        resultMatrix[4], resultMatrix[5], resultMatrix[6], resultMatrix[7],
        resultMatrix[8], resultMatrix[9], resultMatrix[10], resultMatrix[11],
        resultMatrix[12], resultMatrix[13], resultMatrix[14], resultMatrix[15]);

    return resPose;
}

