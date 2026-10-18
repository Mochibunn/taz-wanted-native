/* transforms.cpp - Scene graph and matrix transformation implementation */

#include "transforms.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace blitz {

TransformLoader::TransformLoader() {}
TransformLoader::~TransformLoader() {}

bool TransformLoader::loadSceneGraph(const std::string& filePath, std::vector<SceneGraphNode>& nodes) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    SceneGraphNode rootNode;
    rootNode.name = "Root";
    rootNode.position[0] = 0.0f; rootNode.position[1] = 0.0f; rootNode.position[2] = 0.0f;
    rootNode.rotation[0] = 0.0f; rootNode.rotation[1] = 0.0f; rootNode.rotation[2] = 0.0f;
    rootNode.scale[0] = 1.0f; rootNode.scale[1] = 1.0f; rootNode.scale[2] = 1.0f;
    rootNode.modelMatrix = Matrix4::identity();
    rootNode.valid = true;

    nodes.push_back(rootNode);
    return true;
}

bool TransformLoader::parseObjectTransforms(const std::string& filePath, std::vector<ObjectTransform>& transforms) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        ObjectTransform obj;
        obj.objectName = line;
        obj.position = glm::vec3(0.0f);
        obj.rotation = glm::vec3(0.0f);
        obj.scale = glm::vec3(1.0f);
        obj.modelMatrix = Matrix4::identity();
        
        transforms.push_back(obj);
    }

    return true;
}

Matrix4 TransformLoader::computeMVP(const SceneGraphNode& node, const CameraFrustum& camera) {
    glm::mat4 model = node.modelMatrix;
    glm::mat4 view = camera.viewMatrix;
    glm::mat4 proj = camera.projectionMatrix;
    
    glm::mat4 mvp = proj * view * model;
    return convertToMatrix4(mvp);
}

bool TransformLoader::setupCameraFrustum(CameraFrustum& camera,
                                         const float* pos,
                                         const float* target,
                                         const float* up,
                                         float fov,
                                         float aspect,
                                         float nearPlane,
                                         float farPlane) {
    camera.fov = fov;
    camera.aspect = aspect;
    camera.nearPlane = nearPlane;
    camera.farPlane = farPlane;

    if (pos) {
        camera.position[0] = pos[0];
        camera.position[1] = pos[1];
        camera.position[2] = pos[2];
    }
    if (target) {
        camera.target[0] = target[0];
        camera.target[1] = target[1];
        camera.target[2] = target[2];
    }
    if (up) {
        camera.up[0] = up[0];
        camera.up[1] = up[1];
        camera.up[2] = up[2];
    }

    glm::vec3 eye(camera.position[0], camera.position[1], camera.position[2]);
    glm::vec3 center(camera.target[0], camera.target[1], camera.target[2]);
    glm::vec3 upVec(camera.up[0], camera.up[1], camera.up[2]);

    camera.viewMatrix = glm::lookAt(eye, center, upVec);
    
    glm::mat4 proj = glm::perspective(glm::radians(camera.fov), camera.aspect, camera.nearPlane, camera.farPlane);
    proj[1][1] *= -1.0f; // Vulkan Y-flip on glm::mat4
    camera.projectionMatrix = convertToMatrix4(proj);

    return true;
}

void TransformLoader::applyTransform(SceneGraphNode& node) {
    node.modelMatrix = createModelMatrix(
        glm::vec3(node.position[0], node.position[1], node.position[2]),
        glm::vec3(node.rotation[0], node.rotation[1], node.rotation[2]),
        glm::vec3(node.scale[0], node.scale[1], node.scale[2])
    );
    node.valid = true;
}

Matrix4 TransformLoader::convertToMatrix4(const glm::mat4& glmmat) {
    Matrix4 result;
    const float* src = glm::value_ptr(glmmat);
    std::memcpy(result.data, src, 16 * sizeof(float));
    return result;
}

void TransformLoader::printTransformInfo(const SceneGraphNode& node) {
    std::cout << "Node: " << node.name 
              << " | Pos: (" << node.position[0] << ", " << node.position[1] << ", " << node.position[2] << ")\n";
}

std::vector<std::string> TransformLoader::splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

Matrix4 TransformLoader::multiplyMatrices(Matrix4 m1, Matrix4 m2) {
    glm::mat4 gm1 = m1;
    glm::mat4 gm2 = m2;
    return convertToMatrix4(gm1 * gm2);
}

Matrix4 TransformLoader::translateMatrix(float x, float y, float z) {
    return convertToMatrix4(glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z)));
}

Matrix4 TransformLoader::rotateXMatrix(float angleRadians) {
    return convertToMatrix4(glm::rotate(glm::mat4(1.0f), angleRadians, glm::vec3(1.0f, 0.0f, 0.0f)));
}

Matrix4 TransformLoader::rotateYMatrix(float angleRadians) {
    return convertToMatrix4(glm::rotate(glm::mat4(1.0f), angleRadians, glm::vec3(0.0f, 1.0f, 0.0f)));
}

Matrix4 TransformLoader::rotateZMatrix(float angleRadians) {
    return convertToMatrix4(glm::rotate(glm::mat4(1.0f), angleRadians, glm::vec3(0.0f, 0.0f, 1.0f)));
}

Matrix4 TransformLoader::scaleMatrix(float sx, float sy, float sz) {
    return convertToMatrix4(glm::scale(glm::mat4(1.0f), glm::vec3(sx, sy, sz)));
}

void TransformLoader::extractFloats(const std::string& line, std::vector<float>& out) {
    std::istringstream iss(line);
    float val;
    while (iss >> val) {
        out.push_back(val);
    }
}

glm::mat4 TransformLoader::createModelMatrix(const glm::vec3& position,
                                           const glm::vec3& rotation,
                                           const glm::vec3& scale) {
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);
    return model;
}

bool TransformLoader::validateModelMatrix(const Matrix4& matrix) {
    for (int i = 0; i < 16; ++i) {
        if (std::isnan(matrix.data[i]) || std::isinf(matrix.data[i])) {
            return false;
        }
    }
    return true;
}

} // namespace blitz
