// ===================================================================================
// main.c - Visualizador de Modelos .obj em C (VERSÃO OPENGL LEGADO - COMPLETO)
//
// BIBLIOTECAS: freeglut, GLAD, Assimp (C-API), stb_image, cglm
//
// COMO COMPILAR (usando gcc no MSYS2/MinGW com pacotes do Pacman):
// gcc -o visualizador.exe main.c glad.c -lfreeglut -lopengl32 -lassimp -lgdi32 -lm
// NOTA: Adicionei glad.c ao comando de compilação, se você o tiver separado.
// ===================================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> // <-- ADICIONE AQUI

#include <glad/glad.h>
#include <GL/freeglut.h>
#include <GL/glu.h>

#include <cglm/cglm.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// --- Estruturas de Dados ---

typedef struct {
    vec3 position;
    vec3 normal;
    vec2 texCoords;
} Vertex;

typedef struct {
    unsigned int id;
    char* type;
    char* path;
} Texture;

typedef struct {
    Vertex* vertices;
    unsigned int numVertices;

    unsigned int* indices;
    unsigned int numIndices;

    Texture* textures;
    unsigned int numTextures;
    
    vec3 diffuseColor;
} Mesh;

typedef struct {
    Mesh* meshes;
    unsigned int numMeshes;

    char* directory;

    Texture* textures_loaded;
    unsigned int num_textures_loaded;
} Model;

// --- Variáveis Globais ---
int screen_width = 1920;
int screen_height = 1080;

// Câmera e Mouse (VERSÃO PRIMEIRA PESSOA)
vec3 cameraPos   = {30.0f, 4.0f, -10.0f}; // Posição dos "olhos" da professora. Sinta-se à vontade para ajustar!
vec3 cameraFront = {0.0f, 1.0f, 0.0f}; // Direção inicial para onde a câmera olha
vec3 cameraUp    = {0.0f, 1.0f, 0.0f};  // Vetor "para cima"

float cameraYaw = -90.0f; // Yaw inicial para olhar para o centro da sala (eixo -Z)
float cameraPitch = 0.0f;
int lastX, lastY;
int mouse_left_button_down = 0;

// NOVO: Variáveis para a mira automática da câmera
vec3 cameraTargetDirection; // A direção final para onde a câmera deve olhar
bool isCameraTurning = false; // Flag que controla se a câmera está no meio de uma virada

bool keyStates[256] = {false};

// NOVA FUNÇÃO: Chamada quando uma tecla é pressionada
void keyboardDown(unsigned char key, int x, int y) {
    keyStates[key] = true;
}

// NOVA FUNÇÃO: Chamada quando uma tecla é solta
void keyboardUp(unsigned char key, int x, int y) {
    keyStates[key] = false;
}

// NOVA FUNÇÃO: Processa o input do teclado a cada quadro
void processKeyboard() {
    float movementSpeed = 0.1f; // Ajuste para o personagem andar mais rápido ou mais devagar

    if (keyStates['w']) {
        vec3 move;
        glm_vec3_scale(cameraFront, movementSpeed, move);
        glm_vec3_add(cameraPos, move, cameraPos);
    }
    if (keyStates['s']) {
        vec3 move;
        glm_vec3_scale(cameraFront, movementSpeed, move);
        glm_vec3_sub(cameraPos, move, cameraPos);
    }
    if (keyStates['a']) {
        vec3 move, cameraRight;
        glm_vec3_cross(cameraFront, cameraUp, cameraRight);
        glm_vec3_normalize(cameraRight);
        glm_vec3_scale(cameraRight, movementSpeed, move);
        glm_vec3_sub(cameraPos, move, cameraPos);
    }
    if (keyStates['d']) {
        vec3 move, cameraRight;
        glm_vec3_cross(cameraFront, cameraUp, cameraRight);
        glm_vec3_normalize(cameraRight);
        glm_vec3_scale(cameraRight, movementSpeed, move);
        glm_vec3_add(cameraPos, move, cameraPos);
    }
}

