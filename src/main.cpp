#include "Common.h"
#include "GLM/fwd.hpp"
#include <cstddef>
#include <type_traits>

#define INIT_WIDTH 1600
#define INIT_HEIGHT 900
#define INIT_VIEWPORT_X 0
#define INIT_VIEWPORT_Y 0
#define INIT_VIEWPORT_WIDTH 1600
#define INIT_VIEWPORT_HEIGHT 900

using namespace glm;
using namespace std;

enum ModelShape
{
	Capsule,
	Cone,
	Cube,
	Cylinder,
	Plane,
	Sphere
};

enum ModelTexture
{
	TextureKuro,
	TextureHead,
	TextureTorso,
	TextureUpperarm,
	TextureForearm,
	TextureLThigh,
	TextureRThigh,
	TextureCalf,
	TextureHorn,
};

// Keyboard Pressing record for multiply key input
bool keyPressing[400] = {0};

// paramerter for walk animation
bool isStanding = true;
int walkEnabledCount = 0;
float walkTimerCount = 0;

// paramerter for sakana animation
float sakanaTimerCount = 0;
bool sakanaEnabled = false;
vec3 sakanaShiftVector = vec3(0.0f);
bool sakanaDone = true;

// gui
bool myGuiActive = true;


mat4 view(1.0f);					// V of MVP, viewing matrix
mat4 projection(1.0f);				// P of MVP, projection matrix

struct RotateType
{
	// Struct to represents how object should rotate
	float onX = 0; 
	float onZ = 0; 
	float onY = 0;
	RotateType() : onX(0), onZ(0), onY(0) {}
	RotateType(float x, float z, float y) : onX(x), onZ(z), onY(y) {}
};

RotateType cameraRotate = RotateType();

GLint um4p;
GLint um4mv;
GLint tex;

GLuint program;            // shader program id

struct Shape
{
	GLuint* gridVAO;
	GLuint* gridVBO;
	GLuint* robotVAO;            // vertex array object
	GLuint* robotVBO;            // vertex buffer object

	int materialId;
	int gridLenght;
	vector<int> vertexCounts;
	GLuint* m_texture;
};

Shape m_shape;

struct TextureData
{
	int width;
	int height;
	unsigned char* data;

	TextureData() : width(0), height(0), data(0) {}
};

TextureData loadImg(const char* path)
{
	TextureData texture;
	int n;
	stbi_set_flip_vertically_on_load(true);
	stbi_uc *data = stbi_load(path, &texture.width, &texture.height, &n, 4);
	if(data != NULL)
	{
		texture.data = new unsigned char[texture.width * texture.height * 4 * sizeof(unsigned char)];
		memcpy(texture.data, data, texture.width * texture.height * 4 * sizeof(unsigned char));
		stbi_image_free(data);
	}
	return texture;
}

struct ObjectData
{
	vector<float> vertices;
	vector<float> texcoords;
	vector<float> normals;

	ObjectData(vector<float> vect, vector<float> texc, vector<float> norm) : 
		vertices(vect), texcoords(texc), normals(norm) {}

	~ObjectData()
	{
		vertices.clear();
		vertices.shrink_to_fit();
		texcoords.clear();
		texcoords.shrink_to_fit();
		normals.clear();
		normals.shrink_to_fit();
	}
};


