// OpenGL/GLEW, GLFW, STL, and custom math-library dependencies.
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <map>
#include <sstream>
#include <cmath>
#include <string>
#include <random>
#include <fstream>
#include "gamemath.hpp"

// Numeric block identifiers stored in chunk data and passed to the shader.
enum BlockType {
	AIR = 0,
	STONE = 1,
	WOOD = 2,
	GRASS = 3,
	DIRT = 4,
	LEAVES = 5,
	WATER = 6,
	SAND = 7
};



// Two triangles used to draw one quad face of a cube.
unsigned int cubeIndices[6] = {
	0, 1, 3,
	0, 3, 2
};


// Camera state: world-space position and the basis vectors used for movement and view creation.
struct Camera {
	gamemath::vec3 position = { 0, 32, 0 }; 
	gamemath::vec3 front = { 0, 0, -1 };
	gamemath::vec3 up = { 0, 1, 0 };
	gamemath::vec3 right = { 1, 0, 0 };
	gamemath::vec3 worldUp = { 0, 1, 0 };
	float yaw = -90.0f;
	float pitch = 0.0f;
	float speed = 5.0f;
	float sensitivity = 0.1f;
};
Camera camera;

// Local-space cube geometry. Each vertex stores position (3 floats) and normal (3 floats).
float cubeVertices[]{
	//position						//normals												
	 0.5f,	 0.5f,	0.5f,			0.0f, 0.0f, 1.0f,	//front face
	 0.5f,	-0.5f,	0.5f,			0.0f, 0.0f, 1.0f,
	-0.5f,	 0.5f,	0.5f,			0.0f, 0.0f, 1.0f,
	-0.5f,	-0.5f,	0.5f,			0.0f, 0.0f, 1.0f,

	 0.5f,	 0.5f,	-0.5f,			0.0f, 0.0f, -1.0f,	//back face
	 0.5f,	-0.5f,	-0.5f,			0.0f, 0.0f, -1.0f,
	-0.5f,	 0.5f,	-0.5f,			0.0f, 0.0f, -1.0f,
	-0.5f,	-0.5f,	-0.5f,			0.0f, 0.0f, -1.0f,

	 0.5f,	0.5f,	 0.5f,			0.0f, 1.0f, 0.0f,	//top face
	 0.5f,	0.5f,	-0.5f,			0.0f, 1.0f, 0.0f,
	-0.5f,  0.5f,	 0.5f,			0.0f, 1.0f, 0.0f,
	-0.5f,  0.5f,	-0.5f,			0.0f, 1.0f, 0.0f,

	 0.5f, -0.5f,    0.5f,			0.0f, -1.0f, 0.0f,	//bottom face
	 0.5f, -0.5f,	-0.5f,			0.0f, -1.0f, 0.0f,
	-0.5f, -0.5f,	 0.5f,			0.0f, -1.0f, 0.0f,
	-0.5f, -0.5f,	-0.5f,			0.0f, -1.0f, 0.0f,

	0.5f,  0.5f,	 0.5f,			1.0f, 0.0f, 0.0f,	//right face
	0.5f, -0.5f,	 0.5f,			1.0f, 0.0f, 0.0f,
	0.5f,  0.5f,	-0.5f,			1.0f, 0.0f, 0.0f,
	0.5f, -0.5f,	-0.5f,			1.0f, 0.0f, 0.0f,

	-0.5f,  0.5f,	 0.5f,			-1.0f, 0.0f, 0.0f,	//left face
	-0.5f, -0.5f,	 0.5f,			-1.0f, 0.0f, 0.0f,
	-0.5f,  0.5f,	-0.5f,			-1.0f, 0.0f, 0.0f,
	-0.5f, -0.5f,	-0.5f,			-1.0f, 0.0f, 0.0f


};

// World/chunk configuration and the block type currently selected for placement.
const unsigned int faceSize = 24; //each face has 24 float values(including normal values too)
const int CHUNK_SIZE = 16;
const int WORLD_HEIGHT = 64;
const int RENDER_DISTANCE = 5;
BlockType selectedBlock = STONE;

//hashing function used for world coordinate values
float noise(int x, int z, int seed) {
	int n = x + z * 57 + seed * 131;
	n = (n << 13) ^ n;
	return 1.0 - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0;
}

