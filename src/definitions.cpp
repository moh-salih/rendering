#include <iostream>
#include <vector>
#include <random>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>


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


std::vector<float> cubeVertices = {
    // Front face
    -0.5f, -0.5f,  0.5f,  // 0
     0.5f, -0.5f,  0.5f,  // 1
     0.5f,  0.5f,  0.5f,  // 2
    -0.5f,  0.5f,  0.5f,  // 3

    // Back face
    -0.5f, -0.5f, -0.5f,  // 4
     0.5f, -0.5f, -0.5f,  // 5
     0.5f,  0.5f, -0.5f,  // 6
    -0.5f,  0.5f, -0.5f   // 7
};

std::vector<unsigned int> cubeIndices = {
    // Front face
    0, 1, 2,
    2, 3, 0,

    // Right face
    1, 5, 6,
    6, 2, 1,

    // Back face
    5, 4, 7,
    7, 6, 5,

    // Left face
    4, 0, 3,
    3, 7, 4,

    // Top face
    3, 2, 6,
    6, 7, 3,

    // Bottom face
    4, 5, 1,
    1, 0, 4
};