ObjectData loadObjectData(char* filename)
{
	tinyobj::attrib_t attrib;
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	string warn;
	string err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename);
	if (!warn.empty()) {
		cout << warn << endl;
	}
	if (!err.empty()) {
		cout << err << endl;
	}
	if (!ret) {
		exit(1);
	}
	
	vector<float> vertices, texcoords, normals; // if OBJ preserves vertex order, you can use element array buffer for memory efficiency
	int vertexCount = 0;                        // for 'ladybug.obj', there is only one object
	for (int s = 0; s < shapes.size(); ++s) {  
		int index_offset = 0;
		for (int f = 0; f < shapes[s].mesh.num_face_vertices.size(); ++f) {
			int fv = shapes[s].mesh.num_face_vertices[f];
			for (int v = 0; v < fv; ++v) {
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				vertices.push_back(attrib.vertices[3 * idx.vertex_index + 0]);
				vertices.push_back(attrib.vertices[3 * idx.vertex_index + 1]);
				vertices.push_back(attrib.vertices[3 * idx.vertex_index + 2]);
				texcoords.push_back(attrib.texcoords[2 * idx.texcoord_index + 0]);
				texcoords.push_back(attrib.texcoords[2 * idx.texcoord_index + 1]);
				normals.push_back(attrib.normals[3 * idx.normal_index + 0]);
				normals.push_back(attrib.normals[3 * idx.normal_index + 1]);
				normals.push_back(attrib.normals[3 * idx.normal_index + 2]);
			}
			index_offset += fv;
			vertexCount += fv;
		}
	}

	m_shape.vertexCounts.push_back(vertexCount);
	shapes.clear();
	shapes.shrink_to_fit();
	materials.clear();
	materials.shrink_to_fit();

	return ObjectData(vertices, texcoords, normals);
}

// Load shader file to program
char** loadShaderSource(const char* file)
{
	FILE* fp = fopen(file, "rb");
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *src = new char[size + 1];
	fread(src, sizeof(char), size, fp);
	src[size] = '\0';
	char **srcp = new char*[1];
	srcp[0] = src;
	return srcp;
}

// Free shader file
void freeShaderSource(char** srcp)
{
	delete srcp[0];
	delete srcp;
}

void loadGrid(int slices, float size)
{
	vector<vec3> vertices;
	vector<uvec4> indices;

	for(int j = 0; j <= slices; ++j) {
		for(int i = 0; i <= slices; ++i) {
			float x = (float)i / (float)slices * size - size / 2; 
			float y = 0;
			float z = (float)j / (float)slices * size - size / 2;
			vertices.push_back(vec3(x, y, z));
		}
	}

	for(int j = 0; j < slices; ++j) {
		for(int i = 0; i < slices; ++i) {
			int row1 =  j    * (slices + 1);
			int row2 = (j + 1) * (slices + 1);
			indices.push_back(uvec4(row1 + i, row1 + i + 1, row1 + i + 1, row2 + i + 1));
			indices.push_back(uvec4(row2 + i + 1, row2 + i, row2 + i, row1 + i));
		}
	}

	m_shape.gridVAO = new GLuint[1];
	glGenVertexArrays(1, m_shape.gridVAO);
	glBindVertexArray(m_shape.gridVAO[0]);

	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), value_ptr(vertices[0]), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	GLuint ibo;
	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo );
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(glm::uvec4), glm::value_ptr(indices[0]), GL_STATIC_DRAW);

	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	m_shape.gridLenght = (GLuint)indices.size()*4;
}

// Load .obj model
void loadModels()
{
	vector<ObjectData> objects;
	objects.push_back(loadObjectData((char*)("asset/model/Capsule.obj")));
	objects.push_back(loadObjectData((char*)("asset/model/Cone.obj")));
	objects.push_back(loadObjectData((char*)("asset/model/Cube.obj")));
	objects.push_back(loadObjectData((char*)("asset/model/Cylinder.obj")));
	objects.push_back(loadObjectData((char*)("asset/model/Plane.obj")));
	objects.push_back(loadObjectData((char*)("asset/model/Sphere.obj")));

	int objectsCount = objects.size();
	m_shape.robotVAO = new GLuint[objectsCount + 1];
	m_shape.robotVBO = new GLuint[objectsCount + 1];
	
	// Generate and bind VAO
	glGenVertexArrays(objectsCount, m_shape.robotVAO);
	glBindVertexArray(m_shape.robotVAO[0]);
	
	// Generate VBO
	glGenBuffers(objectsCount, m_shape.robotVBO);

	// Bind VBO[0]
	size_t offsSize = 0;
	for (int i = 0; i < objectsCount; ++i)
	{
		size_t vectSize = objects[i].vertices.size() * sizeof(float);
		size_t texcSize = objects[i].texcoords.size() * sizeof(float);
		size_t normSize = objects[i].normals.size() * sizeof(float);
		glBindVertexArray(m_shape.robotVAO[i]);
		glBindBuffer(GL_ARRAY_BUFFER, m_shape.robotVBO[i]);
		glBufferData(GL_ARRAY_BUFFER, vectSize + texcSize + normSize, NULL, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, vectSize, objects[i].vertices.data());
		glBufferSubData(GL_ARRAY_BUFFER, vectSize, texcSize, objects[i].texcoords.data());
		glBufferSubData(GL_ARRAY_BUFFER, vectSize + texcSize, normSize, objects[i].normals.data());
		
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)vectSize);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(vectSize + texcSize));
		glEnableVertexAttribArray(2);
		
		glBindVertexArray(0);
		offsSize += vectSize + texcSize + normSize;
		cout << "Load " << m_shape.vertexCounts[i] << " vertices" << endl;
	}
}

