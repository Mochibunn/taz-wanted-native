/* transforms.h - Scene graph, coordinate transforms, and MVP matrix calculations */

#ifndef TRANSFORMS_H
#define TRANSFORMS_H

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <map>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstring>
#include <memory>

namespace blitz {

// Simple 4x4 matrix structure (wrapper around GLM mat4 for Vulkan)
struct Matrix4 {
    float data[16];
    
    Matrix4() {
        memset(data, 0, sizeof(data));
        data[0] = 1.0f;
        data[5] = 1.0f;
        data[10] = 1.0f;
        data[15] = 1.0f;
    }
    
    Matrix4(const glm::mat4& other) {
        const float* src = glm::value_ptr(other);
        std::memcpy(data, src, 16 * sizeof(float));
    }
    
    static Matrix4 identity() { return Matrix4(); }
    
    operator glm::mat4() const {
        return glm::make_mat4(data);
    }
};

// Scene graph node structure for hierarchical transforms
struct SceneGraphNode {
    std::string name;
    float position[3];  // x, y, z in world space
    float rotation[3];  // Euler angles in degrees (x=pitch, y=yaw, z=roll)
    float scale[3];     // Scale factors (x, y, z)
    Matrix4 modelMatrix;
    std::vector<std::shared_ptr<SceneGraphNode>> children;
    bool valid = false;
};

// Object transform data from scene file
struct ObjectTransform {
    std::string objectName;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    Matrix4 modelMatrix;
};

// Camera frustum configuration for view/projection matrices
struct CameraFrustum {
    float position[3];
    float target[3];
    float up[3] = {0.0f, 1.0f, 0.0f};
    float fov = 60.0f;
    float aspect = 1.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    Matrix4 projectionMatrix;
    Matrix4 viewMatrix;
};

// Transform loader for parsing scene files and computing matrices
class TransformLoader {
public:
    TransformLoader();
    ~TransformLoader();
    
    bool loadSceneGraph(const std::string& filePath, std::vector<SceneGraphNode>& nodes);
    bool parseObjectTransforms(const std::string& filePath, std::vector<ObjectTransform>& transforms);
    Matrix4 computeMVP(const SceneGraphNode& node, const CameraFrustum& camera);
    
    bool setupCameraFrustum(CameraFrustum& camera,
                          const float* pos = nullptr,
                          const float* target = nullptr,
                          const float* up = nullptr,
                          float fov = 60.0f,
                          float aspect = 1.0f,
                          float nearPlane = 0.1f,
                          float farPlane = 1000.0f);
    
    void applyTransform(SceneGraphNode& node);
    static Matrix4 convertToMatrix4(const glm::mat4& glmmat);
    static void printTransformInfo(const SceneGraphNode& node);
    
private:
    std::vector<std::string> splitString(const std::string& str, char delimiter);
    
    static Matrix4 multiplyMatrices(Matrix4 m1, Matrix4 m2);
    static Matrix4 translateMatrix(float x, float y, float z);
    static Matrix4 rotateXMatrix(float angleRadians);
    static Matrix4 rotateYMatrix(float angleRadians);
    static Matrix4 rotateZMatrix(float angleRadians);
    static Matrix4 scaleMatrix(float sx, float sy, float sz);
    static void extractFloats(const std::string& line, std::vector<float>& out);
    
    static glm::mat4 createModelMatrix(const glm::vec3& position,
                                       const glm::vec3& rotation,
                                       const glm::vec3& scale);
    static bool validateModelMatrix(const Matrix4& matrix);
};

// Vulkan uniform buffer for transforms
struct TransformUniformsRaw {
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 modelMatrix;
};

static_assert(sizeof(TransformUniformsRaw) == sizeof(glm::mat4) * 3, "Size mismatch!");

} // namespace blitz

#endif // TRANSFORMS_H
