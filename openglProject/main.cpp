// MinecraftClone.cpp - Fixed version with better visuals
// Integrated: Improved Perlin Noise (Ken Perlin, 2002) + AABB voxel collision/gravity
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <map>
#include <cmath>
#include <random>
#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

float pitchAngle = 0.0f;
float yawAngle = -90.0f;

// Block Types
enum BlockType {
    AIR = 0,
    GRASS = 1,
    DIRT = 2,
    STONE = 3,
    WOOD = 4,
    LEAVES = 5,
    WATER = 6,
    SAND = 7
};
int positionX = 0;
// World Configuration
const int CHUNK_SIZE = 16;
const int WORLD_HEIGHT = 64;
const int RENDER_DISTANCE = 4;
const float BLOCK_SIZE = 1.0f;

// Physics constants
const float GRAVITY_ACCEL = -20.0f;   // blocks/s^2, downward
const float TERMINAL_VELOCITY = -50.0f;
const float JUMP_SPEED = 10.0f; // apex = JUMP_SPEED^2 / (2*|GRAVITY_ACCEL|) ~= 2.5 blocks

// Camera/Player
struct Camera {
    glm::vec3 position = glm::vec3(0, 40, 0); // eye position
    glm::vec3 front = glm::vec3(0, 0, -1);
    glm::vec3 up = glm::vec3(0, 1, 0);
    glm::vec3 right = glm::vec3(1, 0, 0);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 5.0f;
    float sensitivity = 0.1f;

    // Physics/collision state
    bool flying = false;          // press F to toggle
    glm::vec3 velocity = glm::vec3(0.0f);
    bool onGround = false;

    // Player collision box (Minecraft-like proportions)
    float halfWidth = 0.3f;   // box is 0.6 wide/deep
    float eyeHeight = 1.6f;   // eye position relative to feet
    float playerHeight = 1.8f; // total box height, feet to top
};

// ==========================================================================
// PERLIN NOISE (Improved Perlin Noise, Ken Perlin 2002)
// ==========================================================================
class PerlinNoise {
public:
    explicit PerlinNoise(uint32_t seed) {
        for (int i = 0; i < 256; i++) p[i] = i;
        std::mt19937 gen(seed);
        std::shuffle(p.begin(), p.begin() + 256, gen);
        for (int i = 0; i < 256; i++) p[256 + i] = p[i];
    }

    // Returns noise in roughly [-1, 1]
    float noise2D(float x, float z) const {
        int X = (int)std::floor(x) & 255;
        int Z = (int)std::floor(z) & 255;

        float xf = x - std::floor(x);
        float zf = z - std::floor(z);

        float u = fade(xf);
        float v = fade(zf);

        int aa = p[p[X] + Z];
        int ab = p[p[X] + Z + 1];
        int ba = p[p[X + 1] + Z];
        int bb = p[p[X + 1] + Z + 1];

        float x1 = lerp(grad(aa, xf, zf), grad(ba, xf - 1, zf), u);
        float x2 = lerp(grad(ab, xf, zf - 1), grad(bb, xf - 1, zf - 1), u);

        return lerp(x1, x2, v);
    }

private:
    std::array<int, 512> p{};

    static float fade(float t) {
        return t * t * t * (t * (t * 6 - 15) + 10);
    }

    static float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    static float grad(int hash, float x, float z) {
        switch (hash & 7) {
        case 0: return  x + z;
        case 1: return  x - z;
        case 2: return -x + z;
        case 3: return -x - z;
        case 4: return  x;
        case 5: return -x;
        case 6: return  z;
        default: return -z;
        }
    }
};

// Fractal sum (fBm)
inline float fractalNoise2D(const PerlinNoise& pn, float x, float z,
    int octaves = 4, float baseFrequency = 0.05f,
    float persistence = 0.5f, float lacunarity = 2.0f) {
    float total = 0.0f;
    float frequency = baseFrequency;
    float amplitude = 1.0f;
    for (int i = 0; i < octaves; i++) {
        total += pn.noise2D(x * frequency, z * frequency) * amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
    }
    return total;
}