void loadTextures()
{
	vector<TextureData> textures;
	textures.push_back(loadImg("asset/texture/Kuro.png"));
	textures.push_back(loadImg("asset/texture/TakinaHead.png"));
	textures.push_back(loadImg("asset/texture/TakinaTorso.png"));
	textures.push_back(loadImg("asset/texture/TakinaUpperarm.png"));
	textures.push_back(loadImg("asset/texture/TakinaSkin.png"));
	textures.push_back(loadImg("asset/texture/TakinaLeftThigh.png"));
	textures.push_back(loadImg("asset/texture/TakinaRightThigh.png"));
	textures.push_back(loadImg("asset/texture/TakinaSkin.png"));
	textures.push_back(loadImg("asset/texture/TakinaCatear.png"));	

	int texturesCount = textures.size();

	m_shape.m_texture = new GLuint[texturesCount + 1];
	glGenTextures(texturesCount, m_shape.m_texture);

	for (int i = 0; i < texturesCount; ++i)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_shape.m_texture[i]);
		
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, textures[i].width, textures[i].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textures[i].data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}

// OpenGL initialization
void initialization()
{
	glViewport(INIT_VIEWPORT_X, INIT_VIEWPORT_Y, INIT_VIEWPORT_WIDTH, INIT_VIEWPORT_HEIGHT);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	// Create Shader Program
	program = glCreateProgram();

	// Create customize shader by tell openGL specify shader type
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Load shader file
	char **vertexShaderSource = loadShaderSource("asset/vertex.vs.glsl");
	char **fragmentShaderSource = loadShaderSource("asset/fragment.fs.glsl");

	// Assign content of these shader files to those shaders we created before
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);

	// Free the shader file string(won't be used any more)
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);

	// Compile these shaders
	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);

	// Logging #opt-debug
	shaderLog(vertexShader);
	shaderLog(fragmentShader);

	// Assign the program we created before with these shaders
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	// Get the id of inner variable 'um4p' and 'um4mv' in shader programs
	um4p = glGetUniformLocation(program, "um4p");
	um4mv = glGetUniformLocation(program, "um4mv");
	tex = glGetUniformLocation(program, "tex");

	// Tell OpenGL to use this shader program now
	glUseProgram(program);
   
	loadGrid(100, 50);
	loadModels();
	loadTextures();
	
	// perspective(fov, aspect_ratio, near_plane_distance, far_plane_distance)
	// Setting projection way.
	projection = perspective(radians(60.0f), (float)INIT_VIEWPORT_WIDTH / (float)INIT_VIEWPORT_HEIGHT, 0.1f, 1000.0f);

	// lookAt(camera_position, camera_viewing_vector, up_vector)
	// Setting Camera view
	view = lookAt(vec3(-10.0f * cos(radians(cameraRotate.onZ)), 5.0f, -10.0f * sin(radians(cameraRotate.onZ))), vec3(1.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
}

void drawGrid()
{
	glBindVertexArray(m_shape.gridVAO[0]);
	glUniform1i(tex, TextureKuro);
	// Transfer value of (view*model) to both shader's inner variable 'um4mv';
	glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(view * mat4(1.0f)));
	// Transfer value of projection to both shader's inner variable 'um4p';
	glUniformMatrix4fv(um4p, 1, GL_FALSE, value_ptr(projection));
	glDrawElements(GL_LINES, m_shape.gridLenght, GL_UNSIGNED_INT, NULL);
	glBindVertexArray(0);
}