Model* ourModel = NULL;

// NOVO: Variáveis para o Martelo e sua Animação
Model* hammerModel = NULL; // Guarda a geometria do martelo
float hammerAnimationAngle = 0.0f; // Ângulo atual da animação de batida
// Enum para controlar o estado da animação
typedef enum { IDLE, SWINGING_DOWN, SWINGING_UP } HammerState;
HammerState hammerState = IDLE;

// --- Protótipos ---
void renderScene(void);
void reshape(int width, int height);
void mouseButton(int button, int state, int x, int y);
void mouseMove(int x, int y);
void cleanup(void);
Model* Model_Create(const char* path);
void Model_Destroy(Model* model);
void Model_Draw(Model* model);
Mesh processMesh(struct aiMesh* mesh, const struct aiScene* scene, Model* model);
void processNode(struct aiNode* node, const struct aiScene* scene, Model* model);
unsigned int TextureFromFile(const char* path, const char* directory);
void loadMaterialTextures(struct aiMaterial* mat, enum aiTextureType type, const char* typeName, Mesh* outMesh, Model* model);


// ===================================================================================
// FUNÇÃO PRINCIPAL
// ===================================================================================
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <caminho_para_o_modelo.obj>\n", argv[0]);
        return -1;
    }

    // --- Inicialização do FreeGLUT ---
    glutInit(&argc, argv);
    glutInitContextVersion(2, 1);
    glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(screen_width, screen_height);
    glutCreateWindow("Visualizador Legado - Acerte os Arruaceiros!");

    // --- Inicialização do GLAD ---
    if (!gladLoadGL()) {
        fprintf(stderr, "Falha ao inicializar o GLAD\n");
        return -1;
    }
    
    printf("OpenGL Versão: %s\n", glGetString(GL_VERSION));

    // --- Configurações do OpenGL Legado ---
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glShadeModel(GL_SMOOTH);
    
    stbi_set_flip_vertically_on_load(1);
    ourModel = Model_Create(argv[1]);
    if (!ourModel) {
        fprintf(stderr, "Falha ao carregar o modelo.\n");
        return -1;
    }

    hammerModel = Model_Create("Power_Hammer.obj"); // <-- Mude o caminho se necessário
    if (!hammerModel) {
        fprintf(stderr, "Falha ao carregar o modelo do martelo.\n");
        return -1;
    }
    
    // --- Registro de Callbacks e Loop Principal ---
    glutDisplayFunc(renderScene);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMove);
    glutKeyboardFunc(keyboardDown);     // NOVO: Registra a função para tecla pressionada
    glutKeyboardUpFunc(keyboardUp);       // NOVO: Registra a função para tecla solta
    atexit(cleanup);

    glutMainLoop();

    return 0;
}