PerlinNoise terrainNoise(12345);
PerlinNoise treeNoise(54321);

// Computes the same baseHeight formula used in Chunk::generateTerrain(), so we
// can find a safe (above-ground) spot to spawn the player instead of guessing
// a fixed y value that may or may not be above the actual generated terrain.
int surfaceHeightAt(float worldX, float worldZ) {
    float heightValue = fractalNoise2D(terrainNoise, worldX, worldZ);
    int baseHeight = 25 + (int)(heightValue * 15);
    baseHeight = std::max(5, std::min(WORLD_HEIGHT - 10, baseHeight));
    return baseHeight;
}

// Block vertex data with normals for proper lighting
float blockVertices[] = {
    // Positions          // Normals         // TexCoords
    // Front face (Z+)
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,

    // Back face (Z-)
     0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f, 1.0f,

     // Left face (X-)
     -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
     -0.5f, -0.5f,  0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
     -0.5f,  0.5f,  0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 1.0f,
     -0.5f,  0.5f, -0.5f, -1.0f, 0.0f, 0.0f,  0.0f, 1.0f,

     // Right face (X+)
      0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
      0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f,
      0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f,

      // Top face (Y+)
      -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
       0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
       0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
      -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f,

      // Bottom face (Y-)
      -0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,  0.0f, 0.0f,
       0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,  1.0f, 0.0f,
       0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f,
      -0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f,  0.0f, 1.0f
};

unsigned int blockIndices[] = {
    0,  1,  2,   0,  2,  3,   // front
    4,  5,  6,   4,  6,  7,   // back
    8,  9,  10,  8,  10, 11,  // left
    12, 13, 14,  12, 14, 15,  // right
    16, 17, 18,  16, 18, 19,  // top
    20, 21, 22,  20, 22, 23   // bottom
};

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in float aBlockType;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out float BlockType;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    BlockType = aBlockType;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in float BlockType;

uniform vec3 viewPos;

void main() {
    vec3 baseColor;
    
    if (BlockType == 1.0) {
        baseColor = vec3(0.33, 0.7, 0.26);
    } else if (BlockType == 2.0) {
        baseColor = vec3(0.55, 0.36, 0.23);
    } else if (BlockType == 3.0) {
        baseColor = vec3(0.5, 0.5, 0.5);
    } else if (BlockType == 4.0) {
        baseColor = vec3(0.4, 0.27, 0.13);
    } else if (BlockType == 5.0) {
        baseColor = vec3(0.13, 0.55, 0.13);
    } else if (BlockType == 6.0) {
        baseColor = vec3(0.2, 0.4, 0.8);
    } else if (BlockType == 7.0) {
        baseColor = vec3(0.87, 0.78, 0.5);
    } else {
        baseColor = vec3(0.8, 0.8, 0.8);
    }
    
    float pattern = sin(TexCoord.x * 32.0) * sin(TexCoord.y * 32.0) * 0.05;
    baseColor += pattern;
    
    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.5));
    vec3 norm = normalize(Normal);
    
    float diff = max(dot(norm, lightDir), 0.0);
    float ambient = 0.5;
    
    float faceShading = 1.0;
    if (abs(norm.y) > 0.9) {
        faceShading = (norm.y > 0.0) ? 1.0 : 0.5;
    } else if (abs(norm.x) > 0.9) {
        faceShading = 0.8;
    } else {
        faceShading = 0.6;
    }
    
    float lighting = ambient + diff * 0.5;
    lighting *= faceShading;
    
    vec3 finalColor = baseColor * lighting;
    
    float fogStart = 40.0;
    float fogEnd = 80.0;
    float fogDistance = length(FragPos - viewPos);
    float fogFactor = clamp((fogEnd - fogDistance) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 fogColor = vec3(0.5, 0.75, 1.0);
    
    finalColor = mix(fogColor, finalColor, fogFactor);
    
    FragColor = vec4(finalColor, 1.0);
}
)";

int counter = 0;

// World/Chunk Management
class Chunk {
public:
    BlockType blocks[CHUNK_SIZE][WORLD_HEIGHT][CHUNK_SIZE];
    int chunkX, chunkZ;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int VAO, VBO, EBO;
    bool needsUpdate = true;

