#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <cmath>
#include <random>
#include <limits>
#include <algorithm>
#include <ranges>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <array>


#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "aif/ImageFetcher.h"


// ==================== Namespace Aliases ====================
namespace fs = std::filesystem;

// ==================== Constants ====================
const std::string IMAGE_PROVIDER_URL = "https://thispersondoesnotexist.com";


using Image = aif::ImageFetcher::RawImage;
using Images = std::vector<Image>;


// ==================== Utility Functions ====================
namespace texgan::utils {

    // Dynamically resolve the root of the project from the location of this file.
    inline std::filesystem::path projectRoot() {
        constexpr const char* thisFile = __FILE__;
        std::filesystem::path filePath(thisFile);

        // Example: .../src/texgan/utils/PathUtils.h
        // Go up 4 levels: PathUtils.h → utils → texgan → src → ROOT
        return filePath.parent_path().parent_path();
    }

    // Path to any file in the project root
    inline std::string root(const std::string& relativePath) {
        return (projectRoot() / relativePath).string();
    }

    // Path to the assets folder
    inline std::string asset(const std::string& relativePath) {
        return (projectRoot() / "assets" / relativePath).string();
    }

    // Path to shaders specifically
    inline std::string shader(const std::string& shaderName) {
        return asset("shaders/" + shaderName);
    }

    // Path to any resource subfolder (e.g., "faces", "configs", etc.)
    inline std::string resource(const std::string& folder, const std::string& filename) {
        return (projectRoot() / folder / filename).string();
    }

    // Optional: source file access (useful for dev tools or introspection)
    inline std::string src(const std::string& relativePath) {
        return (projectRoot() / "src" / relativePath).string();
    }

}


// ==================== Core Systems ====================

namespace texgan::core{

    class Window {
        public:
        Window(int width, int height, const std::string& title, bool fullscreen=false): m_window(nullptr){
            if (!glfwInit()) {
                throw std::runtime_error("GLFW initialization failed!");
            }
    
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
            if(fullscreen){
                GLFWmonitor * monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode * mode =  glfwGetVideoMode(monitor);
                m_window = glfwCreateWindow(mode->width, mode->height, title.c_str(), monitor, nullptr);
            }else{
                m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
            }

            if (!m_window) {
                glfwTerminate();
                throw std::runtime_error("Failed to create GLFW window!");
            }
    
            glfwMakeContextCurrent(m_window);

            glfwSwapInterval(1); // Enable V-Sync
    
            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) {
                throw std::runtime_error("GLEW initialization failed!");
            }
    
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_PROGRAM_POINT_SIZE);
    
            
            std::cout << "[TexGAN] OpenGL context created successfully.\n";
        }
        ~Window() {
            if (m_window) {
                glfwDestroyWindow(m_window);
            }
            glfwTerminate();
        }
        
        void pollEvents() {
            glfwPollEvents();
            if(glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(m_window, true);
        }

        void swapBuffers(){
            glfwSwapBuffers(m_window);
        }

        bool shouldClose() const {
            return glfwWindowShouldClose(m_window);
        }
        
        void clear(float r, float g, float b, float a = 1.0f) const {
            glClearColor(r, g, b, a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        
        GLFWwindow* handle() const {
            return m_window;
        }
    
    private: 
        GLFWwindow* m_window;
    };
        
    class Camera {
    public:
        enum Movement {
            FORWARD,
            BACKWARD,
            LEFT,
            RIGHT,
            UP,
            DOWN
        };
    
        Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 20.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f)
        : front(glm::vec3(0.0f, 0.0f, -1.0f)),movementSpeed(10.0f), mouseSensitivity(0.1f), zoom(45.0f) {
            this->position = position;
            this->worldUp = up;
            this->yaw = yaw;
            this->pitch = pitch;
            updateCameraVectors();
        }
    
        glm::mat4 getViewMatrix() const {
            return glm::lookAt(position, position + front, up);
        }

        glm::mat4 getProjectionMatrix(float aspectRatio) const {
            bool isAspectRatioValid = aspectRatio > 0.0; // To avoid divide by zero resulting from minimizing window or switching to other windows
            return  isAspectRatioValid ? glm::perspective(glm::radians(zoom), aspectRatio, 0.1f, 5000.0f) : glm::mat4(1.0f);
        }
    
        void processKeyboard(Movement direction, float deltaTime){
            float velocity = movementSpeed * deltaTime;
            if (direction == FORWARD)
                position += front * velocity;
            if (direction == BACKWARD)
                position -= front * velocity;
            if (direction == LEFT)
                position -= right * velocity;
            if (direction == RIGHT)
                position += right * velocity;
            if (direction == UP)
                position += up * velocity;
            if (direction == DOWN)
                position -= up * velocity;
        }

        void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
            xoffset *= mouseSensitivity;
            yoffset *= mouseSensitivity;
        
            yaw += xoffset;
            pitch += yoffset;
        
            if (constrainPitch) {
                if (pitch > 89.0f)
                    pitch = 89.0f;
                if (pitch < -89.0f)
                    pitch = -89.0f;
            }
        
            updateCameraVectors();
        }
        
        void processMouseScroll(float yoffset){
            zoom -= yoffset;
            if (zoom < 1.0f)
                zoom = 1.0f;
            if (zoom > 90.0f)
                zoom = 90.0f;
        }
        
    
        // Camera attributes
        glm::vec3 position;
        glm::vec3 front;
        glm::vec3 up;
        glm::vec3 right;
        glm::vec3 worldUp;
        
        // Euler angles
        float yaw;
        float pitch;
        
        // Camera options
        float movementSpeed;
        float mouseSensitivity;
        float zoom;
    
    private:
        void updateCameraVectors(){
            glm::vec3 newFront;
            newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            newFront.y = sin(glm::radians(pitch));
            newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            front = glm::normalize(newFront);
            
            right = glm::normalize(glm::cross(front, worldUp));
            up = glm::normalize(glm::cross(right, front));
        }
    };

    class CameraController{
    public:
        CameraController(GLFWwindow * window, Camera& camera, bool disable_cursor = false): m_window(window), m_camera(camera), m_firstMouse(true), m_lastX(0), m_lastY(0), m_lastFrame(0.0f), m_deltaTime(0.0f){
            // Get initial mouse position
            double xpos, ypos;
            glfwGetCursorPos(m_window, &xpos, &ypos);
            m_lastX = static_cast<float>(xpos);
            m_lastY = static_cast<float>(ypos);

            // Set callbacks
            glfwSetWindowUserPointer(m_window, this);

            if(disable_cursor){
                glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }

            glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
                auto* controller = static_cast<CameraController*>(glfwGetWindowUserPointer(window));
                controller->processMouseMovement(static_cast<float>(xpos), static_cast<float>(ypos));
            });

            glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
                auto* controller = static_cast<CameraController*>(glfwGetWindowUserPointer(window));
                controller->processMouseScroll(static_cast<float>(yoffset));
            });
        }

        void update() {
            // Calculate delta time
            float currentFrame = static_cast<float>(glfwGetTime());
            m_deltaTime = currentFrame - m_lastFrame;
            m_lastFrame = currentFrame;

            // Process keyboard input
            processKeyboardInput();
        }

        void processKeyboardInput() {
            if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS)
                m_camera.processKeyboard(Camera::FORWARD, m_deltaTime);
            if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)
                m_camera.processKeyboard(Camera::BACKWARD, m_deltaTime);
            if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)
                m_camera.processKeyboard(Camera::LEFT, m_deltaTime);
            if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)
                m_camera.processKeyboard(Camera::RIGHT, m_deltaTime);
            if (glfwGetKey(m_window, GLFW_KEY_1) == GLFW_PRESS)
                m_camera.processKeyboard(Camera::UP, m_deltaTime);
            if (glfwGetKey(m_window, GLFW_KEY_2) == GLFW_PRESS)
                m_camera.processKeyboard(Camera::DOWN, m_deltaTime);
        }
        
        void processMouseMovement(float xpos, float ypos) {
            if(glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS){

                if (m_firstMouse) {
                    m_lastX = xpos;
                    m_lastY = ypos;
                    m_firstMouse = false;
                }
                
                float xoffset = xpos - m_lastX;
                float yoffset = m_lastY - ypos; // reversed since y-coordinates go from bottom to top
                
                m_lastX = xpos;
                m_lastY = ypos;
                
                m_camera.processMouseMovement(xoffset, yoffset);
            }
        }
    
        void processMouseScroll(float yoffset) {
            m_camera.processMouseScroll(yoffset);
        }

        float getDeltaTime() const { return m_deltaTime; }
        
        float getLastX() const { return m_lastX; }
        float getLastY() const { return m_lastY; }
    private:
        GLFWwindow* m_window;
        Camera& m_camera;
        bool m_firstMouse;
        float m_lastX;
        float m_lastY;
        float m_deltaTime;
        float m_lastFrame;
    };
}