// ===================================================================================
// FUNÇÕES DE CALLBACK DO GLUT
// ===================================================================================
void renderScene(void) {
    processKeyboard();

    // 1. Atualiza a animação do martelo (lógica movida para o topo)
    float swingSpeed = 4.0f;
    if (hammerState == SWINGING_DOWN) {
        hammerAnimationAngle += swingSpeed;
        if (hammerAnimationAngle >= 90.0f) {
            hammerAnimationAngle = 90.0f;
            hammerState = SWINGING_UP;
        }
    } else if (hammerState == SWINGING_UP) {
        hammerAnimationAngle -= swingSpeed;
        if (hammerAnimationAngle <= 0.0f) {
            hammerAnimationAngle = 0.0f;
            hammerState = IDLE;
        }
    }

    // 2. Atualiza a mira da câmera se ela estiver virando
    if (isCameraTurning) {
        // Interpola suavemente a direção atual para a direção alvo
        float interpolationSpeed = 0.15f; // Ajuste para uma virada mais rápida ou lenta
        glm_vec3_lerp(cameraFront, cameraTargetDirection, interpolationSpeed, cameraFront);
        glm_vec3_normalize(cameraFront);

        // Atualiza os ângulos Yaw e Pitch a partir do novo vetor de direção
        cameraPitch = glm_deg(asin(cameraFront[1]));
        cameraYaw = glm_deg(atan2(cameraFront[2], cameraFront[0]));

        // Para a virada quando estiver perto o suficiente do alvo
        if (glm_vec3_distance(cameraFront, cameraTargetDirection) < 0.05f) {
            isCameraTurning = false;
        }
    }

    // Força o redesenho se a câmera ou o martelo estiverem se movendo
    if (isCameraTurning || hammerState != IDLE) {
        glutPostRedisplay();
    }

    glClearColor(0.2f, 0.3f, 0.5f, 1.0f); // Um azul céu
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- Matrizes de Projeção e Visualização ---
    mat4 projection, view;
    
    // 1. Matriz de Projeção (sem alterações)
    glm_perspective(glm_rad(45.0f), (float)screen_width / (float)screen_height, 0.1f, 1000.0f, projection);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((const GLfloat*)projection);

    // 2. LÓGICA DA CÂMERA EM PRIMEIRA PESSOA
    glMatrixMode(GL_MODELVIEW);

    if (!isCameraTurning) { // Só permite o controle livre do mouse se a câmera não estiver "mirando"
        vec3 front;
        front[0] = cos(glm_rad(cameraYaw)) * cos(glm_rad(cameraPitch));
        front[1] = sin(glm_rad(cameraPitch));
        front[2] = sin(glm_rad(cameraYaw)) * cos(glm_rad(cameraPitch));
        glm_vec3_normalize_to(front, cameraFront);
    }
    
    vec3 center;
    glm_vec3_add(cameraPos, cameraFront, center);
    glm_lookat(cameraPos, center, cameraUp, view);
    glLoadMatrixf((const GLfloat*)view);

    // --- Configuração da Luz (sem alterações) ---
    GLfloat light_position[] = { 5.0f, 10.0f, 5.0f, 0.0f };
    GLfloat light_ambient[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    GLfloat light_diffuse[] = { 1.0f, 1.0f, 0.9f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    
    // --- Transformações do Modelo (sem alterações) ---
    glScalef(1.0f, 1.0f, 1.0f);

    // --- Desenha o Modelo ---
    Model_Draw(ourModel);

    glClear(GL_DEPTH_BUFFER_BIT);

    glPushMatrix(); // Salva a matriz atual
    glLoadIdentity(); // Reseta a matriz para desenhar o martelo

    glTranslatef(0.4f, -0.3f, -5.0f); // Move para a posição final na tela

    // 5. APLICA A ANIMAÇÃO!
    //    Gira o martelo para baixo/cima em torno do eixo X (o eixo do "braço").
    glRotatef(hammerAnimationAngle, 1.0f, 0.0f, 0.0f);

    // 6. Escala o martelo para um tamanho razoável
    glScalef(0.3f, 0.3f, 0.3f);

    Model_Draw(hammerModel); // Desenha o martelo

    glPopMatrix(); // Restaura a matriz original

    glutSwapBuffers();
}

void reshape(int width, int height) {
    if (height == 0) height = 1;
    screen_width = width;
    screen_height = height;
    glViewport(0, 0, width, height);
}

void mouseButton(int button, int state, int x, int y) {
    lastX = x;
    lastY = y;
    if (button == GLUT_LEFT_BUTTON) {
        mouse_left_button_down = (state == GLUT_DOWN);
        
        // Inicia a animação e o raycasting ao pressionar o botão
        if (state == GLUT_DOWN) {
            // --- CÁLCULO DO ALVO (RAYCASTING) ---
            
            // 1. Pega as matrizes e o viewport atuais do OpenGL
            GLdouble modelview[16];
            GLdouble projection[16];
            GLint viewport[4];
            glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
            glGetDoublev(GL_PROJECTION_MATRIX, projection);
            glGetIntegerv(GL_VIEWPORT, viewport);

            // 2. Converte as coordenadas da janela para o sistema do OpenGL (Y invertido)
            float winY = (float)viewport[3] - (float)y;

            // 3. "Desprojeta" o clique para obter um raio 3D no mundo
            GLdouble near_x, near_y, near_z;
            GLdouble far_x, far_y, far_z;
            gluUnProject(x, winY, 0.0, modelview, projection, viewport, &near_x, &near_y, &near_z); // Ponto no plano próximo
            gluUnProject(x, winY, 1.0, modelview, projection, viewport, &far_x, &far_y, &far_z);   // Ponto no plano distante

            vec3 ray_origin = {(float)near_x, (float)near_y, (float)near_z};
            vec3 ray_far = {(float)far_x, (float)far_y, (float)far_z};
            
            vec3 ray_dir;
            glm_vec3_sub(ray_far, ray_origin, ray_dir);
            glm_vec3_normalize(ray_dir);

            // 4. Calcula a interseção do raio com o plano do chão (y=0)
            if (ray_dir[1] < -0.001f) { // Evita divisão por zero e raios que não vão para baixo
                float t = -ray_origin[1] / ray_dir[1];
                vec3 targetPoint;
                glm_vec3_scale(ray_dir, t, targetPoint);
                glm_vec3_add(ray_origin, targetPoint, targetPoint);

                // 5. Define a direção alvo para a câmera e inicia a virada
                glm_vec3_sub(targetPoint, cameraPos, cameraTargetDirection);
                glm_vec3_normalize(cameraTargetDirection);
                isCameraTurning = true;
            }

            // Inicia a animação do martelo
            if (hammerState == IDLE) {
                hammerState = SWINGING_DOWN;
            }
        }
    }
}

void mouseMove(int x, int y) {
    // Calcula o deslocamento do mouse desde a última vez
    int dx = x - lastX;
    int dy = lastY - y; // Invertido, pois as coordenadas Y da janela crescem para baixo
    lastX = x;
    lastY = y;

    // Apenas aplica a rotação se o botão esquerdo estiver pressionado
    if (mouse_left_button_down) {
        float sensitivity = 0.1f; // Sensibilidade pode ser ajustada
        cameraYaw += dx * sensitivity;
        cameraPitch += dy * sensitivity;

        // Limita a rotação vertical para não "virar de cabeça para baixo"
        if (cameraPitch > 89.0f) cameraPitch = 89.0f;
        if (cameraPitch < -89.0f) cameraPitch = -89.0f;
    }

    glutPostRedisplay(); // Solicita um redesenho da cena
}

void cleanup(void) {
    printf("Limpando recursos...\n");
    Model_Destroy(ourModel);
    Model_Destroy(hammerModel); // NOVO
}

// ===================================================================================
// NOVO BLOCO DE FUNÇÕES DE CARREGAMENTO (VERSÃO SEGURA E EFICIENTE)
// ===================================================================================

// Protótipos para as novas funções (coloque com os outros protótipos se preferir)
void processNode_pass1_count(struct aiNode* node, const struct aiScene* scene, unsigned int* meshCounter, unsigned int* textureCounter);
void processNode_pass2_fillData(struct aiNode* node, const struct aiScene* scene, Model* model, unsigned int* mesh_idx_counter);
void loadMaterialTextures_fillData(struct aiMaterial* mat, enum aiTextureType type, const char* typeName, Mesh* outMesh, Model* model);

// Passa 1: Percorre a cena para contar o total de malhas e texturas únicas
void processNode_pass1_count(struct aiNode* node, const struct aiScene* scene, unsigned int* meshCounter, unsigned int* textureCounter) {
    *meshCounter += node->mNumMeshes;
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        struct aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        if (mesh->mMaterialIndex >= 0) {
            struct aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            *textureCounter += aiGetMaterialTextureCount(mat, aiTextureType_DIFFUSE);
        }
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode_pass1_count(node->mChildren[i], scene, meshCounter, textureCounter);
    }
}

// Passa 2: Preenche os dados nas estruturas já alocadas
void processNode_pass2_fillData(struct aiNode* node, const struct aiScene* scene, Model* model, unsigned int* mesh_idx_counter) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        struct aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        model->meshes[*mesh_idx_counter] = processMesh(mesh, scene, model);
        (*mesh_idx_counter)++;
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode_pass2_fillData(node->mChildren[i], scene, model, mesh_idx_counter);
    }
}