class DrawObject
{
public:
	int shapeID;
	int textureID;
	RotateType rotate;
	RotateType initialRotate;
	vec3 shift = vec3(0.0f);
	vec3 scale;
	vec3 redirect;
	vec3 translate;
	mat4 translateMatrix = mat4(1.0f);
	mat4 scaleMatrix = mat4(1.0f);
	// mat4 relativeMatrix(1.0f);
	mat4 redirectMatrix = mat4(1.0f);
	mat4 rotateXMatrix = mat4(1.0f);
	mat4 rotateZMatrix = mat4(1.0f);
	mat4 rotateYMatrix = mat4(1.0f);
	mat4 rotateMatrix = mat4(1.0f);
	DrawObject* parentBase;

	DrawObject(int shape, int texture, vec3 scal, vec3 redi, vec3 tran, RotateType rota, DrawObject* parent) : 
		shapeID(shape), textureID(texture), scale(scal), redirect(redi), translate(tran), initialRotate(rota), rotate(rota), parentBase(parent) {}
	~DrawObject(){}

	void draw(int shapeID, int textureID, mat4 modelMatrix)
	{
		glBindVertexArray(m_shape.robotVAO[shapeID]);

		glUniform1i(tex, textureID);
		glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(view * modelMatrix));
		glUniformMatrix4fv(um4p, 1, GL_FALSE, value_ptr(projection));

		glDrawArrays(GL_TRIANGLES, 0, m_shape.vertexCounts[shapeID]);
	}

	void drawPass(int shapeID, int textureID, mat4 relativeMatrix)
	{
		this->redirectMatrix  = glm::translate(mat4(1.0f), this->redirect);
		this->rotateXMatrix   = glm::rotate(mat4(1.0f), radians(this->rotate.onX), vec3(1.0f, 0.0f, 0.0f));
		this->rotateYMatrix   = glm::rotate(mat4(1.0f), radians(this->rotate.onZ), vec3(0.0f, 1.0f, 0.0f));
		this->rotateZMatrix   = glm::rotate(mat4(1.0f), radians(this->rotate.onY), vec3(0.0f, 0.0f, 1.0f));
		this->rotateMatrix    = this->rotateYMatrix * this->rotateZMatrix * this->rotateXMatrix;
		this->translateMatrix = glm::translate(mat4(1.0f), this->shift + this->translate);
		relativeMatrix  = this->translateMatrix * this->rotateMatrix * this->redirectMatrix * relativeMatrix;
		
		if (this->parentBase != NULL)
			parentBase->drawPass(shapeID, textureID, relativeMatrix);
		else
			this->draw(shapeID, textureID, relativeMatrix);
	}

	void drawSelf()
	{
		this->scaleMatrix = glm::scale(mat4(1.0f), this->scale);
		this->drawPass(this->shapeID, this->textureID, this->scaleMatrix);
	}

	void reset()
	{
		this->shift = vec3(0.0f);
		this->rotate = initialRotate;
	}
};

DrawObject bodyDO = DrawObject(Cube, TextureTorso, 
	vec3(1.0f, 2.0f, 1.2f), vec3(0.0f), vec3(0.0f, 3.0f, 0.0f), RotateType(0.0f, 0.0f, 0.0f), NULL);
DrawObject headDO = DrawObject(Sphere, TextureHead, 
	vec3(1.0f, 1.0f, 1.0f), vec3(0.0f), vec3(0.0f, 1.5f, 0.0f), RotateType(), &bodyDO);