// ==================== ECS ====================
namespace texgan::ecs{
    enum class RenderType{ Simple, Instanced };

    // Entity
    using Entity = uint32_t;
    inline Entity nextEntityId{0};
    constexpr texgan::ecs::Entity kInvalidEntity = std::numeric_limits<texgan::ecs::Entity>::max();


    inline Entity createEntity(){
        return nextEntityId++;
    }

    struct TransformComponent{
        glm::vec3 position{0.0F};
        glm::vec3 rotation{1.0F};
        glm::vec3 scale{1.0F};
        float angle{0.0};

        glm::mat4 getModelMatrix() const{
            glm::mat4 model(1.0F);
            model = glm::translate(model, position);
            model = glm::rotate(model, glm::radians(angle), rotation);
            model = glm::scale(model, scale);
            return model;
        }
    };
    
    class MeshComponent{
        unsigned int m_vertexArrayObjectId;
        unsigned int m_vertexBufferObjectId;
        unsigned int m_elementBufferObjectId;
        std::vector<unsigned int> m_instanceBufferObjectIds;

        int m_nextAttribLocation;
        // Number of components per vertex (sum of attribute sizes), NOT in bytes
        size_t m_componentsPerVertex;
        // Size in bytes of one component (e.g. sizeof(float))
        size_t m_componentSizeBytes;
        
        size_t m_attributeOffset;

        // Counts:
        size_t m_numVertices;
        size_t m_numIndices;
        size_t m_numInstances;

    public:

        MeshComponent(): m_vertexArrayObjectId(0),
            m_vertexBufferObjectId(0),
            m_elementBufferObjectId(0),
            m_nextAttribLocation(0),
            m_componentsPerVertex(0),
            m_componentSizeBytes(0),
            m_numVertices(0),
            m_numIndices(0),
            m_numInstances(0),
            m_attributeOffset(0){
            glGenVertexArrays(1, &m_vertexArrayObjectId);
        }

        
        ~MeshComponent() {
            return;
            if (m_vertexArrayObjectId) {
                glDeleteVertexArrays(1, &m_vertexArrayObjectId);
            }
            if (m_vertexBufferObjectId) {
                glDeleteBuffers(1, &m_vertexBufferObjectId);
            }
            if (m_elementBufferObjectId) {
                glDeleteBuffers(1, &m_elementBufferObjectId);
            }
            for (auto bufId : m_instanceBufferObjectIds) {
                glDeleteBuffers(1, &bufId);
            }
        }


        void setVertexData(const std::vector<float>& vertices, size_t componentsPerVertex) {
            if(vertices.empty()) return;

            m_componentSizeBytes = sizeof(vertices[0]);

            bind();
            
            if(m_vertexBufferObjectId){
                glDeleteBuffers(1, &m_vertexBufferObjectId);
            }
            
            glGenBuffers(1, &m_vertexBufferObjectId);
            glBindBuffer(GL_ARRAY_BUFFER, m_vertexBufferObjectId);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * m_componentSizeBytes, vertices.data(), GL_STATIC_DRAW);
            
            
            m_componentsPerVertex = componentsPerVertex;
            m_numVertices = componentsPerVertex > 0 ? vertices.size() / m_componentsPerVertex : 0;
            m_nextAttribLocation = 0;
            m_attributeOffset = 0;

            unbind();
        }

        void setIndexData(const std::vector<unsigned int>& indices) {
            bind();
            if(m_elementBufferObjectId){
                glDeleteBuffers(1, &m_elementBufferObjectId);
            }


            glGenBuffers(1, &m_elementBufferObjectId);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementBufferObjectId);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);
            
            m_numIndices = indices.size();
            unbind();
        }


        void addAttribute(int size, bool normalized = false, GLenum type = GL_FLOAT) {
            bind();
            glBindBuffer(GL_ARRAY_BUFFER, m_vertexBufferObjectId);
            glEnableVertexAttribArray(m_nextAttribLocation);

            const void* offsetPtr = (void*)(m_attributeOffset * m_componentSizeBytes);
            GLsizei strideBytes = static_cast<GLsizei>(m_componentsPerVertex * m_componentSizeBytes);

            glVertexAttribPointer(m_nextAttribLocation, size, type, normalized, strideBytes, offsetPtr);
            

            // Advance offset by this attribute’s component count
            m_attributeOffset += size;
            m_nextAttribLocation++;
            
            unbind();
        }

            
        template<typename T>
        void updateInstanceAttribute(int attribIndex, const std::vector<T>& instanceData) {
            if (attribIndex < 0 || attribIndex >= m_instanceBufferObjectIds.size()) return;
            
            bind();
            glBindBuffer(GL_ARRAY_BUFFER, m_instanceBufferObjectIds[attribIndex]);
            glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(T), instanceData.data(), GL_STATIC_DRAW);
            unbind();
        }

        template<typename T>
        void addInstanceAttribute(const std::vector<T>& instanceData, int size, bool normalized = false, GLenum type = GL_FLOAT) {
            bind();
            
            unsigned int instanceVBO;
            glGenBuffers(1, &instanceVBO);
            m_instanceBufferObjectIds.push_back(instanceVBO);
            
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
            glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(T), instanceData.data(), GL_STATIC_DRAW);
            
            glEnableVertexAttribArray(m_nextAttribLocation);
            glVertexAttribPointer(m_nextAttribLocation, size, type, normalized, size * sizeof(T), (void*)0);
            glVertexAttribDivisor(m_nextAttribLocation, 1);
            m_nextAttribLocation++;
            
            m_numInstances = instanceData.size();
            unbind();
        }

        void addInstanceMatrixAttribute(const std::vector<glm::mat4>& instanceMatrices) {
            bind();
            
            unsigned int instanceVBO;
            glGenBuffers(1, &instanceVBO);
            m_instanceBufferObjectIds.push_back(instanceVBO);
            
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
            glBufferData(GL_ARRAY_BUFFER, instanceMatrices.size() * sizeof(glm::mat4), instanceMatrices.data(), GL_STATIC_DRAW);
            
            for (int i = 0; i < 4; i++) {
                glEnableVertexAttribArray(m_nextAttribLocation + i);
                glVertexAttribPointer(m_nextAttribLocation + i, 4, GL_FLOAT, GL_FALSE, 
                                    sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
                glVertexAttribDivisor(m_nextAttribLocation + i, 1);
            }
            
            m_nextAttribLocation += 4;
            m_numInstances = instanceMatrices.size();
            unbind();
        }


        void bind() {
            glBindVertexArray(m_vertexArrayObjectId);
        }

        void unbind() {
            glBindVertexArray(0);
        }


        bool usesEBO() const { return m_elementBufferObjectId != 0; }

        size_t getVertexCount() const { return m_numVertices; }

        size_t getIndexCount() const { return m_numIndices; }
        
        size_t getInstanceCount() const { return m_numInstances; }

    };
    
    struct TextureComponent{
        GLuint textureId{};

        ~TextureComponent() {
            // if (textureId) glDeleteTextures(1, &textureId);
        }
    };

    struct RenderComponent{
        RenderType type{RenderType::Simple};
        GLuint materialId;
        GLenum primitive{GL_TRIANGLES};
        uint32_t layer{0};
    };

    // World ------------------------------------------------------------------
    class World {
    public:
        /* ───────────── entity creation / destruction ───────────── */
        Entity createEntity() {
            Entity e = createEntityId();
            m_entities.push_back(e);
            return e;
        }

        void clear(){
            // Clear all component maps first
            m_transforms.clear();
            m_meshes.clear();
            m_textures.clear();
            m_renderComponents.clear();
            
            // Then clear the entities vector
            m_entities.clear();
        }

        void destroyEntity(Entity e) {
            m_transforms.erase(e);
            m_meshes.erase(e);          // shared_ptr handles GL cleanup
            m_textures.erase(e);
            m_renderComponents.erase(e);
            m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), e), m_entities.end());
        }

        /* ───────────── component adders ───────────── */
        TransformComponent& addTransform(Entity e,
                                         const TransformComponent& t = {}) {
            return m_transforms[e] = t;
        }

        TextureComponent& addTexture(Entity e,
                                     const TextureComponent& tex = {}) {
            return m_textures[e] = tex;
        }

        /** Give the entity a mesh.
         *  Pass a shared_ptr you created with `std::make_shared<MeshComponent>()`.
         *  Several entities can share the same mesh safely.
         */
        std::shared_ptr<MeshComponent>&
        addMesh(Entity e, std::shared_ptr<MeshComponent> mesh) {
            return m_meshes[e] = std::move(mesh);
        }

        RenderComponent& addRenderComponent(Entity e,
                                            const RenderComponent& r = {}) {
            return m_renderComponents[e] = r;
        }

        /* ───────────── component getters ───────────── */
        TransformComponent* getTransform(Entity e) {
            auto it = m_transforms.find(e);
            return it != m_transforms.end() ? &it->second : nullptr;
        }

        RenderComponent* getRenderComponent(Entity e) {
            auto it = m_renderComponents.find(e);
            return it != m_renderComponents.end() ? &it->second : nullptr;
        }

        /** Returns the raw pointer held by the shared_ptr (or nullptr). */
        MeshComponent* getMesh(Entity e) {
            auto it = m_meshes.find(e);
            return it != m_meshes.end() ? it->second.get() : nullptr;
        }

        TextureComponent* getTexture(Entity e) {
            auto it = m_textures.find(e);
            return it != m_textures.end() ? &it->second : nullptr;
        }

        /* ───────────── misc ───────────── */
        const std::vector<Entity>& getEntities() const { return m_entities; }

    private:
        /* helper */  Entity createEntityId() { return texgan::ecs::createEntity(); }

        /* storage */
        std::vector<Entity>                                   m_entities;
        std::unordered_map<Entity, TransformComponent>        m_transforms;
        std::unordered_map<Entity, TextureComponent>          m_textures;
        std::unordered_map<Entity, RenderComponent>           m_renderComponents;
        std::unordered_map<Entity, std::shared_ptr<MeshComponent>> m_meshes;
    };

};