// Nova versão de loadMaterialTextures que apenas preenche os dados
void loadMaterialTextures_fillData(struct aiMaterial* mat, enum aiTextureType type, const char* typeName, Mesh* outMesh, Model* model) {
    unsigned int texture_count_in_mat = aiGetMaterialTextureCount(mat, type);
    if (texture_count_in_mat == 0) return;

    outMesh->textures = (Texture*)malloc(texture_count_in_mat * sizeof(Texture));
    outMesh->numTextures = 0;

    for (unsigned int i = 0; i < texture_count_in_mat; i++) {
        struct aiString str;
        aiGetMaterialTexture(mat, type, i, &str, NULL, NULL, NULL, NULL, NULL, NULL);

        bool found = false;
        for (unsigned int j = 0; j < model->num_textures_loaded; j++) {
            if (strcmp(model->textures_loaded[j].path, str.data) == 0) {
                outMesh->textures[outMesh->numTextures++] = model->textures_loaded[j];
                found = true;
                break;
            }
        }
        if (!found) {
            Texture texture;
            texture.id = TextureFromFile(str.data, model->directory);
            texture.type = (char*)malloc(strlen(typeName) + 1);
            strcpy(texture.type, typeName);
            texture.path = (char*)malloc(strlen(str.data) + 1);
            strcpy(texture.path, str.data);
            
            outMesh->textures[outMesh->numTextures++] = texture;
            // A lista global é preenchida apenas uma vez, então não precisa de realloc
            model->textures_loaded[model->num_textures_loaded++] = texture;
        }
    }
}


