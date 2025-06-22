#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <cmath>
#include <random>


#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef _DEBUG
#define GL_CHECK_ERROR() {\
    GLenum err = glGetError();\
    if (err != GL_NO_ERROR) {\
        std::cerr << "OpenGL error: " << err << " at line " << __LINE__ << std::endl;\
    }\
}
#else
#define GL_CHECK_ERROR()
#endif

namespace fs = std::filesystem;

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
    
            m_window = glfwCreateWindow(width, height, title.c_str(), fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
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
            return glm::perspective(glm::radians(zoom), aspectRatio, 0.1f, 1000.0f);
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
        CameraController(GLFWwindow * window, Camera& camera): m_window(window), m_camera(camera), m_firstMouse(true), m_lastX(0), m_lastY(0), m_lastFrame(0.0f), m_deltaTime(0.0f){
            // Get initial mouse position
            double xpos, ypos;
            glfwGetCursorPos(m_window, &xpos, &ypos);
            m_lastX = static_cast<float>(xpos);
            m_lastY = static_cast<float>(ypos);

            // Set callbacks
            glfwSetWindowUserPointer(m_window, this);
            
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
            if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS)
                m_camera.processKeyboard(Camera::UP, m_deltaTime);
            if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                m_camera.processKeyboard(Camera::DOWN, m_deltaTime);
        }
        
        void processMouseMovement(float xpos, float ypos) {
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

namespace texgan::ecs{

    struct TransformComponent{
        glm::vec3 position{0.0F};
        glm::vec3 rotation{0.0F};
        glm::vec3 scale{1.0F};

        glm::mat4 getModelMatrix() const{
            glm::mat4 model(1.0F);
            model = glm::translate(model, position);
            model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, scale);
            return model;
        }
    };

    
    class MeshComponent{
        unsigned int m_VAO, m_VBO, m_EBO;
        std::vector<unsigned int> m_instanceVBOs;
        int m_attributeIndex;
        size_t m_stride;
        size_t m_vertexCount, m_indexCount, m_instanceCount;
        size_t m_vertexSize;
    public:

        MeshComponent() : m_VAO(0), m_VBO(0), m_EBO(0), m_attributeIndex(0), m_stride(0), m_vertexCount(0), m_vertexSize(0), m_indexCount(0), m_instanceCount(0) {
            glGenVertexArrays(1, &m_VAO);
        }

        ~MeshComponent(){
            if(m_VAO){
                glDeleteVertexArrays(1, &m_VAO);
            }

            if(m_VBO){
                glDeleteBuffers(1, &m_VBO);
            }
            if(m_EBO){
                glDeleteBuffers(1, &m_EBO);
            }
        }


        void setVertexData(const std::vector<float>& vertices) {
            bind();
            m_vertexSize = sizeof(vertices[0]);
            glGenBuffers(1, &m_VBO);
            glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * m_vertexSize, vertices.data(), GL_STATIC_DRAW);
            m_vertexCount = vertices.size();
            unbind();
        }

        void setIndexData(const std::vector<unsigned int>& indices) {
            bind();
            glGenBuffers(1, &m_EBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);
            m_indexCount = indices.size();
            unbind();
        }


        void addAttribute(int size, bool normalized = false, GLenum type = GL_FLOAT, const void* ptr = (void*)0) {
            bind();
            glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
            glEnableVertexAttribArray(m_attributeIndex);
            glVertexAttribPointer(m_attributeIndex, size, type, normalized, m_stride, ptr);
            
            // Only advance stride for regular attributes (not instanced)
            if (std::find(m_instanceVBOs.begin(), m_instanceVBOs.end(), m_VBO) == m_instanceVBOs.end()) {
                m_stride += size * m_vertexSize;
            }
            
            m_attributeIndex++;
            unbind();
        }

        template<typename T>
        void updateInstanceAttribute(int attribIndex, const std::vector<T>& instanceData) {
            if (attribIndex < 0 || attribIndex >= m_instanceVBOs.size()) return;
            
            bind();
            glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBOs[attribIndex]);
            glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(T), instanceData.data(), GL_STATIC_DRAW);
            unbind();
        }
        template<typename T>
        void addInstanceAttribute(const std::vector<T>& instanceData, int size, bool normalized = false, GLenum type = GL_FLOAT) {
            bind();
            
            // Create new VBO for this instance attribute
            unsigned int instanceVBO;
            glGenBuffers(1, &instanceVBO);
            m_instanceVBOs.push_back(instanceVBO);
            
            // Upload instance data
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
            glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(T), instanceData.data(), GL_STATIC_DRAW);
            
            // Set up vertex attribute
            glEnableVertexAttribArray(m_attributeIndex);
            glVertexAttribPointer(m_attributeIndex, size, type, normalized ? GL_TRUE : GL_FALSE, size * sizeof(T), (void*)0);
            glVertexAttribDivisor(m_attributeIndex, 1);
            m_attributeIndex++;
            
            m_instanceCount = instanceData.size();
            unbind();
        }

        // Add this method to your MeshComponent class
        void addInstanceMatrixAttribute(const std::vector<glm::mat4>& instanceMatrices) {
            bind();
            
            // Create new VBO for instance matrices
            unsigned int instanceVBO;
            glGenBuffers(1, &instanceVBO);
            m_instanceVBOs.push_back(instanceVBO);
            
            // Upload instance data
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
            glBufferData(GL_ARRAY_BUFFER, instanceMatrices.size() * sizeof(glm::mat4), instanceMatrices.data(), GL_STATIC_DRAW);
            
            // Set up vertex attributes for each column of the matrix
            for (int i = 0; i < 4; i++) {
                glEnableVertexAttribArray(m_attributeIndex + i);
                glVertexAttribPointer(m_attributeIndex + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
                glVertexAttribDivisor(m_attributeIndex + i, 1);
            }
            
            m_attributeIndex += 4; // Advance by 4 attribute locations

            m_instanceCount = instanceMatrices.size();
            unbind();
        }

        void finalize() {
            glBindVertexArray(0);
        }

        void bind() {
            glBindVertexArray(m_VAO);
        }

        void unbind() {
            glBindVertexArray(0);
        }


        bool usesEBO() const { return m_EBO != 0; }

        size_t getVertexCount() const { return m_vertexCount; }

        size_t getIndexCount() const { return m_indexCount; }
        
        size_t getInstanceCount() const { return m_instanceCount; }

    };


    struct TextureComponent{
        GLuint textureId{};

        ~TextureComponent() {
            if (textureId) glDeleteTextures(1, &textureId);
        }
    };

    
    enum class RenderType{ Simple, Instanced, Batched };

    struct RenderComponent{
        RenderType type{RenderType::Simple};
        GLuint materialId;
        GLenum primitive{GL_TRIANGLES};
        uint32_t layer{0};
    };

    // Entity
    using Entity = uint32_t;
    inline Entity nextEntityId{0};

    inline Entity createEntity(){
        return nextEntityId++;
    }

    // World
    class World{
    public:
        Entity createEntity(){
            Entity e = createEntityId();
            m_entities.push_back(e);
            return e;
        }


        TransformComponent& addTransform(Entity entity, const ecs::TransformComponent& transform = {}){
            return m_transforms[entity] = transform;
        }
        

        TextureComponent& addTexture(Entity entity, const ecs::TextureComponent& textureId = {}){
            return m_textures[entity] = textureId;
        }

        MeshComponent& addMesh(Entity entity, const MeshComponent& mesh = {}) {
            return m_meshes[entity] = mesh;
        }
        
        RenderComponent& addRenderComponent(Entity entity, const RenderComponent& render = {}) {
            return m_renderComponents[entity] = render;
        }

        RenderComponent* getRenderComponent(Entity entity) {
            auto it = m_renderComponents.find(entity);
            return it != m_renderComponents.end() ? &it->second : nullptr;
        }

        TransformComponent * getTransform(Entity entity){
            auto it = m_transforms.find(entity);
            return it != m_transforms.end() ? &it->second : nullptr;
        }

        MeshComponent* getMesh(Entity entity) {
            auto it = m_meshes.find(entity);
            return it != m_meshes.end() ? &it->second : nullptr;
        }

        TextureComponent* getTexture(Entity entity) {
            auto it = m_textures.find(entity);
            return it != m_textures.end() ? &it->second : nullptr;
        }

        const std::vector<Entity>& getEntities() const { return m_entities; }   

        void destroyEntity(Entity entity) {
            m_transforms.erase(entity);
            m_meshes.erase(entity);
            m_textures.erase(entity);
            m_renderComponents.erase(entity);
            m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), entity), m_entities.end());
        }

    private:
        Entity createEntityId(){
            return texgan::ecs::createEntity();
        }

        std::vector<Entity> m_entities;
        std::unordered_map<Entity, TransformComponent> m_transforms;
        std::unordered_map<Entity, TextureComponent> m_textures;
        std::unordered_map<Entity, RenderComponent> m_renderComponents;
        std::unordered_map<Entity, MeshComponent> m_meshes;
    };

};

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

                if (!transform || !mesh || !render) continue;

                // Set model matrix
                m_shader.setMat4("model", transform->getModelMatrix());

                // Bind texture if available
                if (texture) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, texture->textureId);
                }



                mesh->bind();
                if(mesh->usesEBO()){
                    glDrawElements(render->primitive, mesh->getIndexCount(), GL_UNSIGNED_INT, 0);
                }else{
                    glDrawArrays(render->primitive, 0, mesh->getIndexCount());
                }
                mesh->unbind();
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

                // if(!mesh || !render || transform) continue;

                glm::mat4 model = transform->getModelMatrix();
                m_shader.setMat4("model", model);

                mesh->bind();
                if (mesh->usesEBO()) {
                    glDrawElementsInstanced(render->primitive, mesh->getIndexCount(), GL_UNSIGNED_INT, 0, mesh->getInstanceCount());
                } else {
                    glDrawArraysInstanced(render->primitive, 0, mesh->getVertexCount(), mesh->getInstanceCount());
                }
                mesh->unbind();
            }
            
            m_shader.setInt("useInstancing", 0);
            m_shader.unuse();    
        }

    private:
        ShaderProgram m_shader;
        GLuint m_instanceVBO;
    };

    class BatchRenderer: public IRenderStrategy{
    public:
        explicit BatchRenderer(const ShaderProgram& shader): m_shader(shader){}

        void render(const std::vector<ecs::Entity>& entities, ecs::World& world) override {

        }
    private:
        ShaderProgram m_shader;
    };
  
    class Renderer{
    public:
        explicit Renderer(core::Window & window): m_window{window}{
           
            m_defaultShader.loadFromFiles(texgan::utils::shader("shader.vert"), texgan::utils::shader("shader.frag"));

            m_strategies[ecs::RenderType::Simple] = std::make_unique<SimpleRenderer>(m_defaultShader);
            m_strategies[ecs::RenderType::Instanced] = std::make_unique<InstancedRenderer>(m_defaultShader);
            m_strategies[ecs::RenderType::Batched] = std::make_unique<BatchRenderer>(m_defaultShader);
        }

        void render(ecs::World & world){
            std::unordered_map<ecs::RenderType, std::vector<ecs::Entity>> renderGroups;

            for (auto entity : world.getEntities()) {
                if (auto* render = world.getRenderComponent(entity)) {
                    renderGroups[render->type].push_back(entity);
                }
            }

            // Execute each strategy
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


extern std::vector<glm::vec3> generateRandomColors(size_t count);
extern std::vector<glm::vec2> generateRandom2dPositions(size_t count);
extern std::vector<glm::vec3> generateRandom3dPositions(size_t count, float min, float max);


extern std::vector<float> cubeVertices;
extern std::vector<unsigned int> cubeIndices;





std::vector<glm::vec3> getPositions(size_t count) {
    std::vector<glm::vec3> positions;
    positions.reserve(count);

    // Seed random generators
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::normal_distribution<float> normal_dist(0.0f, 0.15f);

    // Galaxy parameters
    const float armCount = 5.0f;         // Number of spiral arms
    const float tightness = 0.5f;        // How tight the spiral is
    const float armWidth = 0.25f;        // Width of each arm
    const float thickness = 0.3f;        // Thickness of the galaxy
    const float coreRadius = 0.2f;       // Radius of the central core
    const float rotation = 0.7f;        // Overall rotation factor

    for (size_t i = 0; i < count; ++i) {
        // Random point in disk
        float r = std::sqrt(dist(gen));  // Square root for uniform distribution
        float theta = dist(gen) * 2.0f * glm::pi<float>();
        float z = (dist(gen) - 0.5f) * thickness;

        // Apply spiral pattern
        float armOffset = (dist(gen) * 2.0f - 1.0f) * armWidth;
        float spiralR = r + armOffset;
        float spiralTheta = theta + rotation * std::log(spiralR + 0.1f) / tightness;

        // Distribute points among arms
        float whichArm = dist(gen) * armCount;
        float armAngle = whichArm * (2.0f * glm::pi<float>() / armCount);

        // Final position in polar coordinates
        float finalR = r * (1.0f - coreRadius) + coreRadius;
        float finalTheta = spiralTheta + armAngle;

        // Convert to Cartesian coordinates
        float x = finalR * std::cos(finalTheta);
        float y = finalR * std::sin(finalTheta);

        // Add some noise for organic feel
        x += normal_dist(gen) * (1.0f - r);
        y += normal_dist(gen) * (1.0f - r);
        z += normal_dist(gen) * thickness * (1.0f - r);

        positions.emplace_back(x, y, z);
    }

    return positions;
}

auto makeCubes(size_t instanceCount){
    // std::vector<glm::vec3> positions = generateRandom3dPositions(instanceCount, -10.0f, 10.0f);
    std::vector<glm::vec3> positions = makeGrid(sqrt(instanceCount) + 1, sqrt(instanceCount) + 1, 50.0f, 5.0f);

    // Create mesh component
    texgan::ecs::MeshComponent mesh;
    mesh.setVertexData(cubeVertices);
    mesh.setIndexData(cubeIndices);

    // Regular attributes
    mesh.addAttribute(3); // position
    mesh.addAttribute(3); // normal
    mesh.addAttribute(2); // texCoord

    // Create instance matrices
    std::vector<glm::mat4> instanceMatrices;
    for (int i = 0; i < instanceCount; i++) {
        glm::mat4 model(1.0f);
        
        model = glm::translate(model, positions[i]);
        instanceMatrices.push_back(model);
    }

    // Add instance matrix attribute
    mesh.addInstanceMatrixAttribute(instanceMatrices);
    mesh.finalize();
    return mesh;
}



void test(){
    auto window = texgan::core::Window(1800, 800, "Rendering Demo", false);
    auto renderer = texgan::rendering::Renderer(window);
    auto world = texgan::ecs::World();

    std::cout << glfwGetVersionString() << std::endl;

    // Create camera
    texgan::core::Camera camera;

    // Create Camera Controller
    texgan::core::CameraController cameraController(window.handle(), camera);
    
    // Setup input callbacks
    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    

    auto mesh = makeCubes(1000);

    glm::vec3 rotation(0.0f);
    glm::vec3 scale(1.0f);


    for(glm::vec3 pos: makeGrid(5, 5, 100, 5)){
        auto entity = world.createEntity();
        world.addMesh(entity, mesh);
        world.addTransform(entity, {pos, rotation, scale});
        GLenum primitve = true ? GL_TRIANGLES : GL_POINTS;
        world.addRenderComponent(entity, {texgan::ecs::RenderType::Instanced, 0, primitve});
    }
    

    while (!window.shouldClose()) {
        // Update Camera controller
        cameraController.update();


        // Clear screen
        window.clear(0.11f, 0.1f, 0.12f);

        int width, height;
        glfwGetFramebufferSize(window.handle(), &width, &height);
        glViewport(0, 0, width, height);

        double xpos, ypos;
        glfwGetCursorPos(window.handle(), &xpos, &ypos);
        glfwGetWindowSize(window.handle(), &width, &height);
        glm::vec3 mousePos(xpos/width, 1.0-ypos/height, 0.0);
        
        
    
        // Set camera matrices
        renderer.m_defaultShader.use();
        renderer.m_defaultShader.setMat4("view", camera.getViewMatrix());
        renderer.m_defaultShader.setMat4("projection", camera.getProjectionMatrix(static_cast<float>(width) / static_cast<float>(height)));
        renderer.m_defaultShader.setFloat("time", static_cast<float>(glfwGetTime()));
        renderer.m_defaultShader.setVec3("mousePos", mousePos);

        renderer.m_defaultShader.unuse();
        
        // Render scene
        renderer.render(world);

        if(glfwGetKey(window.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
        
        window.swapBuffers();
        window.pollEvents();
    }
}

int main(int argc, char *argv[]){

    try{
        test();
    }catch(const std::exception& e){
        std::cout << e.what() << std::endl;
    }catch(...){
        std::cout << "Unknown exception!" << std::endl;
    }

    return 0;
}