DrawObject leftHornDO = DrawObject(Cone, TextureHorn, 
	vec3(0.15f, 0.15f, 0.15f), vec3(0.0f, 0.15f, 0.0f), vec3(0.0f, 0.45f * cos(radians(30.0f)), 0.45f * sin(radians(30.0f))), RotateType(30.0f, 0.0f, 0.0f), &headDO);
DrawObject RigftHornDO = DrawObject(Cone, TextureHorn, 
	vec3(0.15f, 0.15f, 0.15f), vec3(0.0f, 0.15f, 0.0f), vec3(0.0f, 0.45f * cos(radians(-30.0f)), 0.45f * sin(radians(-30.0f))), RotateType(-30.0f, 0.0f, 0.0f), &headDO);
DrawObject leftUpperarmDO = DrawObject(Cube, TextureUpperarm, 
	vec3(0.5f, 1.0f, 0.5f), vec3(0.0f, -0.5f, 0.0f), vec3(0.0f, 1.0f, 0.85f), RotateType(), &bodyDO);
DrawObject leftForearmDO = DrawObject(Cube, TextureForearm, 
	vec3(0.5f, 1.0f, 0.5f), vec3(0.0f, -0.5f, 0.0f), vec3(0.0f, -0.5f, 0.0f), RotateType(), &leftUpperarmDO);
DrawObject rightUpperarmDO = DrawObject(Cube, TextureUpperarm, 
	vec3(0.5f, 1.0f, 0.5f), vec3(0.0f, -0.5f, 0.0f), vec3(0.0f, 1.0f, -0.85f), RotateType(), &bodyDO);
DrawObject rightForearmDO = DrawObject(Cube, TextureForearm, 
	vec3(0.5f, 1.0f, 0.5f), vec3(0.0f, -0.5f, 0.0f), vec3(0.0f, -0.5f, 0.0f), RotateType(), &rightUpperarmDO);
DrawObject leftThighDO = DrawObject(Cube, TextureLThigh, 
	vec3(0.5f, 1.0f, 0.5f), vec3(0.0f, -0.5f, 0.0f), vec3(0.0f, -1.0f, 0.35f), RotateType(), &bodyDO);
DrawObject leftCalfDO = DrawObject(Cube, TextureCalf, 
	vec3(0.5f, 1.0f, 0.5f), vec3(0.0f, -0.5f, 0.0f), vec3(0.0f, -0.5f, 0.0f), RotateType(), &leftThighDO);
DrawObject rightThighDO = DrawObject(Cube, TextureRThigh, 
	vec3(0.5f, 1.0f, 0.5f), vec3(0.0f, -0.5f, 0.0f), vec3(0.0f, -1.0f, -0.35f), RotateType(), &bodyDO);
DrawObject rightCalfDO = DrawObject(Cube, TextureCalf, 
	vec3(0.5f, 1.0f, 0.5f), vec3(0.0f, -0.5f, 0.0f), vec3(0.0f, -0.5f, 0.0f), RotateType(), &rightThighDO);

void drawRobot()
{
	bodyDO.drawSelf();
	headDO.drawSelf();
	leftHornDO.drawSelf();
	RigftHornDO.drawSelf();
	leftUpperarmDO.drawSelf();
	leftForearmDO.drawSelf();
	rightUpperarmDO.drawSelf();
	rightForearmDO.drawSelf();
	leftThighDO.drawSelf();
	leftCalfDO.drawSelf();
	rightThighDO.drawSelf();
	rightCalfDO.drawSelf();
}