// Nova função Model_Create que orquestra tudo
Model* Model_Create(const char* path) {
    printf("\n--- INICIANDO CARREGAMENTO DE: %s ---\n", path);
    Model* model = (Model*)malloc(sizeof(Model));
    if (!model) return NULL;
    model->meshes = NULL; model->numMeshes = 0; model->textures_loaded = NULL; model->num_textures_loaded = 0; model->directory = NULL;

    const struct aiScene* scene = aiImportFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        fprintf(stderr, "ERRO::ASSIMP:: %s\n", aiGetErrorString());
        free(model);
        return NULL;
    }
    
    // Extrai diretório
    const char* last_slash = strrchr(path, '/'); const char* last_backslash = strrchr(path, '\\');
    const char* final_separator = (last_slash > last_backslash) ? last_slash : last_backslash;
    if (final_separator) { size_t dir_len = final_separator - path; model->directory = (char*)malloc(dir_len + 1); strncpy(model->directory, path, dir_len); model->directory[dir_len] = '\0'; }
    else { model->directory = (char*)malloc(2); strcpy(model->directory, "."); }

    // --- NOVA LÓGICA DE ALOCAÇÃO EM PASSOS ---
    // 1. Contar tudo primeiro
    unsigned int totalMeshes = 0;
    unsigned int totalTextures = 0;
    processNode_pass1_count(scene->mRootNode, scene, &totalMeshes, &totalTextures);
    model->numMeshes = totalMeshes;

    // 2. Alocar memória de uma só vez
    model->meshes = (Mesh*)malloc(model->numMeshes * sizeof(Mesh));
    if (totalTextures > 0) {
        model->textures_loaded = (Texture*)malloc(totalTextures * sizeof(Texture));
    }

    // 3. Preencher os dados
    unsigned int mesh_index_counter = 0;
    model->num_textures_loaded = 0; // Será usado como contador
    processNode_pass2_fillData(scene->mRootNode, scene, model, &mesh_index_counter);
    
    aiReleaseImport(scene);
    printf("--- CARREGAMENTO DE %s CONCLUÍDO ---\n", path);
    return model;
}