    Chunk(int x, int z) : chunkX(x), chunkZ(z) {
        generateTerrain();
        setupMesh();
    }

    void generateTerrain() {
        std::random_device rd;
        std::mt19937 gen(rd());

        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {

                int worldX = chunkX * CHUNK_SIZE + x;
                int worldZ = chunkZ * CHUNK_SIZE + z;

                float heightValue = fractalNoise2D(terrainNoise, (float)worldX, (float)worldZ);
                int baseHeight = 25 + (int)(heightValue * 15);
                baseHeight = std::max(5, std::min(WORLD_HEIGHT - 10, baseHeight));

                for (int y = 0; y < WORLD_HEIGHT; y++) {
                    if (y == 0) {
                        blocks[x][y][z] = STONE;
                    }
                    else if (y < baseHeight - 4) {
                        blocks[x][y][z] = STONE;
                    }
                    else if (y < baseHeight - 1) {
                        blocks[x][y][z] = DIRT;
                    }
                    else if (y == baseHeight - 1) {
                        blocks[x][y][z] = GRASS;
                    }
                    else {
                        if (blocks[x][y][z] != LEAVES) {
                            blocks[x][y][z] = AIR;
                        }
                    }
                }

                if (blocks[x][baseHeight - 1][z] == GRASS &&
                    x > 2 && x < CHUNK_SIZE - 3 && z > 2 && z < CHUNK_SIZE - 3) {

                    // NOTE: scaled by 0.1 so we sample *between* lattice points -
                    // gradient noise is exactly 0 at integer coordinates by construction,
                    // so calling noise2D(worldX, worldZ) directly always returned 0
                    // (this was silently preventing every tree from spawning).
                    float treeValue = treeNoise.noise2D((float)worldX * 0.1f, (float)worldZ * 0.1f);
                    if (treeValue > 0.3f) {
                        int treeHeight = 4 + (gen() % 2);

                        bool shouldPlaceTree = true;
                        int checkRadius = 6;

                        for (int checkX = x - checkRadius; checkX <= x + checkRadius; ++checkX) {
                            if (!shouldPlaceTree) break;
                            for (int checkZ = z - checkRadius; checkZ <= z + checkRadius; ++checkZ) {
                                if (checkX == x && checkZ == z) continue;

                                if (checkX >= 0 && checkX < CHUNK_SIZE &&
                                    checkZ >= 0 && checkZ < CHUNK_SIZE) {

                                    for (int checkY = baseHeight; checkY < baseHeight + treeHeight + 2; ++checkY) {
                                        if (checkY < WORLD_HEIGHT && blocks[checkX][checkY][checkZ] == WOOD) {
                                            shouldPlaceTree = false;
                                            break;
                                        }
                                    }
                                }
                            }
                        }

                        if (shouldPlaceTree) {
                            for (int y = baseHeight; y < baseHeight + treeHeight; y++) {
                                if (y < WORLD_HEIGHT) blocks[x][y][z] = WOOD;
                            }

                            for (int dy = baseHeight + treeHeight - 2; dy <= baseHeight + treeHeight + 1; ++dy) {
                                if (dy >= WORLD_HEIGHT) break;

                                int radius;
                                if (dy == baseHeight + treeHeight + 1) {
                                    radius = 0;
                                }
                                else if (dy == baseHeight + treeHeight) {
                                    radius = 1;
                                }
                                else {
                                    radius = 2;
                                }

                                for (int dx = -radius; dx <= radius; ++dx) {
                                    for (int dz = -radius; dz <= radius; ++dz) {
                                        int leafX = x + dx;
                                        int leafZ = z + dz;

                                        if (dx == 0 && dz == 0 && dy < baseHeight + treeHeight) {
                                            continue;
                                        }

                                        if (leafX >= 0 && leafX < CHUNK_SIZE &&
                                            leafZ >= 0 && leafZ < CHUNK_SIZE) {

                                            int distSq = dx * dx + dz * dz;
                                            if (distSq <= radius * radius) {
                                                blocks[leafX][dy][leafZ] = LEAVES;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    }

    bool isBlockSolid(int x, int y, int z) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= WORLD_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
            return false;
        }
        return blocks[x][y][z] != AIR;
    }

    void generateMesh() {
        vertices.clear();
        indices.clear();

        unsigned int indexOffset = 0;

        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                for (int z = 0; z < CHUNK_SIZE; z++) {
                    if (blocks[x][y][z] == AIR) continue;

                    glm::vec3 pos(chunkX * CHUNK_SIZE + x, y, chunkZ * CHUNK_SIZE + z);
                    BlockType currentBlock = blocks[x][y][z];

                    bool faces[6] = {
                        !isBlockSolid(x, y, z + 1),
                        !isBlockSolid(x, y, z - 1),
                        !isBlockSolid(x - 1, y, z),
                        !isBlockSolid(x + 1, y, z),
                        !isBlockSolid(x, y + 1, z),
                        !isBlockSolid(x, y - 1, z)
                    };

                    for (int face = 0; face < 6; face++) {
                        if (!faces[face]) continue;

                        for (int i = 0; i < 4; i++) {
                            int vertexIndex = face * 4 + i;
                            vertices.push_back(blockVertices[vertexIndex * 8 + 0] + pos.x);
                            vertices.push_back(blockVertices[vertexIndex * 8 + 1] + pos.y);
                            vertices.push_back(blockVertices[vertexIndex * 8 + 2] + pos.z);
                            vertices.push_back(blockVertices[vertexIndex * 8 + 3]);
                            vertices.push_back(blockVertices[vertexIndex * 8 + 4]);
                            vertices.push_back(blockVertices[vertexIndex * 8 + 5]);
                            vertices.push_back(blockVertices[vertexIndex * 8 + 6]);
                            vertices.push_back(blockVertices[vertexIndex * 8 + 7]);
                            vertices.push_back((float)currentBlock);
                        }

                        unsigned int faceIndices[] = { 0, 1, 2, 0, 2, 3 };
                        for (int i = 0; i < 6; i++) {
                            indices.push_back(indexOffset + faceIndices[i]);
                        }
                        indexOffset += 4;
                    }
                }
            }
        }

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(3);

        needsUpdate = false;
    }

    void render() {
        if (needsUpdate) {
            generateMesh();
        }

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    }

    ~Chunk() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }
};

// Game World
class World {
public:
    std::map<std::pair<int, int>, Chunk*> chunks;

    Chunk* getChunk(int chunkX, int chunkZ) {
        auto key = std::make_pair(chunkX, chunkZ);
        if (chunks.find(key) == chunks.end()) {
            chunks[key] = new Chunk(chunkX, chunkZ);
        }
        return chunks[key];
    }

    BlockType getBlock(int x, int y, int z) {
        if (y < 0 || y >= WORLD_HEIGHT) return AIR;

        int chunkX = x / CHUNK_SIZE;
        int chunkZ = z / CHUNK_SIZE;
        int localX = x % CHUNK_SIZE;
        int localZ = z % CHUNK_SIZE;

        if (localX < 0) { localX += CHUNK_SIZE; chunkX--; }
        if (localZ < 0) { localZ += CHUNK_SIZE; chunkZ--; }

        Chunk* chunk = getChunk(chunkX, chunkZ);
        return chunk->blocks[localX][y][localZ];
    }

    void setBlock(int x, int y, int z, BlockType block) {
        if (y < 0 || y >= WORLD_HEIGHT) return;

        int chunkX = x / CHUNK_SIZE;
        int chunkZ = z / CHUNK_SIZE;
        int localX = x % CHUNK_SIZE;
        int localZ = z % CHUNK_SIZE;

        if (localX < 0) { localX += CHUNK_SIZE; chunkX--; }
        if (localZ < 0) { localZ += CHUNK_SIZE; chunkZ--; }

        Chunk* chunk = getChunk(chunkX, chunkZ);
        chunk->blocks[localX][y][localZ] = block;
        chunk->needsUpdate = true;

        if (localX == 0) getChunk(chunkX - 1, chunkZ)->needsUpdate = true;
        if (localX == CHUNK_SIZE - 1) getChunk(chunkX + 1, chunkZ)->needsUpdate = true;
        if (localZ == 0) getChunk(chunkX, chunkZ - 1)->needsUpdate = true;
        if (localZ == CHUNK_SIZE - 1) getChunk(chunkX, chunkZ + 1)->needsUpdate = true;
    }

    void renderAroundPlayer(glm::vec3 playerPos) {
        positionX = playerPos.x;
        int playerChunkX = (int)floor(playerPos.x / CHUNK_SIZE);
        int playerChunkZ = (int)floor(playerPos.z / CHUNK_SIZE);

        for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
            for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                Chunk* chunk = getChunk(x, z);
                chunk->render();
            }
        }
    }

    ~World() {
        for (auto& pair : chunks) {
            delete pair.second;
        }
    }
};

// Shader Compilation
unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shader;
}

unsigned int createShaderProgram() {
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Input handling
Camera camera;
World world;
bool keys[1024];
bool firstMouse = true;
float lastX = 400, lastY = 300;
BlockType selectedBlock = STONE;

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) keys[key] = true;
        else if (action == GLFW_RELEASE) keys[key] = false;
    }

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) selectedBlock = GRASS;
        if (key == GLFW_KEY_2) selectedBlock = DIRT;
        if (key == GLFW_KEY_3) selectedBlock = STONE;
        if (key == GLFW_KEY_4) selectedBlock = WOOD;
        if (key == GLFW_KEY_5) selectedBlock = LEAVES;
        if (key == GLFW_KEY_6) selectedBlock = WATER;
        if (key == GLFW_KEY_7) selectedBlock = SAND;

        // Toggle flying / walking
        if (key == GLFW_KEY_F) {
            camera.flying = !camera.flying;
            camera.velocity = glm::vec3(0.0f);
        }
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    xoffset *= camera.sensitivity;
    yoffset *= camera.sensitivity;

    camera.yaw += xoffset;
    camera.pitch += yoffset;

    if (camera.pitch > 89.0f) camera.pitch = 89.0f;
    if (camera.pitch < -89.0f) camera.pitch = -89.0f;

    glm::vec3 direction;
    direction.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
    direction.y = sin(glm::radians(camera.pitch));
    direction.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
    camera.front = glm::normalize(direction);
    camera.right = glm::normalize(glm::cross(camera.front, glm::vec3(0, 1, 0)));
    camera.up = glm::normalize(glm::cross(camera.right, camera.front));
}

