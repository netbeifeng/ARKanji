#include "Util.h"
#include "Tracker.h"
#include "MetaManager.h"

// tuple => monji1Id, monji2Id, tangoId
std::vector<std::tuple<int, int, int>> monjiCombinations;

// Rendering camera frame input as background in OpenGL 
void renderBackground(cv::Mat frame) {
    // Cam Height * Cam Width * #RGB Channel 3
    unsigned char bkgnd[WINDOW_HEIGHT * WINDOW_WIDTH * 3];
    // Copy data from cv::Mat frame
    memcpy(bkgnd, frame.data, sizeof(bkgnd));

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    // No position changes
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);

    glPushMatrix();
    glLoadIdentity();

    // In the ortho view all objects stay the same size at every distance
    glOrtho(0.0, WINDOW_WIDTH, 0.0, WINDOW_HEIGHT, -1, 1);
    glRasterPos2i(0, WINDOW_HEIGHT - 1);
    glDrawPixels(WINDOW_WIDTH, WINDOW_HEIGHT, GL_BGR_EXT, GL_UNSIGNED_BYTE, bkgnd);

    glPopMatrix();
}

// Draw combination lines for monjis 
// The line will be drawn in OpenCV frame, not directly in OpenGL world
// The frame with lines will be passed into OpenGL as background, which is more easy to implement
// Compared with drawing lines in the context of OpenGL
void drawLines(Tracker tracker, MetaManager metaManager, cv::Mat img) {
    std::map<int, cv::Point2f> centersMap = tracker.getDetectedMarkerCenter();

    // Need to draw, only when multiple markers are detected
    if (centersMap.size() >= 2) {
        std::vector<std::pair<int, cv::Point>> centers;
        for (auto const& x : centersMap) {
            centers.push_back(std::make_pair(x.first, x.second));
        }

        // Check the position relationship between two detected markers
        // Are they in right sequence? 
        // Like 日本 not 本日 (means today, but .. hard to find a model to represent, so exclude here
        for (int i = 0; i < centers.size(); i++) {
            for (int j = i + 1; j < centers.size(); j++) {
                cv::Point2f point_a = centers[i].second;
                cv::Point2f point_b = centers[j].second;
                std::pair<int, cv::Point2f> left;
                std::pair<int, cv::Point2f> right;

                if (point_a.x < point_b.x) {
                    left = centers[i];
                    right = centers[j];
                }
                else {
                    left = centers[j];
                    right = centers[i];
                }

                // When two monjis are correctly ordered => get the tangoId
                int tangoId = metaManager.getTangoId(left.first, right.first);
                if (tangoId != -1) {
                    cv::line(img, left.second, right.second, cv::Scalar(0, 255, 0), 2);
                    monjiCombinations.push_back(std::make_tuple(left.first, right.first, tangoId));
                }
                else {
                    if (DRAW_ALL_LINES) {
                        cv::line(img, left.second, right.second, cv::Scalar(0, 0, 255), 2);
                    }
                }
            }
        }
    }
}