Mesh processMesh(struct aiMesh* mesh, const struct aiScene* scene, Model* model) {
    printf("  [processMesh] Processando uma malha com %u vértices e %u faces.\n", mesh->mNumVertices, mesh->mNumFaces);
    time_t start_mesh, end_mesh;
    time(&start_mesh);

    Mesh newMesh = {0};
    
    time_t start_step = time(NULL);
    // Carrega Vértices
    newMesh.numVertices = mesh->mNumVertices;
    newMesh.vertices = (Vertex*)malloc(newMesh.numVertices * sizeof(Vertex));
    for (unsigned int i = 0; i < newMesh.numVertices; i++) {
        newMesh.vertices[i].position[0] = mesh->mVertices[i].x; newMesh.vertices[i].position[1] = mesh->mVertices[i].y; newMesh.vertices[i].position[2] = mesh->mVertices[i].z;
        if (mesh->mNormals) { newMesh.vertices[i].normal[0] = mesh->mNormals[i].x; newMesh.vertices[i].normal[1] = mesh->mNormals[i].y; newMesh.vertices[i].normal[2] = mesh->mNormals[i].z; }
        if (mesh->mTextureCoords[0]) { newMesh.vertices[i].texCoords[0] = mesh->mTextureCoords[0][i].x; newMesh.vertices[i].texCoords[1] = mesh->mTextureCoords[0][i].y; }
        else { glm_vec2_zero(newMesh.vertices[i].texCoords); }
    }
    printf("    - Vértices copiados em %.2f seg.\n", difftime(time(NULL), start_step));

    start_step = time(NULL);
    // Carrega Índices
    unsigned int total_indices = 0;
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) { total_indices += mesh->mFaces[i].mNumIndices; }
    newMesh.numIndices = total_indices;
    newMesh.indices = (unsigned int*)malloc(newMesh.numIndices * sizeof(unsigned int));
    unsigned int index_counter = 0;
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) { struct aiFace face = mesh->mFaces[i]; for (unsigned int j = 0; j < face.mNumIndices; j++) { newMesh.indices[index_counter++] = face.mIndices[j]; } }
    printf("    - Índices copiados em %.2f seg.\n", difftime(time(NULL), start_step));
    
    start_step = time(NULL);
    // Processa materiais
    if (mesh->mMaterialIndex >= 0) {
        struct aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        loadMaterialTextures_fillData(material, aiTextureType_DIFFUSE, "texture_diffuse", &newMesh, model);
        if (newMesh.numTextures == 0) {
            struct aiColor4D color;
            if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) { newMesh.diffuseColor[0] = color.r; newMesh.diffuseColor[1] = color.g; newMesh.diffuseColor[2] = color.b; }
            else { glm_vec3_one(newMesh.diffuseColor); }
        }
    }
    printf("    - Materiais processados em %.2f seg.\n", difftime(time(NULL), start_step));
    
    time(&end_mesh);
    printf("  [processMesh] Malha concluída em %.2f segundos.\n", difftime(end_mesh, start_mesh));
    return newMesh;
}