// Forward declaration - full definition (with the collision-box math) is below.
bool aabbCollidesAt(const glm::vec3& eyePos);

// Checks whether a 1x1x1 block at integer coords (bx,by,bz) would overlap the
// player's own collision box. Used to stop placement from embedding you in
// a solid block (e.g. placing directly under your own feet).
bool wouldOverlapPlayer(int bx, int by, int bz) {
    float feetY = camera.position.y - camera.eyeHeight;
    float minX = camera.position.x - camera.halfWidth;
    float maxX = camera.position.x + camera.halfWidth;
    float minY = feetY;
    float maxY = feetY + camera.playerHeight;
    float minZ = camera.position.z - camera.halfWidth;
    float maxZ = camera.position.z + camera.halfWidth;

    bool overlapX = ((float)bx < maxX) && ((float)(bx + 1) > minX);
    bool overlapY = ((float)by < maxY) && ((float)(by + 1) > minY);
    bool overlapZ = ((float)bz < maxZ) && ((float)(bz + 1) > minZ);
    return overlapX && overlapY && overlapZ;
}

// Safety net: if the player is ever found already overlapping solid geometry
// (from a placed block, an edge case in generation, etc.), nudge upward until
// clear. moveAndCollide alone has no way to recover from an already-embedded
// starting position - it only rejects moving further INTO something.
void resolveEmbeddedState() {
    int guard = 0;
    while (aabbCollidesAt(camera.position) && guard < 100) {
        camera.position.y += 0.05f;
        guard++;
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        glm::vec3 ray = camera.front;
        glm::vec3 pos = camera.position;

        for (float t = 0; t < 10.0f; t += 0.1f) {
            glm::vec3 testPos = pos + ray * t;
            int x = (int)floor(testPos.x);
            int y = (int)floor(testPos.y);
            int z = (int)floor(testPos.z);

            if (world.getBlock(x, y, z) != AIR) {
                if (button == GLFW_MOUSE_BUTTON_LEFT) {
                    world.setBlock(x, y, z, AIR);
                }
                else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                    glm::vec3 placePos = pos + ray * (t - 0.2f);
                    int px = (int)floor(placePos.x);
                    int py = (int)floor(placePos.y);
                    int pz = (int)floor(placePos.z);
                    if (world.getBlock(px, py, pz) == AIR && !wouldOverlapPlayer(px, py, pz)) {
                        world.setBlock(px, py, pz, selectedBlock);
                    }
                }
                break;
            }
        }
    }
}

