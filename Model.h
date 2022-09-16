#pragma once

// C / C++
#include <map>
#include <vector>
#include <iostream>

// OpenGL
#include <GL/glew.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// SOIL
#include <SOIL2/SOIL2.h>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// For rendering imported aiMesh
class Mesh
{
public:
    GLuint vao, vbo, ebo;
    GLuint diffuseTexture; 

    // Vertex Properties
    std::vector<glm::vec3> vertexPosition;
    std::vector<glm::vec2> vertexTexcoord;
    std::vector<glm::vec3> vertexNormal;

    // glDrawElements indexing
    std::vector<int> index;

    void bindData()
    {
        // Vertex array
        glGenVertexArrays(1, &vao); 
        glBindVertexArray(vao);  	

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
            vertexPosition.size() * sizeof(glm::vec3) +
            vertexTexcoord.size() * sizeof(glm::vec2) +
            vertexNormal.size() * sizeof(glm::vec3),
            NULL, GL_STATIC_DRAW);

        // Position
        GLuint offset_position = 0;
        GLuint size_position = vertexPosition.size() * sizeof(glm::vec3);
        glBufferSubData(GL_ARRAY_BUFFER, offset_position, size_position, vertexPosition.data());
        glEnableVertexAttribArray(0);   // In shader (layout = 0) represents vertex pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(offset_position));

        // UV coordinates
        GLuint offset_texcoord = size_position;
        GLuint size_texcoord = vertexTexcoord.size() * sizeof(glm::vec2);
        glBufferSubData(GL_ARRAY_BUFFER, offset_texcoord, size_texcoord, vertexTexcoord.data());
        glEnableVertexAttribArray(1);   // In shader (layout = 1) represents texture coords
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(offset_texcoord));

        // Normals
        GLuint offset_normal = size_position + size_texcoord;
        GLuint size_normal = vertexNormal.size() * sizeof(glm::vec3);
        glBufferSubData(GL_ARRAY_BUFFER, offset_normal, size_normal, vertexNormal.data());
        glEnableVertexAttribArray(2);   // In shader (layout = 2) represents normals
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(offset_normal));

        // Pass index to ebo
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, index.size() * sizeof(GLuint), index.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
    }
    void draw(GLuint program)
    {
        glBindVertexArray(vao);

        // Texturing
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseTexture);
        glUniform1i(glGetUniformLocation(program, "texture"), 0);

        // Drawing
        glDrawElements(GL_TRIANGLES, this->index.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

class Model
{
public:
    std::vector<Mesh> meshes;
    std::map<std::string, GLuint> textureMap;
    Model() {}
    void load(std::string filepath)
    {
        Assimp::Importer import;
        const aiScene* scene = import.ReadFile(filepath, aiProcess_Triangulate | aiProcess_FlipUVs);
        if (!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            throw std::invalid_argument(import.GetErrorString());
        }

        // Path to model file, read from json
        std::string rootPath = filepath.substr(0, filepath.find_last_of('/'));

        // Load the meshes from imported scene (assimp)
        for (int i = 0; i < scene->mNumMeshes; i++)
        {
            meshes.push_back(Mesh());
            Mesh& mesh = meshes.back();
            aiMesh* aimesh = scene->mMeshes[i];

            // Init Mesh from aiMesh
            for (int j = 0; j < aimesh->mNumVertices; j++)
            {
                // Vertices
                glm::vec3 vvv;
                vvv.x = aimesh->mVertices[j].x;
                vvv.y = aimesh->mVertices[j].y;
                vvv.z = aimesh->mVertices[j].z;
                mesh.vertexPosition.push_back(vvv);

                // Normals
                vvv.x = aimesh->mNormals[j].x;
                vvv.y = aimesh->mNormals[j].y;
                vvv.z = aimesh->mNormals[j].z;
                mesh.vertexNormal.push_back(vvv);

                // Textures
                glm::vec2 vv(0, 0);
                if (aimesh->mTextureCoords[0])
                {
                    vv.x = aimesh->mTextureCoords[0][j].x;
                    vv.y = aimesh->mTextureCoords[0][j].y;
                }
                mesh.vertexTexcoord.push_back(vv);
            }

            // Materials
            if (aimesh->mMaterialIndex >= 0)
            {
                aiMaterial* material = scene->mMaterials[aimesh->mMaterialIndex];

                // Get diffuse map
                aiString aistr;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &aistr);
                std::string texpath = aistr.C_Str();
                texpath = rootPath + '/' + texpath;  

                if (textureMap.find(texpath) == textureMap.end())
                {
                    // If no texture exists, then generate
                    GLuint tex;
                    glGenTextures(1, &tex);
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
                    int textureWidth, textureHeight;
                    unsigned char* image = SOIL_load_image(texpath.c_str(), &textureWidth, &textureHeight, 0, SOIL_LOAD_RGB);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureWidth, textureHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, image);   // 生成纹理
                    delete[] image;

                    textureMap[texpath] = tex;
                }
                mesh.diffuseTexture = textureMap[texpath];
            }

            // Make sure the face information still there
            for (GLuint j = 0; j < aimesh->mNumFaces; j++)
            {
                aiFace face = aimesh->mFaces[j];
                for (GLuint k = 0; k < face.mNumIndices; k++)
                {
                    mesh.index.push_back(face.mIndices[k]);
                }
            }

            mesh.bindData();
        }
    }

    // Draw all formed meshes
    void draw(GLuint program)
    {
        for (int i = 0; i < meshes.size(); i++)
        {
            meshes[i].draw(program);
        }
    }
};