bool robotMove()
{	
	float rotateSpeed = 5.4f;
	float walkSpeed = 0.18f / (float)walkEnabledCount;
	vec4 walkVector = walkSpeed * vec4(1.0f, 0.0f, 0.0f, 0.0f);
	vec3 tempBodyShift = bodyDO.shift;
	vec2 rotateVector = vec2(0.0f);
	if (keyPressing[GLFW_KEY_D])
	{
		rotateVector = rotateVector + vec2(sin(radians(cameraRotate.onZ)) / (float)walkEnabledCount, cos(radians(cameraRotate.onZ)) / (float)walkEnabledCount);
		bodyDO.shift = bodyDO.shift - vec3(bodyDO.rotateMatrix * walkVector);
	}
	if (keyPressing[GLFW_KEY_A])
	{
		rotateVector = rotateVector + vec2(sin(radians(cameraRotate.onZ + 180.0f)) / (float)walkEnabledCount, cos(radians(cameraRotate.onZ + 180.0f)) / (float)walkEnabledCount);
		bodyDO.shift = bodyDO.shift - vec3(bodyDO.rotateMatrix * walkVector);
	}
	if (keyPressing[GLFW_KEY_W])
	{
		rotateVector = rotateVector + vec2(sin(radians(cameraRotate.onZ + 270.0f)) / (float)walkEnabledCount, cos(radians(cameraRotate.onZ + 270.0f)) / (float)walkEnabledCount);
		bodyDO.shift = bodyDO.shift - vec3(bodyDO.rotateMatrix * walkVector);
	}
	if (keyPressing[GLFW_KEY_S])
	{
		rotateVector = rotateVector + vec2(sin(radians(cameraRotate.onZ + 90.0f)) / (float)walkEnabledCount, cos(radians(cameraRotate.onZ + 90.0f)) / (float)walkEnabledCount);
		bodyDO.shift = bodyDO.shift - vec3(bodyDO.rotateMatrix * walkVector);
	}

	if (length(rotateVector) == 0)
	{
		bodyDO.shift = tempBodyShift;
		return false;
	}
	else
	{
		float sinRotateZ = sin(radians(bodyDO.rotate.onZ) - atan(rotateVector.y, rotateVector.x));
		if (sinRotateZ > 0)
			bodyDO.rotate.onZ -= rotateSpeed;
		else
			bodyDO.rotate.onZ += rotateSpeed;
		return true;
	}
}

void animateWalk(bool is_walking)
{
	isStanding = false;
	float circle = 0.175f;
	float walkCircle = radians(walkTimerCount / circle);
	if (!is_walking)
	{
		float loopValueSin = sin(walkCircle);
		float loopValueCos = cos(walkCircle);
		if (abs(loopValueSin) <= 0.1)
		{
			walkTimerCount = 0;
			isStanding = true;
		}
		else if (loopValueSin * loopValueCos > 0)
			walkTimerCount -= 1.0f;
		else
			walkTimerCount += 1.0f;
	}
	else
		walkTimerCount += 1.0f;

	walkCircle = radians(walkTimerCount / circle);
	float moveRate = sin(walkCircle);
	float moveHigh = (sin(2.0f * walkCircle) + 1.0f) / 3.0f; 

	leftUpperarmDO.rotate.onY 	= 60.0f * moveRate;
	rightUpperarmDO.rotate.onY 	= -60.0f * moveRate;
	leftThighDO.rotate.onY 		= -60.0f * moveRate;
	rightThighDO.rotate.onY 	= 60.0f * moveRate;
	leftCalfDO.rotate.onY 		= abs(30.0f * moveRate);
	rightCalfDO.rotate.onY 		= abs(30.0f * moveRate);
	bodyDO.shift 				= vec3(bodyDO.shift.x , moveHigh, bodyDO.shift.z);

	if (isStanding)
	{
		leftForearmDO.rotate.onY = 0.0f;
		rightForearmDO.rotate.onY = 0.0f;
	}
	else
	{
		leftForearmDO.rotate.onY 	= -60.0f;
		rightForearmDO.rotate.onY 	= -60.0f;
	}
}

void animateSakana(float direct)
{
	sakanaTimerCount += direct * 1.0f;
	float circle = 10.0f;
	float moveRate = direct * 1.0f / circle;

	headDO.rotate.onZ 		   += moveRate * 60.0f;
	bodyDO.rotate.onY 		   += moveRate * 45.0f;
	bodyDO.shift 			   += moveRate * sakanaShiftVector;
	leftThighDO.rotate.onY     += moveRate * -45.0f;
	rightCalfDO.rotate.onY 	   += moveRate * 45.0f;
	leftUpperarmDO.rotate.onX  += moveRate * degrees(asin(3.0f/8.0f));
	leftUpperarmDO.rotate.onY  += moveRate * -135.0f;
	leftUpperarmDO.shift   	   += moveRate * vec3(0.0f, -0.25f, 0.0f);
	rightUpperarmDO.rotate.onX += moveRate * -degrees(asin(3.0f/8.0f));
	rightUpperarmDO.rotate.onY += moveRate * -135.0f;
	rightUpperarmDO.shift      += moveRate * vec3(0.0f, -0.25f, 0.0f);

	if (sakanaTimerCount == circle || sakanaTimerCount == 0.0f)
		sakanaDone = true;
}