// ==========================================================================
// COLLISION
//
// The player is treated as an axis-aligned box (AABB): 0.6 wide/deep,
// 1.8 tall, anchored so `eyePos` sits `eyeHeight` above the box's feet.
// aabbCollidesAt() tests whether that box (placed at a candidate eye
// position) overlaps any solid block in the world.
// ==========================================================================
bool aabbCollidesAt(const glm::vec3& eyePos) {
    float feetY = eyePos.y - camera.eyeHeight;

    float minX = eyePos.x - camera.halfWidth;
    float maxX = eyePos.x + camera.halfWidth;
    float minY = feetY;
    float maxY = feetY + camera.playerHeight;
    float minZ = eyePos.z - camera.halfWidth;
    float maxZ = eyePos.z + camera.halfWidth;

    // Shrink the test bounds slightly inward so floating-point error right at
    // a block boundary doesn't register a false collision with the block
    // you're standing flush against.
    const float epsilon = 0.001f;
    int x0 = (int)std::floor(minX + epsilon), x1 = (int)std::floor(maxX - epsilon);
    int y0 = (int)std::floor(minY + epsilon), y1 = (int)std::floor(maxY - epsilon);
    int z0 = (int)std::floor(minZ + epsilon), z1 = (int)std::floor(maxZ - epsilon);

    for (int bx = x0; bx <= x1; bx++) {
        for (int by = y0; by <= y1; by++) {
            for (int bz = z0; bz <= z1; bz++) {
                if (world.getBlock(bx, by, bz) != AIR) return true;
            }
        }
    }
    return false;
}