// ==================== Rendering ====================
namespace texgan::rendering{
    
    class ShaderProgram {
    public:
        ShaderProgram(): m_programID(0){}

        ~ShaderProgram(){
            if (m_programID) {
                glDeleteProgram(m_programID);
            }
        }
        
        void loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
            std::string vSrc = readFile(vertexPath);
            std::string fSrc = readFile(fragmentPath);
    
            GLuint vShader = compileShader(vSrc, GL_VERTEX_SHADER);
            GLuint fShader = compileShader(fSrc, GL_FRAGMENT_SHADER);
    
            m_programID = glCreateProgram();
            glAttachShader(m_programID, vShader);
            glAttachShader(m_programID, fShader);
            glLinkProgram(m_programID);
            
            validate();

            glDeleteShader(vShader);
            glDeleteShader(fShader);
    
        }
        void loadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc){
            GLuint vShader = compileShader(vertexSrc, GL_VERTEX_SHADER);
            GLuint fShader = compileShader(fragmentSrc, GL_FRAGMENT_SHADER);
        
            m_programID = glCreateProgram();
            glAttachShader(m_programID, vShader);
            glAttachShader(m_programID, fShader);
            glLinkProgram(m_programID);
        
            validate();
        
            glDeleteShader(vShader);
            glDeleteShader(fShader);
        }

        void use() const { glUseProgram(m_programID); }
    
        static void unuse(){   glUseProgram(0); }
        
        GLuint getId() const { return m_programID; }

        // Uniform setters
        void setMat4(const std::string& name, const glm::mat4& matrix) const {
            GLint location = getUniformLocation(name);
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
        }

        void setVec3(const std::string& name, const glm::vec3& vec) const {
            GLint location = getUniformLocation(name);
            glUniform3fv(location, 1, glm::value_ptr(vec));
        }

        void setVec2(const std::string& name, const glm::vec2& vec) const {
            GLint location = getUniformLocation(name);
            glUniform2fv(location, 1, glm::value_ptr(vec));
        }

        void setInt(const std::string& name, int value) const {
            GLint location = getUniformLocation(name);
            glUniform1i(location, value);
        }

        void setFloat(const std::string& name, float value) const {
            GLint location = getUniformLocation(name);
            glUniform1f(location, value);
        }

    private:
        void validate() const {
            glValidateProgram(m_programID);
            GLint status;
            glGetProgramiv(m_programID, GL_VALIDATE_STATUS, &status);
            if (status != GL_TRUE) {
                char infoLog[512];
                glGetProgramInfoLog(m_programID, 512, nullptr, infoLog);
                throw std::runtime_error("Shader validation failed:\n" + std::string(infoLog));
            }
        }
        static std::string readFile(const std::string& path) {
            std::ifstream file(path);
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
    
        static GLuint compileShader(const std::string& source, GLenum type){
            GLuint shader = glCreateShader(type);
            const char* src = source.c_str();
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);
    
            GLint success;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                char info[512];
                glGetShaderInfoLog(shader, 512, nullptr, info);
                throw std::runtime_error("Shader compilation failed:\n" + std::string(info));
            }
            return shader;
        }

        GLint getUniformLocation(const std::string& name) const { return glGetUniformLocation(m_programID, name.c_str());   }
    private:
        GLuint m_programID;
    };
         
    class IRenderStrategy{
    public:
        virtual ~IRenderStrategy() = default;
        virtual void render(const std::vector<ecs::Entity>& entities, ecs::World& world) = 0;
    };

    class SimpleRenderer: public IRenderStrategy{
    public:
        explicit SimpleRenderer(const ShaderProgram& shader): m_shader(shader){}

        void render(const std::vector<ecs::Entity>& entities, ecs::World& world) override {

            m_shader.use();
            for (auto entity : entities) {
                auto* transform = world.getTransform(entity);
                auto* mesh = world.getMesh(entity);
                auto* texture = world.getTexture(entity);
                auto* render = world.getRenderComponent(entity);

                
                if (texture && texture->textureId > 0) {
                    m_shader.setInt("useTexture", 1);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, texture->textureId);
                }else{
                    m_shader.setInt("useTexture", 0);
                }

                if(transform){
                    m_shader.setMat4("model", transform->getModelMatrix());
                }



                if(mesh && render){
                    mesh->bind();
                    if(mesh->usesEBO()){
                        glDrawElements(render->primitive, mesh->getIndexCount(), GL_UNSIGNED_INT, 0);
                    }else{
                        glDrawArrays(render->primitive, 0, mesh->getVertexCount());
                    }
                    mesh->unbind();
                }
            }
            m_shader.unuse();
        }
    private:
        ShaderProgram m_shader;
    };
    
    class InstancedRenderer: public IRenderStrategy{
        public:
        explicit InstancedRenderer(const ShaderProgram& shader): m_shader(shader){

        }

        ~InstancedRenderer(){

        }

        void render(const std::vector<ecs::Entity>& entities, ecs::World& world) override {
            if(entities.empty()) return;
            m_shader.use();  
            m_shader.setInt("useInstancing", 1);

            

            for(auto entity: entities){
                auto * mesh = world.getMesh(entity);
                auto * transform = world.getTransform(entity);
                auto * render = world.getRenderComponent(entity);
                auto * texture = world.getTexture(entity);



                if(transform){
                    glm::mat4 model = transform->getModelMatrix();
                    m_shader.setMat4("model", model);
                }


                if (texture && texture->textureId > 0) {
                    m_shader.setInt("useTexture", 1);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, texture->textureId);
                }else{
                    m_shader.setInt("useTexture", 0);
                }

                if(mesh && render){
                    
                    
                    mesh->bind();
                    if (mesh->usesEBO()) {
                        glDrawElementsInstanced(render->primitive, mesh->getIndexCount(), GL_UNSIGNED_INT, 0, mesh->getInstanceCount());
                    } else {
                        glDrawArraysInstanced(render->primitive, 0, mesh->getVertexCount(), mesh->getInstanceCount());
                    }
                    mesh->unbind();
                }
            }
            
            m_shader.setInt("useInstancing", 0);
            m_shader.unuse();    
        }

    private:
        ShaderProgram m_shader;
        GLuint m_instanceVBO;
    };

    class Renderer{
    public:
        explicit Renderer(core::Window & window): m_window{window}{
           
            m_defaultShader.loadFromFiles(texgan::utils::shader("shader.vert"), texgan::utils::shader("shader.frag"));

            m_strategies[ecs::RenderType::Simple] = std::make_unique<SimpleRenderer>(m_defaultShader);
            m_strategies[ecs::RenderType::Instanced] = std::make_unique<InstancedRenderer>(m_defaultShader);
        }

        void render(ecs::World & world){
            std::unordered_map<ecs::RenderType, std::vector<ecs::Entity>> renderGroups;

            for (auto entity : world.getEntities()) {
                if (auto* render = world.getRenderComponent(entity)) {
                    renderGroups[render->type].push_back(entity);
                }
            }

            // update each strategy
            for (auto& [type, entities] : renderGroups) {
                m_strategies[type]->render(entities, world);
            }
        }
        ShaderProgram m_defaultShader;
    private:
        core::Window& m_window;
        std::unordered_map<ecs::RenderType, std::unique_ptr<IRenderStrategy>> m_strategies;
    };
};