void setCameraView()
{
	if (keyPressing[GLFW_KEY_LEFT])
		cameraRotate.onZ += 1.5f;
	if (keyPressing[GLFW_KEY_RIGHT])
		cameraRotate.onZ -= 1.5f;
	view = lookAt(vec3(-10.0f * cos(radians(cameraRotate.onZ)), 5.0f, -10.0f * sin(radians(cameraRotate.onZ))), vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
}

// Called to draw the scene.
void display()
{
	// Clear display buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	
	setCameraView();

	if (!sakanaDone)
	{
		if (isStanding)
		{
			if (sakanaEnabled)
				animateSakana(1.0f);
			else
				animateSakana(-1.0f);
		}
		else
			animateWalk(false);
	}
	else if (!(sakanaDone && sakanaEnabled))
	{
		bool is_walking = false;
		if (walkEnabledCount > 0)
			is_walking = robotMove();

		animateWalk(is_walking);
	}	
	
	// Tell openGL to use the shader program we created before
	glUseProgram(program);

	drawGrid();
	drawRobot();
}

// Setting up viewing matrix
void reshapeResponse(GLFWwindow *window, int width, int height)
{
	glViewport(0, 0, width, height);
	
	float viewportAspect = (float)width / (float)height;
	projection = perspective(radians(60.0f), viewportAspect, 0.1f, 1000.0f);
}

void resetObjects()
{
	cameraRotate.onZ = 0.0f;
	headDO.reset();
	bodyDO.reset();
	leftUpperarmDO.reset();
	leftForearmDO.reset();
	rightUpperarmDO.reset();
	rightForearmDO.reset();
	leftThighDO.reset();
	leftCalfDO.reset();
	rightThighDO.reset();
	rightCalfDO.reset();
	walkTimerCount = 0.0f;
	sakanaTimerCount = 0.0f;
	sakanaEnabled = false;
	sakanaDone = true;
}

void keyboardResponse(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	printf("Key %d is pressed\n", key);
	switch (key) {
		// Exit: Esc
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, true);
			break;
		// Robot transform control: W, A, S, D
		case GLFW_KEY_D:
			if (action == GLFW_PRESS)
			{
				keyPressing[GLFW_KEY_D] = true;
				walkEnabledCount++;
			}
			else if (action == GLFW_RELEASE)
			{
				keyPressing[GLFW_KEY_D] = false;
				walkEnabledCount--;
			}
			break;
		case GLFW_KEY_A:
			if (action == GLFW_PRESS)
			{
				keyPressing[GLFW_KEY_A] = true;
				walkEnabledCount++;
			}
			else if (action == GLFW_RELEASE)
			{
				keyPressing[GLFW_KEY_A] = false;
				walkEnabledCount--;
			}
			break;
		case GLFW_KEY_W:
			if (action == GLFW_PRESS)
			{
				keyPressing[GLFW_KEY_W] = true;
				walkEnabledCount++;
			}
			else if (action == GLFW_RELEASE)
			{
				keyPressing[GLFW_KEY_W] = false;
				walkEnabledCount--;
			}
			break;
		case GLFW_KEY_S:
			if (action == GLFW_PRESS)
			{
				keyPressing[GLFW_KEY_S] = true;
				walkEnabledCount++;
			}
			else if (action == GLFW_RELEASE)
			{
				keyPressing[GLFW_KEY_S] = false;
				walkEnabledCount--;
			}
			break;
		// Camera rotate : <- and ->
		case GLFW_KEY_LEFT:
			if (action == GLFW_PRESS)
			{
				keyPressing[GLFW_KEY_LEFT] = true;
			}
			else if (action == GLFW_RELEASE)
			{
				keyPressing[GLFW_KEY_LEFT] = false;
			}
			break;
		case GLFW_KEY_RIGHT:
			if (action == GLFW_PRESS)
			{
				keyPressing[GLFW_KEY_RIGHT] = true;
			}
			else if (action == GLFW_RELEASE)
			{
				keyPressing[GLFW_KEY_RIGHT] = false;
			}
			break;
		// Rebot state reset:F5
		case GLFW_KEY_F5:
			if (action == GLFW_PRESS) resetObjects();
			break;
		// Test key
		case GLFW_KEY_T:
			if (action == GLFW_PRESS)
			{
				if (!sakanaEnabled)
					sakanaShiftVector = vec3(bodyDO.rotateMatrix * vec4(vec3(-sin(radians(45.0f)), sin(radians(45.0f)) - 1, 0), 0.0f));
				sakanaEnabled = !sakanaEnabled;
				sakanaDone = false;
			}
			break;
		default:
			break;
	}
}	