// Move by `delta`, resolving collisions one axis at a time so the player
// slides along walls/floors instead of getting stuck. Updates camera.position,
// camera.velocity (zeroing components that hit something), and camera.onGround.
void moveAndCollide(const glm::vec3& delta) {
    glm::vec3 pos = camera.position;

    // X axis
    pos.x += delta.x;
    if (aabbCollidesAt(pos)) {
        pos.x = camera.position.x;
        camera.velocity.x = 0.0f;
    }

    // Z axis
    pos.z += delta.z;
    if (aabbCollidesAt(pos)) {
        pos.z = camera.position.z;
        camera.velocity.z = 0.0f;
    }

    // Y axis (checked last; this is what sets onGround)
    pos.y += delta.y;
    if (aabbCollidesAt(pos)) {
        if (delta.y < 0.0f) camera.onGround = true; // landed on something below
        pos.y = camera.position.y;
        camera.velocity.y = 0.0f;
    }

    camera.position = pos;
}

void processInput(GLFWwindow* window, float deltaTime) {
    // Safety net: recover if somehow already overlapping solid geometry
    // (e.g. a block just got placed under our own feet).
    resolveEmbeddedState();

    // Horizontal input direction, projected onto the XZ plane (so looking
    // up/down doesn't speed up or slow down walking).
    glm::vec3 flatFront = camera.front;
    flatFront.y = 0.0f;
    if (glm::length(flatFront) > 0.0001f) flatFront = glm::normalize(flatFront);

    glm::vec3 flatRight = camera.right;
    flatRight.y = 0.0f;
    if (glm::length(flatRight) > 0.0001f) flatRight = glm::normalize(flatRight);

    glm::vec3 horizontalMove(0.0f);
    if (keys[GLFW_KEY_W]) horizontalMove += flatFront;
    if (keys[GLFW_KEY_S]) horizontalMove -= flatFront;
    if (keys[GLFW_KEY_A]) horizontalMove -= flatRight;
    if (keys[GLFW_KEY_D]) horizontalMove += flatRight;
    if (glm::length(horizontalMove) > 0.0001f) horizontalMove = glm::normalize(horizontalMove);

    if (camera.flying) {
        // Original no-collision flight behavior
        float velocity = camera.speed * deltaTime;
        camera.position += horizontalMove * velocity;
        if (keys[GLFW_KEY_SPACE]) camera.position += glm::vec3(0, 1, 0) * velocity;
        if (keys[GLFW_KEY_LEFT_SHIFT]) camera.position -= glm::vec3(0, 1, 0) * velocity;
    }
    else {
        // Walking mode: gravity + jump + collision
        camera.velocity.x = horizontalMove.x * camera.speed;
        camera.velocity.z = horizontalMove.z * camera.speed;

        camera.velocity.y += GRAVITY_ACCEL * deltaTime;
        if (camera.velocity.y < TERMINAL_VELOCITY) camera.velocity.y = TERMINAL_VELOCITY;

        if (keys[GLFW_KEY_SPACE] && camera.onGround) {
            camera.velocity.y = JUMP_SPEED;
            camera.onGround = false;
        }

        camera.onGround = false; // moveAndCollide sets this back to true if we land
        glm::vec3 delta = camera.velocity * deltaTime;

        // Substep the movement so a single large displacement (e.g. after a
        // frame hitch, or falling at high speed) can't skip clean over a
        // thin wall or floor between the old and new position - discrete
        // "test only the final position" collision would otherwise tunnel.
        const float maxStepDistance = 0.2f; // well under one block width
        float travelDistance = glm::length(delta);
        int numSteps = std::max(1, (int)std::ceil(travelDistance / maxStepDistance));
        glm::vec3 stepDelta = delta / (float)numSteps;

        for (int i = 0; i < numSteps; i++) {
            moveAndCollide(stepDelta);
        }
    }
}