// ==================== Performance Monitoring ====================
namespace texgan::monitoring{
    static std::string makeCsvName(const std::string& approach, int textureCount) {
        // get now as time_t
        auto now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        // format as YYYYMMDD_HHMMSS
        std::tm tm;
    #if defined(_WIN32)
        localtime_s(&tm, &now_c);
    #else
        localtime_r(&now_c, &tm);
    #endif
        std::ostringstream ss;
        ss << "fps_"
        << approach
        << "_"
        << textureCount
        << "tex_"
        << std::put_time(&tm, "%Y%m%dT%H%M%S")
        << ".csv";
        return ss.str();
    }

    class FPSLogger {
    public:
        FPSLogger(const std::string& approach, int textureCount)
        : frameCount(0),
            lastTime(std::chrono::steady_clock::now()),
            startTime(std::chrono::steady_clock::now())
        {
            auto name = makeCsvName(approach, textureCount);
            csv.open(texgan::utils::asset("log/" + name), std::ios::out);
            if (!csv) {
                std::cerr << "Failed to open " << name << " for writing\n";
            } else {
                csv << "Time(s),FPS\n";
            }
        }

        ~FPSLogger() {
            if (csv.is_open()) csv.close();
        }

        void frameTick() {
            ++frameCount;
            auto now = std::chrono::steady_clock::now();
            auto elapsed = now - lastTime;
            if (elapsed >= std::chrono::seconds(1)) {
                double secSinceStart =
                std::chrono::duration<double>(now - startTime).count();
                int fps = frameCount;
                std::cout << "Time: " << std::fixed << std::setprecision(1)
                        << secSinceStart << "s, FPS: " << fps << "\n";
                if (csv) {
                    csv << secSinceStart << "," << fps << "\n";
                }
                frameCount = 0;
                lastTime = now;
            }
        }

    private:
        std::ofstream csv;
        int frameCount;
        std::chrono::steady_clock::time_point lastTime, startTime;
    };
}



// ==================== [TEXTURE LOADING] ====================
namespace texgan::loading{
    // A struct to hold decoded image data
    struct DecodedImage {
        Image pixels;
        int width;
        int height;
        int channels;
    };

    unsigned int loadTextureFromFile(const char* path){
        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrChannels;
        unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
        
        if (data) 
        {
            GLenum format;
            if (nrChannels == 1)
                format = GL_RED;
            else if (nrChannels == 3)
                format = GL_RGB;
            else if (nrChannels == 4)
                format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            // Set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(data);
        }
        else 
        {
            std::cerr << "Failed to load texture at path: " << path << std::endl;
            stbi_image_free(data);
        }

        return textureID;
    }

    // 1. Decode function (can be called in a worker thread)
    bool decodeImageToMemory(const Image& image, DecodedImage& out) {
        stbi_set_flip_vertically_on_load(true);
        int w, h, c;
        unsigned char* data = stbi_load_from_memory(image.data(), image.size(), &w, &h, &c, 0);
        if (!data) return false;
        size_t size = static_cast<size_t>(w) * h * c;
        out.pixels.assign(data, data + size);
        out.width = w;
        out.height = h;
        out.channels = c;
        stbi_image_free(data);
        return true;
    }

    // 2. Generate GL texture (must run on main thread with valid context)
    GLuint generateGLTexture(const DecodedImage& img) {
        if (img.pixels.empty()) return 0;
        GLenum format = GL_RGB;
        if (img.channels == 1) format = GL_RED;
        else if (img.channels == 3) format = GL_RGB;
        else if (img.channels == 4) format = GL_RGBA;

        GLuint textureID = 0;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        // set parameters as desired
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // upload
        glTexImage2D(GL_TEXTURE_2D, 0, format, img.width, img.height, 0, format, GL_UNSIGNED_BYTE, img.pixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return textureID;
    }


    struct TextureLoader{
        virtual ~TextureLoader() = default;
        /// Clean up any GL resources or windows
        virtual void cleanup() = 0;
        /// Called on the main thread each frame to upload pending textures and assign them to entities
        virtual void update(texgan::ecs::World& world) = 0;
        /// Called from the image-fetch callback (possibly worker thread) to enqueue or immediately upload
        virtual void processImages(bool success, const Images& images) = 0;
    };

    class SingleContextUploadApproach : public TextureLoader{
    public:
        SingleContextUploadApproach() = default;
        ~SingleContextUploadApproach() override {
            cleanup();
        }

        
        virtual void cleanup() override {
            std::lock_guard<std::mutex> lock(mutex);
            for(GLuint texture: textures) glDeleteTextures(1, &texture);
            textures.clear();
        }

        /// Called on the main thread each frame to upload pending textures and assign them to entities
        virtual void update(texgan::ecs::World& world) override {
            std::deque<DecodedImage> localQueue;

            // Safely move pendingImageQueue to local queue. which means pendingImageQueue is now empty
            {
                std::lock_guard<std::mutex> lock(mutex);
                if(imageQueue.empty()) return;
                localQueue.swap(imageQueue);
            }

            for(const DecodedImage& decodedImage: localQueue){
                GLuint textureID = generateGLTexture(decodedImage);
                if(textureID) textures.push_back(textureID);
            }

            assignTextures(world);
        }

        /// Called from the image-fetch callback (possibly worker thread) to enqueue or immediately upload
        virtual void processImages(bool success, const Images& images) override {
            if(!success) return;

            for(const auto& image: images){

                DecodedImage decodedImage;
                if(!decodeImageToMemory(image, decodedImage)) continue; // failure

                // Safely push to a shared queue
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    imageQueue.push_back(std::move(decodedImage));
                }
            }
        }

    private:
        std::mutex mutex;
        std::deque<DecodedImage> imageQueue;
        std::vector<GLuint> textures;


        void assignTextures(texgan::ecs::World& world) {
            const auto& ents = world.getEntities();
            std::lock_guard<std::mutex> lock(mutex);
            size_t count = std::min(textures.size(), ents.size());
            for (size_t i = 0; i < count; ++i) {
                if (auto* tc = world.getTexture(ents[i])) {
                    tc->textureId = textures[i];
                }
            }
        }
    };

    class SharedContextUploadApproach : public TextureLoader{
    public:
        SharedContextUploadApproach() = default;
        ~SharedContextUploadApproach() override {
            cleanup();
        }


        void initSharedContext(GLFWwindow* mainWindow) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_sharedContextWindow) {
                // already initialized
                return;
            }
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            m_sharedContextWindow = glfwCreateWindow(1, 1, "Shared Context", nullptr, mainWindow);
            if (!m_sharedContextWindow) {
                throw std::runtime_error("Failed to create shared GLFW context");
            }

            m_mainContextWindow = mainWindow;
        }

        
        /// Clean up any GL resources or windows
        virtual void cleanup() override {
            std::lock_guard<std::mutex> lock(m_mutex);
            if(m_sharedContextWindow){
                glfwDestroyWindow(m_sharedContextWindow);
                m_sharedContextWindow = nullptr;
            }

            for(GLuint texture: m_textures) glDeleteTextures(1, &texture);

            m_textures.clear();
        }


        /// Called on the main thread each frame to upload pending textures and assign them to entities
        virtual void update(texgan::ecs::World& world) override {
            std::lock_guard<std::mutex> lock(m_mutex);

            const auto& ents = world.getEntities();
            size_t count = std::min(m_textures.size(), ents.size());
            for (size_t i = 0; i < count; ++i) {
                if (auto* tc = world.getTexture(ents[i])) {
                    tc->textureId = m_textures[i];
                }
            }
        }


        /// Called from the image-fetch callback (possibly worker thread) to enqueue or immediately upload
        virtual void processImages(bool success, const Images& images) override {
            if (!success) return;
            std::lock_guard<std::mutex> lock(m_mutex);

            if (!m_sharedContextWindow || !m_mainContextWindow){
                std::cout << "shared context not initialized!\n";
                return;
            }


            // Make shared context current
            glfwMakeContextCurrent(m_sharedContextWindow);
            for (const auto& image : images) {
                DecodedImage decodedImage;
                if (!decodeImageToMemory(image, decodedImage)) {
                    continue;
                }
                GLuint textureID = generateGLTexture(decodedImage);
                if (textureID) {
                    m_textures.push_back(textureID);
                }
            }
            // Restore main context
            glfwMakeContextCurrent(m_mainContextWindow);
        }
    private:
        std::mutex m_mutex;
        GLFWwindow* m_sharedContextWindow = nullptr;
        GLFWwindow* m_mainContextWindow = nullptr;
        std::vector<GLuint> m_textures;
    };

}