void Model_Draw(Model* model) {
    for (unsigned int i = 0; i < model->numMeshes; i++) {
        Mesh* currentMesh = &model->meshes[i];

        if (currentMesh->numTextures > 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, currentMesh->textures[0].id);
        } else {
            glDisable(GL_TEXTURE_2D);
            glColor3fv((const GLfloat*)currentMesh->diffuseColor);
        }

        glBegin(GL_TRIANGLES);
        for (unsigned int j = 0; j < currentMesh->numIndices; j++) {
            unsigned int vertexIndex = currentMesh->indices[j];
            
            if (currentMesh->numTextures > 0) {
                glTexCoord2fv((const GLfloat*)currentMesh->vertices[vertexIndex].texCoords);
            }
            glNormal3fv((const GLfloat*)currentMesh->vertices[vertexIndex].normal);
            glVertex3fv((const GLfloat*)currentMesh->vertices[vertexIndex].position);
        }
        glEnd();

        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Model_Destroy(Model* model) {
    if (!model) return;

    // Libera as malhas
    if (model->meshes) {
        for (unsigned int i = 0; i < model->numMeshes; i++) {
            if (model->meshes[i].vertices) free(model->meshes[i].vertices);
            if (model->meshes[i].indices) free(model->meshes[i].indices);
            if (model->meshes[i].textures) free(model->meshes[i].textures);
        }
        free(model->meshes);
    }

    // Libera as texturas carregadas
    if (model->textures_loaded) {
        for (unsigned int i = 0; i < model->num_textures_loaded; i++) {
            if (model->textures_loaded[i].path) free(model->textures_loaded[i].path);
            if (model->textures_loaded[i].type) free(model->textures_loaded[i].type);
        }
        free(model->textures_loaded);
    }

    // Libera o resto
    if (model->directory) free(model->directory);
    free(model);
}

unsigned int TextureFromFile(const char* path, const char* directory) {
    char filename[512];
    sprintf(filename, "%s/%s", directory, path);

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;
        else format = GL_RGB;

        glBindTexture(GL_TEXTURE_2D, textureID);
        // Em OpenGL legado, glTexImage2D é suficiente. Mipmaps podem ser gerados por GLU ou desativados.
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        
        // Configurações de textura mais simples para legado
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        printf("Textura carregada com sucesso: %s\n", filename);
    } else {
        fprintf(stderr, "Falha ao carregar textura: %s\n", filename);
    }
    stbi_image_free(data);
    return textureID;
}

void loadMaterialTextures(struct aiMaterial* mat, enum aiTextureType type, const char* typeName, Mesh* outMesh, Model* model) {
    unsigned int texture_count = aiGetMaterialTextureCount(mat, type);
    if (texture_count == 0) return;

    // Aloca memória para o array de texturas da malha de uma só vez
    outMesh->textures = (Texture*)malloc(texture_count * sizeof(Texture));
    outMesh->numTextures = 0; // Usaremos como um contador ao preencher

    for (unsigned int i = 0; i < texture_count; i++) {
        struct aiString str;
        aiGetMaterialTexture(mat, type, i, &str, NULL, NULL, NULL, NULL, NULL, NULL);

        bool skip = false;
        // Verifica se a textura já foi carregada no modelo
        for (unsigned int j = 0; j < model->num_textures_loaded; j++) {
            if (strcmp(model->textures_loaded[j].path, str.data) == 0) {
                // Se sim, apenas copia a struct, não carrega de novo
                outMesh->textures[outMesh->numTextures++] = model->textures_loaded[j];
                skip = true;
                break;
            }
        }

        if (!skip) {
            // Se não, é uma nova textura: carrega e adiciona à lista global do modelo
            Texture texture;
            texture.id = TextureFromFile(str.data, model->directory);
            texture.type = (char*)malloc(strlen(typeName) + 1);
            strcpy(texture.type, typeName);
            texture.path = (char*)malloc(strlen(str.data) + 1);
            strcpy(texture.path, str.data);
            
            // Adiciona à lista de texturas da malha
            outMesh->textures[outMesh->numTextures++] = texture;

            // Adiciona à lista global de texturas do modelo (usando realloc aqui é aceitável,
            // pois o número de texturas únicas geralmente é pequeno)
            model->num_textures_loaded++;
            model->textures_loaded = (Texture*)realloc(model->textures_loaded, model->num_textures_loaded * sizeof(Texture));
            model->textures_loaded[model->num_textures_loaded - 1] = texture;
        }
    }
}