// Main function
int main() {

    std::cout << "\n";
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1200, 2000, "Minecraft Clone - Enhanced", NULL, NULL);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (glewInit() != GLEW_OK) {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    unsigned int shaderProgram = createShaderProgram();

    // Spawn above the actual generated terrain surface at (0,0), rather than
    // a fixed y value that could land inside solid ground depending on what
    // the noise happens to produce there (baseHeight can range 5-54).
    int spawnSurfaceHeight = surfaceHeightAt(0.5f, 0.5f);
    camera.position = glm::vec3(0.5f, (float)spawnSurfaceHeight + 5.0f, 0.5f);

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    std::cout << "=== MINECRAFT CLONE - ENHANCED (Perlin noise + collision) ===" << std::endl;
    std::cout << "WASD: Move" << std::endl;
    std::cout << "Mouse: Look around" << std::endl;
    std::cout << "Space: Jump (walking) / Fly up (flying)" << std::endl;
    std::cout << "Shift: Fly down (flying mode only)" << std::endl;
    std::cout << "F: Toggle flying / walking" << std::endl;
    std::cout << "Left Click: Break block" << std::endl;
    std::cout << "Right Click: Place block" << std::endl;
    std::cout << "1-7: Select block type" << std::endl;
    std::cout << "ESC: Exit" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        // Clamp deltaTime so a debugger pause / hitch doesn't cause the
        // player to tunnel through the floor in one giant physics step.
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        processInput(window, deltaTime);

        glClearColor(0.5f, 0.75f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 projection = glm::perspective(glm::radians(100.0f), 1200.0f / 800.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.front, camera.up);
        glm::mat4 model = glm::mat4(1.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(camera.position));

        world.renderAroundPlayer(camera.position);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}