// ==================== Helper Functions ====================
namespace texgan::helpers{
    std::vector<glm::vec3> makeGrid(const int gridWidth = 15, const int gridDepth = 8, const float padding = 1.0f, const float cubeSize = 1.0f){
        std::vector<glm::vec3> positions;
        for (int x = 0; x < gridWidth; x++) {
            for (int y = 0; y < gridDepth; y++) {
                // Calculate position with padding
                float xPos = (x - gridWidth/2.0f) * (cubeSize + padding);
                float yPos = (y - gridDepth/2.0f) * (cubeSize + padding);
                float zPos = 0.0f;
                positions.push_back(glm::vec3(xPos, yPos, zPos));
            }
        }

        return positions;
    };

    std::shared_ptr<texgan::ecs::MeshComponent>  makeCubes(const std::vector<glm::vec3>& instancePositions={}){

        // Create mesh component
        auto mesh = std::make_shared<texgan::ecs::MeshComponent>();

        std::vector<float> vertices = {
            // positions          // normals           // texture coords
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
            0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
            0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
            0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,

            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
            0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,

            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

            0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
            0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
            0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,
            0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
            0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,

            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
            0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f
        };

        mesh->setVertexData(vertices, 8);

        
        // Regular attributes
        mesh->addAttribute(3); // position
        mesh->addAttribute(3); // normal
        mesh->addAttribute(2); // texCoord


        // instanced
        if(!instancePositions.empty()){
            size_t instanceCount = instancePositions.size();
            // Create instance matrices
            std::vector<glm::mat4> instanceMatrices;
            instanceMatrices.reserve(instanceCount);
            
            std::ranges::transform(instancePositions | std::views::take(instanceCount), std::back_inserter(instanceMatrices),[](const auto& pos) { return glm::translate(glm::mat4{1.0f}, pos); });
            
            mesh->addInstanceMatrixAttribute(instanceMatrices);
        }

        return mesh;
    }


    std::vector<glm::vec3> generateRandomColors(size_t count) {
        std::vector<glm::vec3> colors(count);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> colorDist(0.1f, 1.0f);
        
        for (size_t i = 0; i < count; i++) {
            colors[i] = glm::vec3(
                colorDist(gen),
                colorDist(gen),
                colorDist(gen)
            );
        }
        
        return colors;
    }

    std::vector<float> generateRandomFloatNumbers(size_t count, float min, float max){
        std::vector<float> numbers(count);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> posDist(min, max);
        
        for (size_t i = 0; i < count; i++) {
            numbers.push_back(posDist(gen));
        }
        
        return numbers;
    }

    std::vector<glm::vec2> generateRandom2dPositions(size_t count) {
        std::vector<glm::vec2> positions(count);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> posDist(-0.9f, 0.9f);
        
        for (size_t i = 0; i < count; i++) {
            positions[i] = glm::vec2(
                posDist(gen),
                posDist(gen)
            );
        }
        
        return positions;
    }

    std::vector<glm::vec3> generateRandom3dPositions(size_t count, float min, float max) {
        std::vector<glm::vec3> positions(count);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> posDist(min, max);
        
        for (size_t i = 0; i < count; i++) {
            positions[i] = glm::vec3(
                posDist(gen),
                posDist(gen),
                posDist(gen)
            );
        }
        
        return positions;
    }

}

// ==================== UI ====================
namespace texgan::ui{
    struct WindowStyle{
        ImVec2 position, size;
        struct{
            float borderSize = 0.0f;
            float rounding = 0.0f;
            ImVec2 padding = {10.0f, 10.0f};
            ImVec2 titleAlign = {0.5f, 0.5f}; 
            ImVec4 textColor = ImVec4(0.30f, 0.25f, 0.20f, 1.0f);
            ImVec4 backgroundColor = ImVec4(0.96f, 0.96f, 0.94f, 1.0f);
            ImVec4 titleTextColor = ImVec4(0.98f, 0.97f, 0.93f, 1.0f);
            ImVec4 titleBackgroundColor = ImVec4(0.0f, 0.6f, 1.0f, 1.0f);
            int flags = 0;
        } window;

        struct frame{
            float rounding = 8.0f;
            float borderSize = 1.0f;
            ImVec2 padding = {7.0f, 7.0f};
            ImVec4 backgroundColor = ImVec4(0.95f, 0.96f, 0.96f, 1.0f);
        } frame;
        
        struct{
            ImVec2 itemSpacing = {10.0f, 10.0f};
            ImVec2 itemInnerSpacing = {10.0f, 10.0f};
        } spacing;

        struct {
            ImVec2 textAlign = {0.5f, 0.5f};  // Centered text
            ImVec4 color = {0.1f, 0.2f, 0.15f, 1.0f};
            float thickness = 1.0f;
        } separator;
        
        
        bool visible = true;

        static void applyStyles(const WindowStyle& style) {
            // Window styles
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, style.window.borderSize);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, style.window.rounding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.window.padding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, style.window.titleAlign);
            
