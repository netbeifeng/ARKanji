// C / C++
#include <exception>

// JSON
#include <json/json.h>

#include "Model.h"

// Get Meta-information from meta.json 
class MetaManager {
    public:
        MetaManager(Json::Value para_json) {
            json = para_json;
            monjis = para_json["monji"];
            tangos = para_json["tango"];

            // Load model for every monji 文字　もんじ
            for (int i = 0; i < monjis.size(); i++) {
                Model model;
                model.load("../" + monjis[i]["model"].asString());
                modelMap[monjis[i]["id"].asInt()] = model;
            }

            // Load model for every tango　単語　たんご
            for (int i = 0; i < tangos.size(); i++) {
                Model model;
                model.load("../" + tangos[i]["model"].asString());
                modelMap[tangos[i]["id"].asInt()] = model;
            }
        }

        int getIdByKanji(std::string kanji) {
            for (int i = 0; i < monjis.size(); i++) {
                if (kanji.find(monjis[i]["kanji"].asString()) != std::string::npos) {
                    return monjis[i]["id"].asInt();
                }
            }
            return -1;
        }

        std::string getKanjiById(int id) {
            for (int i = 0; i < monjis.size(); i++) {
                if (id == monjis[i]["id"].asInt()) {
                    return monjis[i]["kanji"].asString();
                }
            }
            return "";
        }

        Model getModelById(int id) {
            if (id < 0) {
                throw std::invalid_argument("Id must be a postive integer.");
            }
            return modelMap.at(id);
        }

        Model getModelById(std::string id) {
            return modelMap.at(std::stoi(id));
        }

        Model getModelByKanji(std::string kanji) {
            int id = getIdByKanji(kanji);
            return getModelById(id);
        }

        // Find whether two monjis can form a tango, if yes return id of tango
        int getTangoId(int id1, int id2) {
            for (int i = 0; i < tangos.size(); i++) {
                int tangoId = tangos[i]["id"].asInt();
                if ((tangoId / 10) == id1 && (tangoId % 10) == id2) {
                    return tangos[i]["id"].asInt();
                }
            }
            return -1;
        }

        // Return u8 encoded onyomi string
        std::string getOnyomiById(int id) {
            for (int i = 0; i < monjis.size(); i++) {
                if (id == monjis[i]["id"].asInt()) {
                    return monjis[i]["onyomi"].asString();
                }
            }
            return "";
        }

        // Return u8 encoded kunyomi string
        std::string getKunyomiById(int id) {
            for (int i = 0; i < monjis.size(); i++) {
                if (id == monjis[i]["id"].asInt()) {
                    return monjis[i]["kunyomi"].asString();
                }
            }
            return "";
        }

        // Hard-coded function for adjusting the models from internet
        // All models are somehow not uniformly created in different scale and also rotation
        // It would be more recommendable to use 3d modeling software to make all resources in same scale
        // Since we only have few models, I write it here for understanding
        glm::mat4 getModelTunningMatrix(int id, glm::mat4 mv) {
            glm::mat4 tunning;
            switch(id) {
                case 1: {
                    glm::mat4 scale = glm::scale(mv, glm::vec3(0.0005, 0.0005, 0.0005));
                    glm::mat4 rotate = glm::rotate(scale, glm::radians(-90.f), glm::vec3(1.0, 0.0, 0.0));
                    tunning = rotate;
                    break;
                } 

                case 2: {
                    glm::mat4 scale = glm::scale(mv, glm::vec3(0.0015, 0.0015, 0.0015));
                    tunning = scale;
                    break;
                }

                case 3: {
                    glm::mat4 scale = glm::scale(mv, glm::vec3(0.003, 0.003, 0.003));
                    glm::mat4 rotate = glm::rotate(scale, glm::radians(180.f), glm::vec3(1.0, 0.0, 0.0));
                    tunning = rotate;
                    break;
                }

                case 4: {
                    glm::mat4 scale = glm::scale(mv, glm::vec3(0.018, 0.018, 0.018));
                    glm::mat4 rotate = glm::rotate(scale, glm::radians(180.f), glm::vec3(1.0, 0.0, 0.0));
                    glm::mat4 rotate2 = glm::rotate(rotate, glm::radians(90.f), glm::vec3(0.0, 0.0, 1.0));
                    tunning = rotate2;
                    break;
                }

                case 5: {
                    glm::mat4 scale = glm::scale(mv, glm::vec3(0.03, 0.03, 0.03));
                    glm::mat4 rotate = glm::rotate(scale, glm::radians(180.f), glm::vec3(1.0, 0.0, 0.0));
                    tunning = rotate;
                    break;
                }

                case 6: {
                    glm::mat4 scale = glm::scale(mv, glm::vec3(0.001, 0.001, 0.001));
                    glm::mat4 rotate = glm::rotate(scale, glm::radians(270.f), glm::vec3(1.0, 0.0, 0.0));
                    glm::mat4 rotate2 = glm::rotate(rotate, glm::radians(180.f), glm::vec3(0.0, 1.0, 0.0));
                    tunning = rotate2;
                    break;
                }

                case 31: {
                    glm::mat4 scale = glm::scale(mv, glm::vec3(0.001, 0.001, 0.001));
                    glm::mat4 rotate = glm::rotate(scale, glm::radians(180.f), glm::vec3(1.0, 0.0, 0.0));
                    tunning = rotate;
                    break;
                }

                case 24: {
                    glm::mat4 scale = glm::scale(mv, glm::vec3(0.01, 0.01, 0.01));
                    glm::mat4 rotate = glm::rotate(scale, glm::radians(90.f), glm::vec3(1.0, 0.0, 0.0));
                    tunning = rotate;
                    break;
                }

                case 56: {
                    glm::mat4 scale = glm::scale(mv, glm::vec3(0.006, 0.006, 0.006));
                    glm::mat4 rotate = glm::rotate(scale, glm::radians(180.f), glm::vec3(1.0, 0.0, 0.0));
                    tunning = rotate;
                    break;
                }

                default: {
                    tunning = glm::mat4(1.f);
                    break;
                }
            }
            return tunning;
        }

    private:
        Json::Value monjis;
        Json::Value tangos;
        Json::Value json;
        std::map<int, Model> modelMap;
};