// Rendering Yomikata and Presentation-Model
void renderObjs(Tracker tracker, MetaManager metaManager, GLuint program, FTFont* font) {
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);

    std::map<int, cv::Mat> markers = tracker.getDetectedMarkerPose();
    
    for (auto const& markerPair : markers) {
        float* resultMatrix = (float*)markerPair.second.data;

        // Transpose the found RT-Pose  
        float resultTransposedMatrix[16];
        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                resultTransposedMatrix[x * 4 + y] = resultMatrix[y * 4 + x];
            }
        }

        // Rendering Yomikata 読み方 by using FTGL
        glUseProgram(0);
        glLoadMatrixf(resultTransposedMatrix);
        glRotatef(-90, 1, 0, 0);
        glRotatef(90, 0, 1, 0);
        glRotatef(-90, 1, 0, 0);
        glScalef(0.01, 0.01, 0.01);

        // Set the Color for Font Rendering
        glColor3f(0, 0.36, 0.08); // しんぺき -> https://irocore.com/shinpeki/

        // Rendering Onyomi Text
        glTranslatef(-3, 3.5, 0);
        std::string onyomi = u8"音読み：";
        font->Render((onyomi + metaManager.getOnyomiById(markerPair.first)).c_str());

        // Rendering Kunyomi Text
        glTranslatef(0, 1.5, 0);
        std::string kunyomi = u8"訓読み：";
        font->Render((kunyomi + metaManager.getKunyomiById(markerPair.first)).c_str());

        // Reset the MV matrix and start rendering the imported model 
        glLoadIdentity();
        glUseProgram(program);

        if (resultMatrix[0] != -1) {
            GLfloat projection[16];
            glGetFloatv(GL_PROJECTION_MATRIX, projection);

            // Get tunning matrix to rescale, rotation ...
            glm::mat4 tunning = metaManager.getModelTunningMatrix(markerPair.first, glm::make_mat4(resultTransposedMatrix));
            
            // Pass the uniform to shader
            GLuint location = glGetUniformLocation(program, "MVP");
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(glm::make_mat4(projection) * tunning));
            
            // Rendering the model
            metaManager.getModelById(markerPair.first).draw(program);   
        }
    }
}
// Rendering the possible monjis combination => tangos
void renderCombis(Tracker tracker, MetaManager modelManager, GLuint program, FTFont* font) {
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);

    // Get the current furstum projection
    GLfloat projection[16];
    glGetFloatv(GL_PROJECTION_MATRIX, projection);

    for (std::tuple<int, int, int> combiPair : monjiCombinations) {
        int m1Id = std::get<0>(combiPair); // monjiId1
        int m2Id = std::get<1>(combiPair); // monjiId2

        std::vector<cv::Point2f> m1MarkerCorners = tracker.getMarkerCornersById(m1Id); // Corners of monjiMarker1
        std::vector<cv::Point2f> m2MarkerCorners = tracker.getMarkerCornersById(m2Id); // Corners of monjiMarker2

        cv::Point2f m1Center = tracker.getMarkerCenterById(m1Id); // Center of MonjiMarker1
        cv::Point2f m2Center = tracker.getMarkerCenterById(m2Id); // Center of MonjiMarker2
        cv::Point2f offset = (m2Center - m1Center) / 2.0; // The offset, we should set to middle point

        // to m1MarkerCorners[i] + (m2Center - m1Center) / 2.0
        for (int i = 0; i < 4; i++) {
            m1MarkerCorners[i] += offset;
        }

        // Now we have a virtual marker at the middle point of marker1 and marker2

        // Get the pose of that virtual marker
        float* resultMatrix = (float*)getVirtualPose(m1MarkerCorners).data;

        float resultTransposedMatrix[16];
        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                resultTransposedMatrix[x * 4 + y] = resultMatrix[y * 4 + x];
            }
        }

        glUseProgram(0);
        glLoadMatrixf(resultTransposedMatrix);
        glRotatef(-90, 1, 0, 0);
        glRotatef(90, 0, 1, 0);
        glRotatef(-90, 1, 0, 0);
        glRotatef(180, 0, 0, 1);
        glScalef(0.01, 0.01, 0.01);

        // Set the Color for Font Rendering
        glColor3f(0, 0.35, 0.6); // こんぺき -> https://irocore.com/konpeki/

        // Rendering Tango Text
        glTranslatef(-1, -5, 0);
        font->Render((modelManager.getTangoById(std::get<2>(combiPair))).c_str());

        glLoadIdentity();

        glUseProgram(program);

        glm::mat4 tunning = modelManager.getModelTunningMatrix(std::get<2>(combiPair), glm::make_mat4(resultTransposedMatrix));
        GLuint location = glGetUniformLocation(program, "MVP");
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(glm::make_mat4(projection) * tunning));

        // Rendering the tango Model
        modelManager.getModelById(std::get<2>(combiPair)).draw(program);
    }
}
int main(int argc, char** argv)
{
    // Read the meta.json contains the possible kanjis and tangos 
    std::ifstream metaJson(META_JSON_PATH, std::ifstream::binary);
    Json::Value meta; // Parse in Jsoncpp library
    metaJson >> meta;

    // Init FTGL font for text rendering in 3d
    FTFont* font = initFTGL();
    
    // Init OpenCV VideoStream read stream from camera
    cv::VideoCapture capture(0);
    if (!capture.isOpened()) {
        std::cout << "Cannot open video: " << 0 << std::endl;
        exit(EXIT_FAILURE);
    }
    cv::namedWindow("ARKanji - Tracking", cv::WINDOW_AUTOSIZE);

    // Slider for setting B/W-Threshold value
    int slider_value = 100;
    cv::createTrackbar("Threshold", "ARKanji - Tracking", &slider_value, 255, on_trackbar, &slider_value);

    // Init GLFW window 
    GLFWwindow* window;
    if (!glfwInit())
        return -1;
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "ARKanji", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwSetFramebufferSizeCallback(window, setUpFrustum);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Setup Projection fov, etc 
	setUpFrustum(window, WINDOW_WIDTH, WINDOW_HEIGHT);
    
    // Init GLEW / OpenGL
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        std::cout << "GLEW initialisation error: " << glewGetErrorString(err) << std::endl;
        exit(-1);
    }
    std::cout << "GLEW okay - using version: " << glewGetString(GLEW_VERSION) << std::endl;
    initGL();

    // MetaManager instance
    // For loading Models of Monjis and Tangos
    MetaManager metaManager = MetaManager(meta);

    // Generate String for Tesseract char whitelist
    std::string whiteListKanjis = "";
    for (int i = 0; i < metaManager.getMonjisSize(); i++) {
        whiteListKanjis += metaManager.getKanjiById(i);
    }

    // Init Tesseract for Kanji recognition
    tesseract::TessBaseAPI* api = initTesseract(whiteListKanjis);

    // Tracker instance 
    // api => Tesseract API for recognizing marker content
    // meta["monji"] => only needs recognizing monjis part 
    Tracker tracker = Tracker(api, meta["monji"]);

    // Compiled Shader program for rendering imported models with textures
    GLuint program = getShaderProgram(FRAGMENT_SHADER_PATH, VERTEX_SHADER_PATH);

    // Feed-in frame from camera 
    cv::Mat frame;

    while (!glfwWindowShouldClose(window)) {
        if (!capture.read(frame)) {
            std::cout << "Cannot grab a frame." << std::endl;
            break;
        }
        capture >> frame; // Read in

        // Track the current frame => Searching markers and recognizing kanjis
        cv::Mat trackingFrame = tracker.track(frame, slider_value);
        cv::imshow("ARKanji - Tracking", trackingFrame);

        // Draw combination lines if combinable kanjis found
        drawLines(tracker, metaManager, frame);

        // Render background by filling Camera frame
        glUseProgram(0);
        renderBackground(frame);

        // Render Models and Yomikata-Instructions
        glUseProgram(program);
        renderObjs(tracker, metaManager, program, font);

        // Render found Tangos
        glUseProgram(program);
        renderCombis(tracker, metaManager, program, font);

        // Clean all maps of old frame
        tracker.cleanDetectedMarkers();
        monjiCombinations.clear();

        // Swap Buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Termination Cleaning
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