            // Frame styles
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, style.frame.borderSize);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, style.frame.rounding);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.frame.padding);
            
            // Spacing
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.spacing.itemSpacing);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, style.spacing.itemInnerSpacing);
            
            // Separator
            ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, style.separator.textAlign);
            
            // Colors
            ImGui::PushStyleColor(ImGuiCol_WindowBg, style.window.backgroundColor);
            ImGui::PushStyleColor(ImGuiCol_TitleBg, style.window.titleBackgroundColor);
            ImGui::PushStyleColor(ImGuiCol_TitleBgActive, style.window.titleBackgroundColor);
            ImGui::PushStyleColor(ImGuiCol_Text, style.window.textColor);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, style.frame.backgroundColor);
            ImGui::PushStyleColor(ImGuiCol_Separator, style.separator.color);
        }

        // Static method to reset styles
        static void resetStyles() {
            // Pop all style vars 
            ImGui::PopStyleVar(10);
            
            // Pop all style colors 
            ImGui::PopStyleColor(6);
        }
    };

    class TextureLoaderUI {  
    public:
        TextureLoaderUI(const texgan::core::Window& window, texgan::ecs::World& world, texgan::core::Camera& camera): 
        m_window(window.handle()), m_world(world), m_camera(camera), m_activeCube(texgan::ecs::kInvalidEntity){
            // Init ImGui for GLFW + OpenGL3
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            ImGui::StyleColorsDark();
            ImGui_ImplGlfw_InitForOpenGL(m_window, true);
            ImGui_ImplOpenGL3_Init("#version 330");

            #ifdef NDEBUG
                io.IniFilename = nullptr; // disable for production
            #endif

            // Default to single-context approach
            useSingleContextApproach = true;
            m_singleUploader = std::make_unique<texgan::loading::SingleContextUploadApproach>();
            // Prepare shared context approach ahead of time
            m_sharedUploader = std::make_unique<texgan::loading::SharedContextUploadApproach>();
            m_sharedUploader->initSharedContext(m_window);

            float fontSize = 18.0f;

            io.Fonts->AddFontFromFileTTF(texgan::utils::asset("fonts/mont/MontserratBold-DOWZd.ttf").c_str(), 35.0f);
            io.Fonts->AddFontFromFileTTF(texgan::utils::asset("fonts/mont/MontserratBold-DOWZd.ttf").c_str(), 24.0f);
            io.Fonts->AddFontFromFileTTF(texgan::utils::asset("fonts/mont/MontserratBold-DOWZd.ttf").c_str(), 20.0f);
            io.Fonts->AddFontFromFileTTF(texgan::utils::asset("fonts/Roboto/Roboto-Italic-VariableFont_wdth,wght.ttf").c_str(), fontSize);
            io.Fonts->AddFontFromFileTTF(texgan::utils::asset("fonts/Zilla_Slab_Highlight/ZillaSlabHighlight-Bold.ttf").c_str(), fontSize);
            io.Fonts->AddFontFromFileTTF(texgan::utils::asset("fonts/Zilla_Slab_Highlight/ZillaSlabHighlight-Regular.ttf").c_str(), fontSize);
            io.Fonts->AddFontFromFileTTF(texgan::utils::asset("fonts/Rubik_Doodle_Triangles/RubikDoodleTriangles-Regular.ttf").c_str(), fontSize);
            io.Fonts->AddFontFromFileTTF(texgan::utils::asset("fonts/Orbitron/Orbitron-VariableFont_wght.ttf").c_str(), fontSize);
            io.Fonts->AddFontFromFileTTF(texgan::utils::asset("fonts/swan/Swansea-q3pd.ttf").c_str(), 6.0f);
            io.FontDefault = io.Fonts->AddFontFromFileTTF(texgan::utils::asset("fonts/Nova_Round/NovaRound-Regular.ttf").c_str(), fontSize);
        
            io.Fonts->AddFontDefault(); // Fonts[0] - default font


            myImage = texgan::loading::loadTextureFromFile(texgan::utils::asset("images/salih3.png").c_str());
        }

        ~TextureLoaderUI() {
            // Cleanup uploader resources
            if (m_singleUploader) {
                m_singleUploader->cleanup();
            }
            if (m_sharedUploader) {
                m_sharedUploader->cleanup();
            }
            // ImGui shutdown
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }

        void showInfoWindow(GLuint textureId, const WindowStyle& ws) {
            WindowStyle::applyStyles(ws);
            
            ImGui::SetNextWindowPos(ws.position);
            ImGui::SetNextWindowSize(ws.size);

            if (ImGui::Begin("InfoPanel", nullptr, ws.window.flags))
            {
                // Create columns for image + text
                ImGui::Columns(2, "info_columns", false);
                ImGui::SetColumnWidth(0, ws.size.y); 
                
                // Image display (left column)
                if (textureId != 0) {
                    int imageWidth = 0.8f * ws.size.y;
                    int imageHeight = 0.8f * ws.size.y;
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.0); // Add vertical padding
                    ImGui::Image((ImTextureID)(intptr_t)textureId, ImVec2(imageWidth, imageHeight));
                }
                

                // Text content (right column)
                ImGui::NextColumn();
                
                // University info with styling
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.99f, 1.0f, 1.0f));
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                ImGui::Text("Karadeniz Technical University");
                ImGui::PopFont();
                ImGui::PopStyleColor();
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.8f, 1.0f, 1.0f));
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGui::Text("Of faculty of Technology");
                ImGui::PopFont();
                ImGui::PopStyleColor();
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 1.0f, 1.0f));
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGui::Text("Software Engineering");
                ImGui::PopFont();
                ImGui::PopStyleColor();
                
                ImGui::Spacing();
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.2f, 1.0f));
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGui::Text("Mohammed Jabbar Salih SALIH");
                ImGui::PopFont();
                ImGui::PopStyleColor();
                
                ImGui::SameLine();
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 1.0f, 1.0f));
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGui::Text("under supervision of");
                ImGui::PopFont();
                ImGui::PopStyleColor();
                
                ImGui::SameLine();
                

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.2f, 1.0f));
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGui::Text("Asst. Prof. Sefa ARAS");
                ImGui::PopFont();
                ImGui::PopStyleColor();
                
                
                ImGui::Columns(1); // Reset columns
            }
            ImGui::End();

            WindowStyle::resetStyles();
        }

        void showTextureLoaderControls(const WindowStyle& ws) {
            WindowStyle::applyStyles(ws);

            ImGui::SetNextWindowPos(ws.position);
            ImGui::SetNextWindowSize(ws.size);

            ImGui::PushStyleColor(ImGuiCol_Text, ws.window.titleTextColor);
            ImGui::Begin("Texture Loader Controls", nullptr, ws.window.flags);
            ImGui::PopStyleColor();

            // Batch size control
            static int count = 5;
            ImGui::SetNextItemWidth(ws.size.x / 2);
            ImGui::SliderInt("Texture Batch Size", &count, 1, 500, "%d textures");
            ImGui::Spacing();

            // Context mode selector using radio buttons
            ImGui::Text("Loading Mode:");
            ImGui::Spacing();
            
            // Radio button group container
            ImGui::BeginGroup();
            {
                // Custom styling for radio buttons
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 5));
                
                // First radio button - Single Context
                if (ImGui::RadioButton("Single Context", useSingleContextApproach)) {
                    useSingleContextApproach = true;
                }
                
                // Second radio button - Shared Context
                ImGui::SameLine();
                if (ImGui::RadioButton("Shared Context", !useSingleContextApproach)) {
                    useSingleContextApproach = false;
                }
                
                ImGui::PopStyleVar(2);
            }
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Download action with visual feedback
            static bool isLoading = false;
            if (isLoading) {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), "Downloading %d textures...", count);
            } else {
                if (ImGui::Button("Download Textures", ImVec2(ws.size.x - 30, 40))) {
                    isLoading = true;
                    m_fpsLogger = std::make_unique<texgan::monitoring::FPSLogger>(useSingleContextApproach ? "single" : "shared", count); 
                    m_fetcher.fetchMany(count, IMAGE_PROVIDER_URL,
                        [this](bool success, const Images& images) {
                            if (useSingleContextApproach) {
                                m_singleUploader->processImages(success, images);
                            } else {
                                m_sharedUploader->processImages(success, images);
                            }
                            isLoading = false;
                        }
                    );
                }
                
                // Help marker
                ImGui::SameLine();
            }

            ImGui::End();
            WindowStyle::resetStyles();
        }

        void showRendererPropertiesWindow(const WindowStyle& ws){
            WindowStyle::applyStyles(ws);

            const auto& entities = m_world.getEntities();

            ImGui::SetNextWindowPos(ws.position);
            ImGui::SetNextWindowSize(ws.size);

            ImGui::PushStyleColor(ImGuiCol_Text, ws.window.titleTextColor);
            ImGui::Begin("Renderer Properties", nullptr, ws.window.flags);
            ImGui::PopStyleColor();

            /* ────────── Camera ────────── */
            ImGui::SeparatorText("Camera");

            std::stringstream ss;
            ss << "Position: " << std::fixed << std::setprecision(2)
            << m_camera.position.x << ", "
            << m_camera.position.y << ", "
            << m_camera.position.z;
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.8f, 1.0f), "%s", ss.str().c_str());
            ImGui::Spacing();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Movement Speed:");   ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##MoveSpeed", &m_camera.movementSpeed, 1.0f, 1000.0f);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Movement Sensitivity:"); ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##MoveSensitivity", &m_camera.mouseSensitivity, 0.01f, 1.0f);

            /* ────────── Shape Transformation ────────── */
            ImGui::SeparatorText("Shape Transformation");

            bool hasSelection =
                (m_activeCube != texgan::ecs::kInvalidEntity) &&
                (std::find(entities.begin(), entities.end(), m_activeCube) != entities.end());

            if (!hasSelection)
                ImGui::BeginDisabled(true);          // Gray-out everything that follows

            auto* transform = hasSelection ? m_world.getTransform(m_activeCube) : nullptr;
            static texgan::ecs::TransformComponent dummy{};   // zero-initialised fallback
            auto& t = transform ? *transform : dummy;

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Size:");      ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat3("##Size", &t.scale[0], 1.0f, 100.0f);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Position:");  ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat3("##Pos", &t.position[0], -1000.0f, 1000.0f);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Rotation:");  ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat3("##Rot", &t.rotation[0], -1.0f, 1.0f);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Angle:");     ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##Ang", &t.angle, -360.0f, 360.0f);

            if (!hasSelection) {
                ImGui::EndDisabled();
            }

            /* ────────── Scene Stats ────────── */
            int totalEntities   = static_cast<int>(entities.size());
            int texturedCount   = 0;
            for (const auto& e : entities) {
                auto tex = m_world.getTexture(e);
                if (tex && tex->textureId != 0)
                    ++texturedCount;
            }

            ImGui::SeparatorText("Scene Stats");

            ImGui::Text("Total Entities: ");     ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0, 0.7f, 1), "%d", totalEntities);

            ImGui::Text("Textured Entities: ");  ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0, 0.7f, 1), "%d", texturedCount);

            float ratio = (totalEntities > 0) ?
                        static_cast<float>(texturedCount) / totalEntities : 0.0f;
            ImGui::ProgressBar(ratio, ImVec2(ws.size.x - 30, 20), (std::to_string(texturedCount) + "/" +std::to_string(totalEntities)).c_str());

            /* ─────────────────────────────── */
            ImGui::End();
            WindowStyle::resetStyles();
        }

        void showPerformanceMetricsWindow(const WindowStyle& ws) {
            // Static buffers & last‐time marker, internal to this function
            static std::array<float,100> fpsHistory{};
            static std::array<float,100> frameTimeHistory{};
            static std::array<float,100> memoryHistory{};
            static float              lastTime = glfwGetTime();

            // Compute deltas & shift histories
            float now = glfwGetTime();
            float dt  = now - lastTime;
            lastTime  = now;

            std::rotate(fpsHistory.begin(),       fpsHistory.begin()+1,       fpsHistory.end());
            std::rotate(frameTimeHistory.begin(), frameTimeHistory.begin()+1, frameTimeHistory.end());
            std::rotate(memoryHistory.begin(),    memoryHistory.begin()+1,    memoryHistory.end());

            // Sample new values
            fpsHistory.back()       = ImGui::GetIO().Framerate;
            frameTimeHistory.back() = dt * 1000.0f;
            #ifdef _WIN32
            {
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
                memoryHistory.back() = pmc.WorkingSetSize / (1024.0f*1024.0f);
            else
                memoryHistory.back() = 0.0f;
            }
            #else
            memoryHistory.back() = 0.0f;
            #endif

            // Now render
            WindowStyle::applyStyles(ws);
            ImGui::SetNextWindowPos(ws.position);
            ImGui::SetNextWindowSize(ws.size);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1,0,0,1));

            ImGui::PushStyleColor(ImGuiCol_Text, ws.window.titleTextColor);
            ImGui::Begin("Performance Metrics", nullptr, ws.window.flags);
            ImGui::PopStyleColor();

            float colW = (ws.size.x - 40.0f) / 3.0f;

            // FPS column
            ImGui::BeginChild("FPS", ImVec2(colW,0), true);
            ImGui::TextColored(ImVec4(0.2f,1,0.2f,1), "FPS");
            ImGui::Text("%.1f", fpsHistory.back());
            ImGui::PlotLines("##fps", fpsHistory.data(), fpsHistory.size(), 0, nullptr, 0.0f, FLT_MAX, ImVec2(colW-20,80));
            ImGui::EndChild();

            ImGui::SameLine();

            // Frame time column
            ImGui::BeginChild("FrameTime", ImVec2(colW,0), true);
            ImGui::TextColored(ImVec4(1,0.5f,0.2f,1), "Frame Time");
            ImGui::Text("%.2f ms", frameTimeHistory.back());
            ImGui::PlotLines("##ft", frameTimeHistory.data(), frameTimeHistory.size(), 0, nullptr, 0.0f, 50.0f, ImVec2(colW-20,80));
            ImGui::EndChild();

            ImGui::SameLine();

            // Memory column
            ImGui::BeginChild("Memory", ImVec2(colW,0), true);
            ImGui::TextColored(ImVec4(0.2f,0.6f,1,1), "Memory");
            ImGui::Text("%.1f MB", memoryHistory.back());
            ImGui::PlotLines("##mem", memoryHistory.data(), memoryHistory.size(), 0, nullptr, 0.0f, FLT_MAX, ImVec2(colW-20,80));
            ImGui::EndChild();

            ImGui::End();
            ImGui::PopStyleColor();
            WindowStyle::resetStyles();
        }

        void showViewportBorderWindow() {
            // Save current style
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4 oldWindowBg = style.Colors[ImGuiCol_WindowBg];
            ImVec4 oldBorder = style.Colors[ImGuiCol_Border];
            float oldAlpha = style.Alpha;
            
            // Setup transparent border window style
            style.Colors[ImGuiCol_WindowBg] = ImVec4(0,0,0,0); // Fully transparent
            style.Colors[ImGuiCol_Border] = ImVec4(1,1,1,1);   // White border
            style.Alpha = 1.0f; // Full opacity for the border
            
            // Create a border window that matches the viewport
            ImGui::SetNextWindowPos(ImVec2(m_viewport.x, m_viewport.y));
            ImGui::SetNextWindowSize(ImVec2(m_viewport.z, m_viewport.w));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
            
            ImGui::Begin("Viewport Border", nullptr, 
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoSavedSettings);
            
            // Optional: Add corner markers
            const float marker_size = 10.0f;
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 p_min = ImGui::GetWindowPos();
            ImVec2 p_max = ImVec2(p_min.x + m_viewport.z, p_min.y + m_viewport.w);

            float markerSize = 10.0f;
            
            // Draw corner markers
            draw_list->AddLine(p_min, ImVec2(p_min.x + marker_size, p_min.y), IM_COL32(255,255,255,255), markerSize);
            draw_list->AddLine(p_min, ImVec2(p_min.x, p_min.y + marker_size), IM_COL32(255,255,255,255), markerSize);
            
            draw_list->AddLine(ImVec2(p_max.x, p_min.y), ImVec2(p_max.x - marker_size, p_min.y), IM_COL32(255,255,255,255), markerSize);
            draw_list->AddLine(ImVec2(p_max.x, p_min.y), ImVec2(p_max.x, p_min.y + marker_size), IM_COL32(255,255,255,255), markerSize);
            
            draw_list->AddLine(ImVec2(p_min.x, p_max.y), ImVec2(p_min.x + marker_size, p_max.y), IM_COL32(255,255,255,255), markerSize);
            draw_list->AddLine(ImVec2(p_min.x, p_max.y), ImVec2(p_min.x, p_max.y - marker_size), IM_COL32(255,255,255,255), markerSize);
            
            draw_list->AddLine(p_max, ImVec2(p_max.x - marker_size, p_max.y), IM_COL32(255,255,255,255), markerSize);
            draw_list->AddLine(p_max, ImVec2(p_max.x, p_max.y - marker_size), IM_COL32(255,255,255,255), markerSize);
            
            ImGui::End();
            
            // Restore style
            ImGui::PopStyleVar(2);
            style.Colors[ImGuiCol_WindowBg] = oldWindowBg;
            style.Colors[ImGuiCol_Border] = oldBorder;
            style.Alpha = oldAlpha;
        }
        
        void showCubeCreatorWindow(const WindowStyle& ws) {
        WindowStyle::applyStyles(ws);
        
        ImGui::SetNextWindowPos(ws.position);
        ImGui::SetNextWindowSize(ws.size);
        
        ImGui::PushStyleColor(ImGuiCol_Text, ws.window.titleTextColor);
        ImGui::Begin("Cube Creator", nullptr, ws.window.flags);
        ImGui::PopStyleColor();

        // Calculate available width for controls
        const float labelWidth = ImGui::CalcTextSize("Instances Per Cube:").x + ImGui::GetStyle().ItemSpacing.x;
        const float controlWidth = ws.size.x - labelWidth - ImGui::GetStyle().WindowPadding.x * 2;

        // Cube settings section
        ImGui::SeparatorText("Cube Settings");
        
        // Number of cubes control
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Number of Cubes:");
        ImGui::SameLine();
        static int cubeCount = 100;
        ImGui::SetNextItemWidth(controlWidth);
        ImGui::SliderInt("##CubeCount", &cubeCount, 1, 1000);
        
        // Position range controls
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Min Position:");
        ImGui::SameLine();
        static float minPos = -200.0f;
        ImGui::SetNextItemWidth(controlWidth);
        ImGui::SliderFloat("##MinPos", &minPos, -1000.0f, 0.0f);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Max Position:");
        ImGui::SameLine();
        static float maxPos = 200.0f;
        ImGui::SetNextItemWidth(controlWidth);
        ImGui::SliderFloat("##MaxPos", &maxPos, 0.0f, 1000.0f);
        
        ImGui::Spacing();

        // Rendering mode section
        ImGui::SeparatorText("Rendering Mode");
        static bool useInstancing = false;
        static int instancesPerCube = 100;
        
        ImGui::AlignTextToFramePadding();
        ImGui::Checkbox("Use Instanced Rendering##Instanced", &useInstancing);
        
        if (useInstancing) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Instances Per Cube:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(controlWidth);
            ImGui::SliderInt("##InstancesPerCube", &instancesPerCube, 1, 1000);
            
            // Help marker
            ImGui::SameLine();
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Number of instances to render for each cube");
                ImGui::EndTooltip();
            }
        }
        ImGui::Spacing();

        // Button row - using available width with proper spacing
        const float buttonSpacing = 10.0f;
        const float totalButtonWidth = ws.size.x - ImGui::GetStyle().WindowPadding.x * 2;
        const float buttonWidth = (totalButtonWidth - buttonSpacing) / 2.0f;
        
        if (ImGui::Button("Create Cubes", ImVec2(buttonWidth, 40))) {
            auto cubePositions = texgan::helpers::generateRandom3dPositions(cubeCount, minPos, maxPos);
            
            if (useInstancing) {
                auto instancePositions = texgan::helpers::generateRandom3dPositions(instancesPerCube, minPos, maxPos);
                // Create instanced cubes
                auto mesh = texgan::helpers::makeCubes(instancePositions);
                
                for (size_t i = 0; i < cubeCount; ++i) {
                    auto entity = m_world.createEntity();
                    m_world.addMesh(entity, mesh);
                    
                    auto transform = texgan::ecs::TransformComponent();
                    transform.position = cubePositions[i];
                    transform.scale = glm::vec3(10.0f);
                    
                    m_world.addTransform(entity, transform);
                    m_world.addRenderComponent(entity, {
                        texgan::ecs::RenderType::Instanced,
                        GL_TRIANGLES
                    });
                    m_world.addTexture(entity, {});
                }
            } else {
                // Create regular cubes
                auto mesh = texgan::helpers::makeCubes();
                
                for (size_t i = 0; i < cubeCount; ++i) {
                    auto entity = m_world.createEntity();
                    m_world.addMesh(entity, mesh);
                    
                    auto transform = texgan::ecs::TransformComponent();
                    transform.position = cubePositions[i];
                    transform.scale = glm::vec3(10.0f);
                    
                    m_world.addTransform(entity, transform);
                    m_world.addRenderComponent(entity, {
                        texgan::ecs::RenderType::Simple,
                        GL_TRIANGLES
                    });
                    m_world.addTexture(entity, {});
                }
            }
        }

        ImGui::SameLine(0, buttonSpacing);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 0, 0, 1));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        if (ImGui::Button("Clear All", ImVec2(buttonWidth, 40))) {
            m_world.clear();
        }
        ImGui::PopStyleColor(2);
        
        ImGui::End();
        WindowStyle::resetStyles();
    }
        
        // Call this once per frame in your main loop, after polling events but before 3D rendering:
        void render() {
            // Start ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            static ImGuiIO& io = ImGui::GetIO();

            int width, height;
            glfwGetFramebufferSize(m_window, &width, &height);
            float windowWidth = io.DisplaySize.x;
            float windowHeight = io.DisplaySize.y;

            
            static WindowStyle infoWindowStyle;
            infoWindowStyle.position = {0.0f, 0.0f};
            infoWindowStyle.size = {windowWidth, 130};
            infoWindowStyle.window.flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
            infoWindowStyle.window.backgroundColor = ImVec4(0.1137f, 0.2824f, 0.4196f, 1.0f);
            infoWindowStyle.spacing.itemSpacing = {15.0f, 0.0f};

            showInfoWindow(myImage, infoWindowStyle);


            static WindowStyle pmWindowStyle;
            pmWindowStyle.size = {windowWidth, 220};
            pmWindowStyle.position = {0, windowHeight - pmWindowStyle.size.y};
            pmWindowStyle.window.padding  = ImVec2(10, 10);
            pmWindowStyle.spacing.itemSpacing = ImVec2(10, 5);
            pmWindowStyle.window.flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
            pmWindowStyle.frame.borderSize = 0.0f;
            showPerformanceMetricsWindow(pmWindowStyle);


            static WindowStyle tlWindowStyle;
            tlWindowStyle.position = {0, infoWindowStyle.size.y + infoWindowStyle.position.y };
            tlWindowStyle.size = {350, 280};
            tlWindowStyle.window.flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
            showTextureLoaderControls(tlWindowStyle);


            
            
            static WindowStyle rpWindowStyle;
            rpWindowStyle.position = {windowWidth - tlWindowStyle.size.x, infoWindowStyle.size.y};
            rpWindowStyle.size = {tlWindowStyle.size.x, windowHeight - infoWindowStyle.size.y - pmWindowStyle.size.y};
            rpWindowStyle.window.flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
            showRendererPropertiesWindow(rpWindowStyle);



            static WindowStyle ccWindowStyle;
            ccWindowStyle.position = {0, tlWindowStyle.size.y + tlWindowStyle.position.y};
            ccWindowStyle.size = {tlWindowStyle.size.x, windowHeight - infoWindowStyle.size.y - pmWindowStyle.size.y - tlWindowStyle.size.y};
            ccWindowStyle.window.flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
            
            showCubeCreatorWindow(ccWindowStyle);

            showViewportBorderWindow();


            // Render ImGui
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


            updateActiveCube();

            // Each frame, call update on the active uploader
            useSingleContextApproach ? m_singleUploader->update(m_world) : m_sharedUploader->update(m_world);


            
            float viewportX = tlWindowStyle.size.x;  // Start after left panel
            float viewportY = infoWindowStyle.size.y;  // Start below top panel
            float viewportWidth = windowWidth - tlWindowStyle.size.x - rpWindowStyle.size.x;
            float viewportHeight = windowHeight - infoWindowStyle.size.y - pmWindowStyle.size.y;

            m_viewport.x = viewportX;
            m_viewport.y = viewportY;
            m_viewport.z =  viewportWidth;
            m_viewport.w =  viewportHeight;

            if(m_fpsLogger) m_fpsLogger->frameTick();
        }

        ImVec4 getViewport() const {
            return m_viewport;
        }


    private:
        GLFWwindow* m_window;
        texgan::ecs::World& m_world;


        texgan::core::Camera& m_camera;
        aif::ImageFetcher m_fetcher;

        std::unique_ptr<monitoring::FPSLogger> m_fpsLogger;

        bool useSingleContextApproach;
        std::unique_ptr<texgan::loading::SingleContextUploadApproach> m_singleUploader;
        std::unique_ptr<texgan::loading::SharedContextUploadApproach> m_sharedUploader;
        GLuint myImage{};

        ImVec4 m_viewport{};

        texgan::ecs::Entity m_activeCube;

        // Helper functions
        void updateActiveCube(){
            const auto& entities = m_world.getEntities();

            if (entities.empty()) {
                m_activeCube = texgan::ecs::kInvalidEntity;
                return;
            }

            // If we have no selection or the previous one vanished → pick first
            if (m_activeCube == texgan::ecs::kInvalidEntity || std::find(entities.begin(), entities.end(), m_activeCube) == entities.end()){
                m_activeCube = entities.front();
            }

            // Cycle with SPACE
            if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                auto it = std::find(entities.begin(), entities.end(), m_activeCube);
                ++it;
                m_activeCube = (it != entities.end()) ? *it : entities.front();
                glfwWaitEventsTimeout(0.1);            // debounce
            }
        }

    };

}