float smoothNoise(float x, float z, int seed) {
	int intX = (int)std::floor(x);
	int intZ = (int)std::floor(z);
	
	//get the horizontal and vertical percentages in lattice cell
	float fracX = x - intX;
	float fracZ = z - intZ;

	//sample values at each corner of lattice cell
	float v1 = noise(intX,		intZ,		seed); //
	float v2 = noise(intX + 1,  intZ,		seed);
	float v3 = noise(intX,		intZ + 1,	seed);
	float v4 = noise(intX + 1,	intZ + 1,	seed);
	
	//calculate the influence all 4 corner values have on (fracx, fracz)
	float topLeft = v1 * (1 - fracX) * (1 - fracZ);
	float topRight = v2 * (fracX) * (1 - fracZ);
	float bottomLeft = v3 * (1 - fracX) * (fracZ);
	float bottomRight = v4 * (fracX) * (fracZ);

	//Bilinearly interpolate calculated values to approximate (fracx, fracz)
	float interpolatedHeightValue = topLeft + topRight + bottomLeft + bottomRight;
	return interpolatedHeightValue;
}

// Combines smooth-noise samples at increasing frequencies. Currently one octave is used.
float interpolatedNoise(int x, int z, int seed) {
	float frequency = 0.05f;
	float amplitude = 1.0f;
	float total = 0.0f;

	for (int i = 0; i < 1; ++i) {
		total += smoothNoise(x * frequency, z * frequency, seed) * amplitude;
		frequency *= 2.0f;
		amplitude *= 0.5f;
	}

	return total;
}

// Owns block data and the generated GPU mesh for one chunk.
class Chunk {
public:
	BlockType blocks[CHUNK_SIZE][WORLD_HEIGHT][CHUNK_SIZE];
	bool needsUpdate = true;
	Chunk(int cx, int cz) : chunkX(cx), chunkZ(cz) {
		generateTerrain();
		setupMesh();
	}