void mouseResponse(GLFWwindow *window, int button, int action, int mods)
{
	double x, y;
	glfwGetCursorPos(window, &x, &y);
	
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			printf("Mouse %d is pressed at (%f, %f)\n", button, x, y);
		}
		else if (action == GLFW_RELEASE) {
			printf("Mouse %d is released at (%f, %f)\n", button, x, y);
		}
	}
}

void GUImenu()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowSize(ImVec2(150, 50));
	ImGui::SetNextWindowPos(ImVec2(20, 20));
	ImGui::Begin("Menu", &myGuiActive, ImGuiWindowFlags_MenuBar);
	if (ImGui::BeginMenuBar())
	{
	    if (ImGui::BeginMenu("AnimateSakana"))
	    {
	    	if (!sakanaEnabled)
	    	{
	    		if (ImGui::MenuItem("Start")) 
		        { 
		        	sakanaEnabled = !sakanaEnabled;
					sakanaDone = false;
					sakanaShiftVector = vec3(bodyDO.rotateMatrix * vec4(vec3(-sin(radians(45.0f)), sin(radians(45.0f)) - 1, 0), 0.0f));
		        }
	    	}
	    	else
	    	{
	    		if (ImGui::MenuItem("End")) 
		        { 
		        	sakanaEnabled = !sakanaEnabled;
					sakanaDone = false;
		        }
	    	}
	        ImGui::EndMenu();
	    }
	    
	    ImGui::EndMenuBar();
	}

	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int main(int argc, char **argv)
{
	// initial glfw
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// create window
	GLFWwindow* window = glfwCreateWindow(INIT_WIDTH, INIT_HEIGHT, "GPA2022_Assignment1", NULL, NULL);
	if (window == NULL)
	{
		cout << "Failed to create GLFW window" << endl;
		glfwTerminate();
		return -1;
	}
	// glfwSetWindowPos(window, 0, 0);
	glfwMakeContextCurrent(window);
	
	// setup imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	
	ImGui::StyleColorsDark();
	
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410 core");
	
	// register glfw callback functions
	glfwSetFramebufferSizeCallback(window, reshapeResponse);
	glfwSetKeyCallback(window, keyboardResponse);
	glfwSetMouseButtonCallback(window, mouseResponse);
	
	// load OpenGL function pointer
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		cout << "Failed to initialize GLAD" << endl;
		return -1;
	}

	// #opt-debug
	dumpInfo();

	initialization();

	// main loop
	while (!glfwWindowShouldClose(window))
	{
		// Poll input event
		glfwPollEvents();
				
		display();

		GUImenu();

		// swap buffer from back to front
		glfwSwapBuffers(window);
	}
	
	// cleanup imgui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	
	glfwDestroyWindow(window);
	glfwTerminate();
	
	// just for compatibiliy purposes
	return 0;
}