// ==================== Main Application ====================

void startApplication(){
    auto window = texgan::core::Window(1800, 900, "These people do not exist", false);
    auto renderer = texgan::rendering::Renderer(window);
    auto world = texgan::ecs::World();

    // Setup camera
    texgan::core::Camera camera;
    texgan::core::CameraController cameraController(window.handle(), camera, false);
    
    texgan::ui::TextureLoaderUI ui(window, world, camera);

    auto shader = renderer.m_defaultShader;

    while (!window.shouldClose()) {
        // Update Camera controller
        cameraController.update();

        // Clear screen
        window.clear(0.2f, 0.3f, 0.3f);


        auto vp = ui.getViewport();

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window.handle(), &fbWidth, &fbHeight);

        // Flip y from ImGui (top-left) to OpenGL (bottom-left)
        int glY = static_cast<int>(fbHeight - vp.y - vp.w);

        glViewport(static_cast<GLint>(vp.x),
                static_cast<GLint>(glY),
                static_cast<GLint>(vp.z),
                static_cast<GLint>(vp.w));



        // Set camera matrices
        shader.use();

        shader.setMat4("view", camera.getViewMatrix());
        float aspectRatio = static_cast<float>(vp.z) / static_cast<float>(vp.w);
        aspectRatio = glm::max(aspectRatio, 0.001f); // Prevent division by zero
        

        shader.setMat4("projection", camera.getProjectionMatrix(aspectRatio));
        shader.unuse();
        
        
        // Render scene
        renderer.render(world);
        
        ui.render();
        
        window.swapBuffers();
        window.pollEvents();
    }

}

int main(int argc, char** argv){
    try{
        startApplication();
    }catch(const std::exception& e){
        std::cout << e.what() << std::endl;
    }catch(...){
        std::cout << "Unknown exception!" << std::endl;
    }
    return 0;
}



#ifdef _WIN32
// For WIN32 subsystem builds (GUI)
#ifdef TEXGAN_GUI
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv); // Bridge to main()
}
#endif
#endif