	// Fill this chunk with terrain layers and procedurally generated trees.
	void generateTerrain() {
		std::random_device rd;
		std::mt19937 gen(rd());

		// Generate one vertical terrain column for every local (x, z) position.
		for (int x = 0; x < CHUNK_SIZE; ++x) {
			for (int z = 0; z < CHUNK_SIZE; ++z) {

				// Convert local chunk coordinates to world coordinates for deterministic noise sampling.
				int worldX = chunkX * CHUNK_SIZE + x;
				int worldZ = chunkZ * CHUNK_SIZE + z;


				// Convert the noise value into a terrain height centered around y = 25.
				int baseHeight = 25 + (int)(interpolatedNoise(worldX, worldZ, 12345) * 15);
				baseHeight = std::max(std::min(baseHeight, WORLD_HEIGHT - 10), 5); //clamp height

				for (int y = 0; y < WORLD_HEIGHT; ++y) {
					if (y == 0) {
						blocks[x][y][z] = STONE; //bedrock
					}
					else if (y < baseHeight - 4) {
						blocks[x][y][z] = STONE;
					}
					else if (y < baseHeight - 1) {
						blocks[x][y][z] = DIRT;
					}
					else if (y < baseHeight) {
						blocks[x][y][z] = GRASS;
					}

					else if (blocks[x][y][z] != LEAVES) {
						blocks[x][y][z] = AIR;
					}
				}


				// Use a separate deterministic noise field to choose potential tree positions.
				float treeNoise = noise(worldX, worldZ, 54321);


				// Keep trees away from chunk edges so their leaves remain inside this chunk.
				if (treeNoise > 0.87 &&
					x > 2 && x < CHUNK_SIZE - 3 &&
					z > 2 && z < CHUNK_SIZE - 3) {
					// Reject this tree if another trunk is found inside the spacing radius.
					bool shouldPlace = true;
					int radius = 6;

					for (int checkX = -radius; checkX <= radius; ++checkX) {
						if (!shouldPlace) break;
						for (int checkZ = -radius; checkZ <= radius; ++checkZ) {
							if (checkX == 0 && checkZ == 0) continue;

							int newX = x + checkX;
							int newZ = z + checkZ;

							//searches 12 x 12 grid area for nearby trees
							if (newX >= 0 && newX < CHUNK_SIZE && newZ >= 0 && newZ < CHUNK_SIZE) {
								for (int checkY = baseHeight; checkY < WORLD_HEIGHT; ++checkY) {
									if (blocks[newX][checkY][newZ] == WOOD) {
										shouldPlace = false;
										break;
									}
								}
							}
						}
					}


					// No nearby trunk was found, so build the trunk and canopy.
					if (shouldPlace) {
						int treeHeight = 4 + (gen() % 2);

						//create tree trunk
						for (int y = baseHeight; y < baseHeight + treeHeight; ++y) {
							if (y >= 0 && y < WORLD_HEIGHT) {
								blocks[x][y][z] = WOOD;
							}
						}


						// Build four leaf layers that narrow from radius 2 to a single top block.
						int radius = 2;
						for (int dy = baseHeight + treeHeight - 2; dy < baseHeight + treeHeight + 2; ++dy) {
							if (dy >= WORLD_HEIGHT) break;

							if (dy < baseHeight + treeHeight) {
								radius = 2;
							}

							else if (dy < baseHeight + treeHeight + 1) {
								radius = 1;
							}
							else if (dy < baseHeight + treeHeight + 2) {
								radius = 0;
							}
							for (int dx = -radius; dx <= radius; ++dx) {
								for (int dz = -radius; dz <= radius; ++dz) {
									if (dx == 0 && dz == 0 && dy < baseHeight + treeHeight) continue;

									int leafX = x + dx;
									int leafZ = z + dz;
									if (leafX >= 0 && leafX < CHUNK_SIZE && leafZ >= 0 && leafZ < CHUNK_SIZE) {
										float dist = dx * dx + dz * dz;
										if (dist <= radius * radius) {
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

	// Create the VAO, VBO, and EBO used by this chunk.
	void setupMesh() {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
	}

	void generateMesh() {
		vertices.clear(); 
		indices.clear();
		//clear to avoid appending old values with new values if generateMesh is called more than once


		// Base vertex index for the next visible face appended to the mesh.
		unsigned int indexOffset = 0;

		for (int x = 0; x < CHUNK_SIZE; ++x) {
			for (int y = 0; y < WORLD_HEIGHT; ++y) {
				for (int z = 0; z < CHUNK_SIZE; ++z) {


					// Convert the block's local chunk coordinate to its world-space center.
					gamemath::vec3 pos(chunkX * CHUNK_SIZE + x, y, chunkZ * CHUNK_SIZE + z);
					if (blocks[x][y][z] == AIR) continue;

					BlockType block = blocks[x][y][z];


					// A face is visible only when the neighboring block in that direction is not solid.
					bool faces[6] = {
						!isBlockSolid(x, y, z + 1),		// front face
						!isBlockSolid(x, y, z - 1),		// back face
						!isBlockSolid(x, y + 1, z),		// top face
						!isBlockSolid(x, y - 1, z),		// bottom face
						!isBlockSolid(x + 1, y, z),		// right  face
						!isBlockSolid(x - 1, y, z)		//left face
					};

					unsigned int faceOffset = 0;

					// Append face vertices if visible
					for (int face = 0; face < 6; ++face) {
						if (!faces[face]) continue;


						// Jump to this face's 4 vertices inside cubeVertices.
						faceOffset = face * faceSize;
						for (int index = 0; index < 4; ++index) {
							vertices.push_back(cubeVertices[faceOffset + index * 6 + 0] + pos.x);
							vertices.push_back(cubeVertices[faceOffset + index * 6 + 1] + pos.y);
							vertices.push_back(cubeVertices[faceOffset + index * 6 + 2] + pos.z);
							vertices.push_back(cubeVertices[faceOffset + index * 6 + 3]);
							vertices.push_back(cubeVertices[faceOffset + index * 6 + 4]);
							vertices.push_back(cubeVertices[faceOffset + index * 6 + 5]);
							vertices.push_back((float)block);
						}


						// Reuse the same six local face indices, shifted to the newly appended vertices.
						for (int idx = 0; idx < 6; ++idx) {
							indices.push_back(cubeIndices[idx] + indexOffset);

						}
						indexOffset += 4;
					}

				}
			}
		}


		// Upload the rebuilt CPU-side vertex and index arrays to the GPU.
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);


		// Vertex layout: position(3), normal(3), block type(1) = 7 floats per vertex.
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (const void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (const void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (const void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		needsUpdate = false;
	}

	// Rebuild the mesh when block data changes, then draw the chunk.
	void render() {
		if (needsUpdate) {
			generateMesh();
		}


		// Upload the rebuilt CPU-side vertex and index arrays to the GPU.
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
	}


	// Out-of-chunk coordinates are treated as empty; AIR is the only non-solid block type here.
	bool isBlockSolid(int x, int y, int z) {
		if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= WORLD_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
			return false;
		}

		return blocks[x][y][z] != AIR;
	}

	~Chunk() {
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
	}
private:
	std::vector<float> vertices;
	std::vector<unsigned int> indices;
	unsigned int VBO, VAO, EBO;
	int chunkX, chunkZ;
};


// Stores chunks by integer coordinates and exposes world-space block access.
class World {
public:

	// Return an existing chunk or generate it the first time it is requested.
	Chunk* getChunk(int cx, int cz) {
		auto chunkCoord = std::make_pair(cx, cz);
		if (chunks.find(chunkCoord) == chunks.end()) {
			chunks[chunkCoord] = new Chunk(cx, cz);
		}
		
		return chunks[chunkCoord];
	}


	// Convert a world-space block coordinate into chunk/local coordinates and modify it.
	void setBlock(int x, int y, int z, BlockType block) {
		if (y < 0 || y >= WORLD_HEIGHT) return;

		int chunkX = (int)std::floor((float)x / CHUNK_SIZE);
		int chunkZ = (int)std::floor((float)z / CHUNK_SIZE);

		int localX = x % CHUNK_SIZE;
		int localZ = z % CHUNK_SIZE;

		if (localX < 0) localX += CHUNK_SIZE;
		if (localZ < 0) localZ += CHUNK_SIZE;

		Chunk* chunk = getChunk(chunkX, chunkZ);
		chunk->blocks[localX][y][localZ] = block;
		chunk->needsUpdate = true;
	}



	// Resolve a world-space block coordinate and return the block stored in its chunk.
	BlockType getBlock(int x, int y, int z) {
		if (y < 0 || y >= WORLD_HEIGHT) return AIR;

		int chunkX = (int)std::floor((float)x / CHUNK_SIZE);
		int chunkZ = (int)std::floor((float)z / CHUNK_SIZE);

		int localX = x % CHUNK_SIZE;
		int localZ = z % CHUNK_SIZE;

		if (localX < 0) localX += CHUNK_SIZE;
		if (localZ < 0) localZ += CHUNK_SIZE; 

		Chunk* chunk = getChunk(chunkX, chunkZ);

		return chunk->blocks[localX][y][localZ];
	}
	~World() {
		for (auto& chunk : chunks) {
			delete chunk.second;
		}
	}
private:
	std::map<std::pair<int, int>, Chunk* > chunks;
};
World world;

// Raycast from the camera when a mouse button is pressed to break or place a block.
void mouseButtonCallBack(GLFWwindow * window, int button, int action, int mod) {
	if (action == GLFW_PRESS) {
		// March up to 10 world units along the camera front vector in 0.1-unit steps.
		for (float t = 0.0f; t <= 10.0f; t+= 0.1) {
			gamemath::vec3 raycast = camera.position + camera.front * t;

			int x = (int)std::floor(raycast.x + 0.5f);
			int y = (int)std::floor(raycast.y + 0.5f);
			int z = (int)std::floor(raycast.z + 0.5f);


			// Stop at the first non-air block hit by the ray.
			if (world.getBlock(x, y, z) != AIR) {
				if (button == GLFW_MOUSE_BUTTON_LEFT) {
					world.setBlock(x, y, z, AIR);
				}

				else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
					gamemath::vec3 backtrackedRay = camera.position + camera.front * (t - 0.2);

					int px = (int)std::floor(backtrackedRay.x + 0.5f);
					int py = (int)std::floor(backtrackedRay.y + 0.5f);
					int pz = (int)std::floor(backtrackedRay.z + 0.5f);

					world.setBlock(px, py, pz, selectedBlock);
				}
				break;
			}
		}
	}
}

bool keys[1024];

// Track key press/release state and handle number-key block selection.
void keyCallBack(GLFWwindow * window, int key, int scancode, int action, int mod) {
	if (key >= 0 && key < 1024) {
		if (action == GLFW_PRESS) {
			keys[key] = true;
		}
		else if (action == GLFW_RELEASE) {
			keys[key] = false;
		}


		if (action == GLFW_PRESS) {
			if		(key == GLFW_KEY_1) selectedBlock = STONE;
			else if (key == GLFW_KEY_2) selectedBlock = WOOD;
			else if (key == GLFW_KEY_3) selectedBlock = GRASS;
			else if (key == GLFW_KEY_4) selectedBlock = DIRT;
			else if (key == GLFW_KEY_5) selectedBlock = WATER;
			else if (key == GLFW_KEY_6) selectedBlock = SAND ;
		}
	}
}


// Apply camera movement using the current key states.
void processInput(GLFWwindow* window, float deltaTime) {
	float velocity = deltaTime * camera.speed;
	
	if (keys[GLFW_KEY_ESCAPE]) {
		glfwSetWindowShouldClose(window, true);
	}

	if (keys[GLFW_KEY_W]) camera.position += camera.front * velocity;
	if (keys[GLFW_KEY_S]) camera.position -= camera.front * velocity;
	if (keys[GLFW_KEY_A]) camera.position -= camera.right * velocity;
	if (keys[GLFW_KEY_D]) camera.position += camera.right * velocity;

	if (keys[GLFW_KEY_SPACE]) camera.position += camera.up * velocity;
	if (keys[GLFW_KEY_LEFT_SHIFT]) camera.position -= camera.up * velocity;

}

bool isFirstMouse = true;
double lastMouseX, lastMouseY;

void mousePosCallBack(GLFWwindow* window, double xPos, double yPos) {

	// Initialize the previous cursor position so the first callback produces no jump.
	if (isFirstMouse) {
		lastMouseX = xPos;
		lastMouseY = yPos;
		isFirstMouse = false;
	}


	float deltaMouseX = static_cast<float>(xPos - lastMouseX);
	float deltaMouseY = static_cast<float>(lastMouseY - yPos);

	lastMouseX = xPos;
	lastMouseY = yPos;
	
	camera.yaw += deltaMouseX * camera.sensitivity;
	camera.pitch += deltaMouseY * camera.sensitivity;

	//Avoid gimbal lock at -90 and +90 degrees
	if (camera.pitch < -89.0f)  camera.pitch = -89.0f;
	if (camera.pitch > 89.0f)  camera.pitch = 89.0f;


	// Convert yaw and pitch into the camera's front direction.
	gamemath::vec3 direction(
		std::cos(gamemath::radians(camera.yaw)) * std::cos(gamemath::radians(camera.pitch)),
		std::sin(gamemath::radians(camera.pitch)),
		std::sin(gamemath::radians(camera.yaw)) * std::cos(gamemath::radians(camera.pitch))
	);


	// Rebuild a roll-free orthonormal basis using the fixed world-up direction.
	camera.front = gamemath::normalize(direction);
	camera.right = gamemath::normalize(gamemath::cross(camera.front, camera.worldUp));
	camera.up = gamemath::normalize(gamemath::cross(camera.right, camera.front));

}

// Find the player's current chunk and render a square region around it.
void renderAroundPlayer(gamemath::vec3 position) {
	int playerChunkX = (int)std::floor(position.x / CHUNK_SIZE);
	int playerChunkZ = (int)std::floor(position.z / CHUNK_SIZE);

	for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; ++x) {
		for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; ++z) {

			Chunk* chunk = world.getChunk(x, z);
			chunk->render();
		}
	}
}


// Loads a combined shader file, separates its vertex/fragment sections, compiles, and links them.
struct ShaderProgram {
	std::string filepath;
	enum class shader_type { VERTEX_SHADER = 0, FRAGMENT_SHADER = 1 };
	ShaderProgram(const std::string& s) : filepath(s) {
	}


	// Parse the shader file into two source strings using the custom section markers.
	unsigned int getShaderProgram() {
		std::ifstream file(filepath);
		if (!file.is_open()) {
			throw std::runtime_error("could not find shader file: " + filepath);
		}
		
		std::stringstream s[2];
		std::string line;
		shader_type shaderT;

		while (std::getline(file, line)) {
			if (line.find("#VERTEX SHADER") != std::string::npos) {
				shaderT = shader_type::VERTEX_SHADER;
			}

			else if (line.find("#FRAGMENT SHADER") != std::string::npos) {
				shaderT = shader_type::FRAGMENT_SHADER;
			}

			else {
				s[(int)shaderT] << line << '\n';
			}
		}
		return getProgram(s[0].str(), s[1].str());
	}


	// Compile both stages, link them into one program, and release the individual shader objects.
	unsigned int getProgram(const std::string& vertexShader, const std::string& fragmentShader) {
		unsigned int vs = compileShader(vertexShader, GL_VERTEX_SHADER);
		unsigned int fs = compileShader(fragmentShader, GL_FRAGMENT_SHADER);

		unsigned int program = glCreateProgram();

		glAttachShader(program, vs);
		glAttachShader(program, fs);
		glLinkProgram(program);

		int success;
		glGetProgramiv(program, GL_LINK_STATUS, &success);

		if (!success) {
			char infoLog[512];
			glGetProgramInfoLog(program, 512, NULL, infoLog);

			std::cerr << "ERROR LINKING SHADERS\n" << infoLog << '\n';
		}

		glDeleteShader(vs);
		glDeleteShader(fs);

		return program;
	}


	// Create one shader object, provide its GLSL source, compile it, and report errors.
	unsigned int compileShader(const std::string & shaderString, unsigned int shaderType) {
		unsigned int shader = glCreateShader(shaderType);

		const char* source = shaderString.c_str();
		glShaderSource(shader, 1, &source, NULL);
		glCompileShader(shader);

		int success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			char infoLog[512];
			glGetShaderInfoLog(shader, 512, NULL, infoLog);

			std::cerr << "SHADER ERROR FOR " << ((shaderType == GL_VERTEX_SHADER) ? "VERTEX SHADER" : "FRAGMENT SHADER") << '\n' << infoLog << '\n';

		}

		return shader;
	}
};



int main() {
	if (!glfwInit()) {
		std::cerr << "GLFW NOT INITIALIZED\n";
		return -1;
	}


	// Size the window from the primary monitor's current video mode.
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	int width = mode->width;
	int height = mode->height;


	GLFWwindow* window = glfwCreateWindow(width, height, "Minecraft-clone", NULL, NULL);

	if (!window) {
		std::cerr << "WINDOW NOT CREATED\n";
		glfwTerminate();
		return -1;
	}



	glfwMakeContextCurrent(window);


	// Register input callbacks and capture the cursor for FPS-style camera control.
	glfwSetKeyCallback(window, keyCallBack);
	glfwSetCursorPosCallback(window, mousePosCallBack);
	glfwSetMouseButtonCallback(window, mouseButtonCallBack);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	if (glewInit() != GLEW_OK) {
		std::cerr << "GLEW NOT INITIALIZED\n";
		glfwTerminate();
		return -1;
	}

	// Load, compile, and link the combined GLSL shader file.
	ShaderProgram shader("minecraft.shader");
	unsigned int program = shader.getShaderProgram();

	glEnable(GL_DEPTH_TEST);

	double startTime = glfwGetTime();


	// Main frame loop.
	while (!glfwWindowShouldClose(window)) {
		glUseProgram(program);

		glClearColor(0.5f, 0.75f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		double currentTime = glfwGetTime();
		float deltaTime = static_cast<float>(currentTime - startTime);

		startTime = currentTime;
		processInput(window, deltaTime);

		gamemath::mat4 model(1.0f);
		gamemath::mat4 projection = gamemath::perspective(gamemath::radians(90.0f), (float)width / height, 0.1f, 100.0f);
		gamemath::mat4 view = gamemath::lookAt(camera.position, camera.position + camera.front, camera.up);

		renderAroundPlayer(camera.position);


		// Send transformation matrices and camera position to the active shader program.
		glUniformMatrix4fv(glGetUniformLocation(program, "model"),		1, GL_FALSE, gamemath::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, gamemath::value_ptr(projection));
		glUniformMatrix4fv(glGetUniformLocation(program, "view"),		1, GL_FALSE, gamemath::value_ptr(view));
		glUniform3fv(glGetUniformLocation(program,		 "camPos"),		1,			 gamemath::value_ptr(camera.position));

		
		glfwSwapBuffers(window);
		glfwPollEvents(); 
	}


	// Release the linked shader program before shutting down GLFW.
	glDeleteProgram(program);
	glfwTerminate();
	return 0;


}