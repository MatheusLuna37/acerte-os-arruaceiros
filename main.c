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

#include <glad/glad.h>
#include <GL/freeglut.h>

#include <cglm/cglm.h>
#include <stdbool.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <float.h>
#include <math.h>

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

// Câmera e Mouse
float cameraYaw = 90.0f;
float cameraPitch = 15.0f;
int lastX, lastY;
int mouse_left_button_down = 0;
int mouse_right_button_down = 0;

Model* ourModel = NULL;
Model* hammerModel = NULL; // modelo do martelo, usado como objeto em primeira pessoa
// --- Variáveis para câmera em primeira pessoa e martelo ---
vec3 cameraPos = {30.0f, 4.0f, -10.0f}; // posição dos olhos (ajuste conforme necessário)
vec3 cameraFront = {0.0f, 0.0f, -1.0f};
vec3 cameraUp = {0.0f, 1.0f, 0.0f};
bool isCameraTurning = false;
vec3 cameraTargetDirection; // direção alvo quando fazemos raycast

float hammerAnimationAngle = 0.0f; // pode ser usado para animação de batida
// Estados da animação do martelo
typedef enum { IDLE, SWINGING_DOWN, SWINGING_UP } HammerState;
HammerState hammerState = IDLE;

// teclado em tempo real
bool keyStates[256] = {false};
// --- Slot (ponto onde o "aluno" irá aparecer) ---
typedef struct {
    vec3 pos; // x, y, z (y já contém o topo da mesa + offset)
    int clicked; // 0 = não clicado, 1 = já clicado (contou ponto)
    int type; // 0..3 tipos diferentes de bonecos
} Slot;

Slot* slots = NULL;
unsigned int numSlots = 0;
int score = 0;

// --- Lógica do jogo (whack-a-mole) ---
int gameActive = 0; // 0 = inativo, 1 = rodando
int currentActive = -1; // índice do slot atualmente visível (-1 se nenhum)
int moleVisible = 0; // se o "mole"/slot está visível
unsigned int moleShowMs = 900; // tempo que o mole fica visível em ms
unsigned int moleIntervalMs = 400; // tempo entre moles em ms

// protótipos
void gameTick(int value);
void startGame();
void stopGame();

// Escolhe um slot aleatório e exibe por moleShowMs; agenda próxima chamada
void gameTick(int value) {
    if (!gameActive) return;
    // se atualmente visível, escondemos e agendamos intervalo curto antes do próximo
    if (moleVisible) {
        moleVisible = 0;
        currentActive = -1;
        // agenda próximo 'mole' após moleIntervalMs
        glutTimerFunc(moleIntervalMs, gameTick, 0);
    } else {
        // escolher um índice aleatório diferente do atual, se possível
        if (numSlots == 0) {
            // nada a fazer
            glutTimerFunc(moleIntervalMs, gameTick, 0);
            return;
        }
        int next = rand() % (int)numSlots;
        // tentar evitar repetir o mesmo índice repetidamente
        if (numSlots > 1 && next == currentActive) {
            next = (next + 1) % (int)numSlots;
        }
        currentActive = next;
        moleVisible = 1;
        // reset clicked para permitir novo acerto
        slots[currentActive].clicked = 0;
        // agendar esconder após moleShowMs
        glutTimerFunc(moleShowMs, gameTick, 0);
    }
    glutPostRedisplay();
}

void startGame() {
    if (gameActive) return;
    /* jogo iniciado */
    gameActive = 1;
    score = 0;
    currentActive = -1;
    moleVisible = 0;
    // iniciar primeira chamada
    glutTimerFunc(moleIntervalMs, gameTick, 0);
}

void stopGame() {
    if (!gameActive) return;
    /* jogo parado */
    gameActive = 0;
    currentActive = -1;
    moleVisible = 0;
    glutPostRedisplay();
}

// (TableCandidate and candidate-collection removed to keep code enxuto)

// Protótipos para slots
void addSlot(float centerX, float topY, float centerZ);
void drawSlot(float x, float z); // desenha o quadrado verde no centro da mesa (procura y internamente)
void drawBoneco(float x, float z);
void drawBonecoAtIndex(unsigned int idx);
// candidate-related functions removed
int loadSlotsFromFile(const char* path); // reimplementado para carregar 8 slots do arquivo

// Se true, desenha cubos (bonecos) nos slots; caso contrário, desenha quadrados verdes
int drawCubeMode = 0;

// Offset global para ajustar posição dos slots (útil para correção fina)
// Por padrão zero — usar apenas se precisar corrigir manualmente posições lidas de spots.txt
float slotOffsetX = 0.0f;
float slotOffsetZ = 0.0f;

// --- Protótipos ---
void renderScene(void);
void reshape(int width, int height);
void mouseButton(int button, int state, int x, int y);
void mouseMove(int x, int y);
void keyboardDown(unsigned char key, int x, int y);
void keyboardUp(unsigned char key, int x, int y);
void processKeyboard(void);
void cleanup(void);
Model* Model_Create(const char* path);
void Model_Destroy(Model* model);
void Model_Draw(Model* model);
Mesh processMesh(struct aiMesh* mesh, const struct aiScene* scene, Model* model);
void processNode(struct aiNode* node, const struct aiScene* scene, Model* model, const struct aiMatrix4x4* parentTransform);
unsigned int TextureFromFile(const char* path, const char* directory);
void loadMaterialTextures(struct aiMaterial* mat, enum aiTextureType type, const char* typeName, Mesh* outMesh, Model* model);

// -------------------------------------------------------------------------
// Implementação de slots (centros das mesas)
// -------------------------------------------------------------------------
void addSlot(float centerX, float topY, float centerZ) {
    numSlots++;
    slots = (Slot*)realloc(slots, numSlots * sizeof(Slot));
    slots[numSlots - 1].pos[0] = centerX;
    slots[numSlots - 1].pos[1] = topY; // já contém offset + 0.01
    slots[numSlots - 1].pos[2] = centerZ;
    slots[numSlots - 1].clicked = 0;
}

void addSlotWithType(float centerX, float topY, float centerZ, int type) {
    addSlot(centerX, topY, centerZ);
    if (numSlots > 0) slots[numSlots - 1].type = type % 4;
}

void drawSlot(float x, float z) {
    // desenha um pequeno quadrado verde centrado em (x, z) na altura y = slots[i].pos[1]
    float y = 0.01f; // fallback
    for (unsigned int i = 0; i < numSlots; i++) {
        if (fabsf(slots[i].pos[0] - x) < 0.0001f && fabsf(slots[i].pos[2] - z) < 0.0001f) {
            y = slots[i].pos[1];
            break;
        }
    }

    float half = 0.30f; // tamanho do quadrado (metade do lado) - aumentado para visibilidade
    // aplica offset global de ajuste fino
    x += slotOffsetX;
    z += slotOffsetZ;

    // Desenho em modo imediato (legacy)
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.0f, 0.9f, 0.0f);

    glBegin(GL_QUADS);
        glVertex3f(x - half, y, z - half);
        glVertex3f(x + half, y, z - half);
        glVertex3f(x + half, y, z + half);
        glVertex3f(x - half, y, z + half);
    glEnd();

    glPopAttrib();
}

void drawBoneco(float x, float z) {
    // procura o y do slot
    float y = 0.01f;
    for (unsigned int i = 0; i < numSlots; i++) {
        if (fabsf(slots[i].pos[0] - x) < 0.0001f && fabsf(slots[i].pos[2] - z) < 0.0001f) {
            y = slots[i].pos[1];
            break;
        }
    }

    // Parâmetros do boneco: tronco como cubo escalado e cabeça como esfera
    float trunkWidth = 0.9f;
    float trunkHeight = 1.4f; // maior altura para parecer tronco
    float trunkDepth = 0.6f;
    float headRadius = 0.35f;

    // aplica offset global de ajuste fino
    x += slotOffsetX;
    z += slotOffsetZ;

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    // determina cor do tronco baseada no tipo do slot (se encontrado)
    float trunkR = 0.8f, trunkG = 0.6f, trunkB = 0.3f; // default
    for (unsigned int i = 0; i < numSlots; i++) {
        if (fabsf(slots[i].pos[0] - (x - slotOffsetX)) < 0.0001f && fabsf(slots[i].pos[2] - (z - slotOffsetZ)) < 0.0001f) {
            int t = slots[i].type % 4;
            if (t == 0) { trunkR = 0.0f; trunkG = 0.9f; trunkB = 0.0f; } // verde
            else if (t == 1) { trunkR = 0.0f; trunkG = 0.0f; trunkB = 0.9f; } // azul
            else if (t == 2) { trunkR = 0.9f; trunkG = 0.0f; trunkB = 0.0f; } // vermelho
            else { trunkR = 0.05f; trunkG = 0.05f; trunkB = 0.05f; } // preto
            break;
        }
    }

    // Tronco: desenhado como um cubo escalado verticalmente
    glPushMatrix();
    glTranslatef(x, y + trunkHeight * 0.5f, z); // centraliza o tronco sobre o topo
    glColor3f(trunkR, trunkG, trunkB);
    glScalef(trunkWidth, trunkHeight, trunkDepth);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Cabeça: esfera posicionada acima do tronco
    glPushMatrix();
    glTranslatef(x, y + trunkHeight + headRadius, z);
    glutSolidSphere(headRadius, 16, 16);
    glPopMatrix();

    glPopAttrib();
}


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
    glutCreateWindow("Visualizador Legado - Movimento de Câmera");

    // --- Inicialização do GLAD ---
    if (!gladLoadGL()) {
        fprintf(stderr, "Falha ao inicializar o GLAD\n");
        return -1;
    }
    
    /* OpenGL versão (silenciado) */

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
    // Auto-load: se existir spots.txt na pasta do projeto, carregue automaticamente
    FILE* fspots = fopen("spots.txt", "r");
    if (fspots) {
        fclose(fspots);
        if (loadSlotsFromFile("spots.txt")) {
            /* spots.txt carregado automaticamente (silenciado) */
        }
    }
    
        // Tenta carregar o martelo a partir do mesmo diretório do modelo principal (mais robusto)
        char hammerPath[512];
        if (ourModel && ourModel->directory) {
            snprintf(hammerPath, sizeof(hammerPath), "%s/Power_Hammer.obj", ourModel->directory);
        } else {
            snprintf(hammerPath, sizeof(hammerPath), "Power_Hammer.obj");
        }
        hammerModel = Model_Create(hammerPath);
        if (!hammerModel) {
            fprintf(stderr, "Falha ao carregar o modelo do martelo (%s).\n", hammerPath);
            return -1;
        }
    
    // --- Registro de Callbacks e Loop Principal ---
    glutDisplayFunc(renderScene);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMove);
    // teclas normais (down/up)
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    atexit(cleanup);

    glutMainLoop();

    return 0;
}

// ===================================================================================
// FUNÇÕES DE CALLBACK DO GLUT
// ===================================================================================
void renderScene(void) {
    // Processa movimento do teclado antes de renderizar
    processKeyboard();

    // 1. Atualiza a animação do martelo
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

        // Força o redesenho se a câmera ou o martelo estiverem se movendo
        if (isCameraTurning || hammerState != IDLE) {
            glutPostRedisplay();
        }

    glClearColor(0.2f, 0.3f, 0.5f, 1.0f); // Um azul céu
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Configuração das matrizes usando a pilha de matrizes legada
    mat4 projection, view;
    
    // Matriz de Projeção
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

    // Configuração da luz no sistema legado
    GLfloat light_position[] = { 5.0f, 10.0f, 5.0f, 0.0f }; // w=0.0 para luz direcional
    GLfloat light_ambient[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    GLfloat light_diffuse[] = { 1.0f, 1.0f, 0.9f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    
    // Transformações do modelo (escala, rotação, etc.)
    glScalef(1.0f, 1.0f, 1.0f);

    // --- Desenha o Modelo ---
    Model_Draw(ourModel);

    // --- Desenha os quadradinhos (slots) no topo das mesas ---
    if (gameActive) {
        // desenhar apenas o slot ativo (se houver)
        if (currentActive >= 0 && (unsigned int)currentActive < numSlots && moleVisible) {
            if (drawCubeMode) drawBonecoAtIndex(currentActive);
            else drawSlot(slots[currentActive].pos[0], slots[currentActive].pos[2]);
        }
    } else {
        // modo livre: desenhar todos
        for (unsigned int i = 0; i < numSlots; i++) {
            if (drawCubeMode) drawBonecoAtIndex(i);
            else drawSlot(slots[i].pos[0], slots[i].pos[2]);
        }
    }

    // desenhar score no canto superior esquerdo
    char scoreStr[64];
    sprintf(scoreStr, "Score: %d", score);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, screen_width, 0, screen_height);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2i(10, screen_height - 20);
    for (char* c = scoreStr; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // --- Desenha o martelo em frente à câmera (primeira pessoa) ---
    glClear(GL_DEPTH_BUFFER_BIT);

    glPushMatrix(); // Salva a matriz atual
    glLoadIdentity(); // Reseta a matriz para desenhar o martelo

    // Posiciona o martelo na frente da câmera, menor e mais distante
    glTranslatef(0.4f, -0.3f, -5.0f); // Move para a posição final na tela (mais distante)

    // Aplica a animação simples de balanço (se usada)
    glRotatef(hammerAnimationAngle, 1.0f, 0.0f, 0.0f);

    // Escala menor para não ficar grande demais
    glScalef(0.25f, 0.25f, 0.25f);

    if (hammerModel) Model_Draw(hammerModel); // Desenha o martelo

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
        // detectar clique em slot quando botão é pressionado
        if (state == GLUT_DOWN) {
            // use ray picking (gluUnProject) to find the nearest slot under the mouse
            GLdouble model[16], proj[16];
            GLint view[4];
            glGetDoublev(GL_MODELVIEW_MATRIX, model);
            glGetDoublev(GL_PROJECTION_MATRIX, proj);
            glGetIntegerv(GL_VIEWPORT, view);
            GLdouble winX = (GLdouble)x;
            GLdouble winY = (GLdouble)(view[3] - y);
            GLdouble nearX, nearY, nearZ, farX, farY, farZ;
            if (gluUnProject(winX, winY, 0.0, model, proj, view, &nearX, &nearY, &nearZ) == GL_FALSE) {
                // fallback: nada
            } else if (gluUnProject(winX, winY, 1.0, model, proj, view, &farX, &farY, &farZ) == GL_FALSE) {
                // fallback: nada
            } else {
                double dirX = farX - nearX;
                double dirY = farY - nearY;
                double dirZ = farZ - nearZ;
                double dirLen = sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
                if (dirLen > 1e-6) {
                    dirX /= dirLen; dirY /= dirLen; dirZ /= dirLen;
                    int bestIdx = -1;
                    double bestT = 1e30;
                    double worldThreshold = 0.9; // tolerância em unidades de mundo (ajuste fino)
                    for (unsigned int i = 0; i < numSlots; i++) {
                        // se jogo ativo, considere apenas o slot ativo
                        if (gameActive && ((int)i != currentActive || !moleVisible)) continue;
                        double sx = (double)slots[i].pos[0];
                        double sy = (double)slots[i].pos[1];
                        double sz = (double)slots[i].pos[2];
                        // vector from near to slot
                        double wx = sx - nearX;
                        double wy = sy - nearY;
                        double wz = sz - nearZ;
                        double t = wx*dirX + wy*dirY + wz*dirZ; // projection onto ray
                        if (t < 0) continue; // behind the camera
                        // closest point
                        double cx = nearX + dirX * t;
                        double cy = nearY + dirY * t;
                        double cz = nearZ + dirZ * t;
                        double dx2 = sx - cx;
                        double dy2 = sy - cy;
                        double dz2 = sz - cz;
                        double dist2 = dx2*dx2 + dy2*dy2 + dz2*dz2;
                        if (dist2 <= worldThreshold*worldThreshold) {
                            if (t < bestT) {
                                bestT = t;
                                bestIdx = (int)i;
                            }
                        }
                    }
                    if (bestIdx >= 0) {
                        unsigned int i = (unsigned int)bestIdx;
                        if (gameActive) {
                            if (!slots[i].clicked) {
                                slots[i].clicked = 1;
                                int ttype = slots[i].type % 4;
                                int points = (ttype == 0) ? 1 : (ttype == 1) ? 2 : (ttype == 2) ? -1 : 4;
                                score += points;
                                moleVisible = 0;
                            }
                        } else {
                            if (!slots[i].clicked) {
                                slots[i].clicked = 1;
                                int ttype = slots[i].type % 4;
                                int points = (ttype == 0) ? 1 : (ttype == 1) ? 2 : (ttype == 2) ? -1 : 4;
                                score += points;
                            }
                        }
                    }
                }
            }
            // Inicia a animação do martelo ao pressionar o botão esquerdo
            if (hammerState == IDLE) {
                hammerState = SWINGING_DOWN;
            }
        }
    }
    if (button == GLUT_RIGHT_BUTTON) {
        mouse_right_button_down = (state == GLUT_DOWN);
    }
}

void mouseMove(int x, int y) {
    int dx = x - lastX;
    int dy = lastY - y;
    lastX = x;
    lastY = y;

    if (mouse_left_button_down) {
        float sensitivity = 0.2f;
        cameraYaw += dx * sensitivity;
        cameraPitch += dy * sensitivity;

        if (cameraPitch > 89.0f) cameraPitch = 89.0f;
        if (cameraPitch < -89.0f) cameraPitch = -89.0f;
    }
    // right button currently unused in first-person mode

    glutPostRedisplay();
}

// Teclado: registramos estado de teclas para movimento contínuo
void keyboardDown(unsigned char key, int x, int y) {
    if ((unsigned char)key < 256) keyStates[(unsigned char)key] = true;
    if (key == 'b' || key == 'B') {
        if (gameActive) stopGame(); else startGame();
    } else if (key == 'v' || key == 'V') {
    drawCubeMode = !drawCubeMode;
    /* modo boneco alternado */
    } else if (key == 'O') {
        if (!loadSlotsFromFile("spots.txt")) {
            /* falha ao carregar spots.txt (silenciado) */
        } else {
            /* slots carregados com sucesso (silenciado) */
        }
    }
}

void keyboardUp(unsigned char key, int x, int y) {
    if ((unsigned char)key < 256) keyStates[(unsigned char)key] = false;
}

void processKeyboard() {
    float movementSpeed = 0.1f; // Ajuste para o personagem andar mais rápido ou mais devagar

    if (keyStates['w'] || keyStates['W']) {
        vec3 move;
        glm_vec3_scale(cameraFront, movementSpeed, move);
        glm_vec3_add(cameraPos, move, cameraPos);
    }
    if (keyStates['s'] || keyStates['S']) {
        vec3 move;
        glm_vec3_scale(cameraFront, movementSpeed, move);
        glm_vec3_sub(cameraPos, move, cameraPos);
    }
    if (keyStates['a'] || keyStates['A']) {
        vec3 move, cameraRight;
        glm_vec3_cross(cameraFront, cameraUp, cameraRight);
        glm_vec3_normalize(cameraRight);
        glm_vec3_scale(cameraRight, movementSpeed, move);
        glm_vec3_sub(cameraPos, move, cameraPos);
    }
    if (keyStates['d'] || keyStates['D']) {
        vec3 move, cameraRight;
        glm_vec3_cross(cameraFront, cameraUp, cameraRight);
        glm_vec3_normalize(cameraRight);
        glm_vec3_scale(cameraRight, movementSpeed, move);
        glm_vec3_add(cameraPos, move, cameraPos);
    }

    // request redraw when moving
    if (keyStates['w'] || keyStates['a'] || keyStates['s'] || keyStates['d'] || keyStates['W'] || keyStates['A'] || keyStates['S'] || keyStates['D']) {
        glutPostRedisplay();
    }
}


void cleanup(void) {
    printf("Limpando recursos...\n");
    Model_Destroy(ourModel);
    Model_Destroy(hammerModel);
}

// ===================================================================================
// FUNÇÕES DE CARREGAMENTO E DESENHO
// ===================================================================================

Model* Model_Create(const char* path) {
    Model* model = (Model*)malloc(sizeof(Model));
    if (!model) return NULL;
    
    model->meshes = NULL;
    model->numMeshes = 0;
    model->textures_loaded = NULL;
    model->num_textures_loaded = 0;
    model->directory = NULL;

    const struct aiScene* scene = aiImportFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        fprintf(stderr, "ERRO::ASSIMP:: %s\n", aiGetErrorString());
        free(model);
        return NULL;
    }
    
    const char* last_slash = strrchr(path, '/');
    const char* last_backslash = strrchr(path, '\\');
    const char* final_separator = (last_slash > last_backslash) ? last_slash : last_backslash;

    if (final_separator) {
        size_t dir_len = final_separator - path;
        model->directory = (char*)malloc(dir_len + 1);
        strncpy(model->directory, path, dir_len);
        model->directory[dir_len] = '\0';
    } else {
        model->directory = (char*)malloc(2);
        strcpy(model->directory, ".");
    }

    // start with identity parent transform
    struct aiMatrix4x4 identity;
    aiIdentityMatrix4(&identity);
    processNode(scene->mRootNode, scene, model, &identity);

    aiReleaseImport(scene);

    return model;
}

void aiMultiplyMat4(const struct aiMatrix4x4* a, const struct aiMatrix4x4* b, struct aiMatrix4x4* out) {
    out->a1 = a->a1*b->a1 + a->a2*b->b1 + a->a3*b->c1 + a->a4*b->d1;
    out->a2 = a->a1*b->a2 + a->a2*b->b2 + a->a3*b->c2 + a->a4*b->d2;
    out->a3 = a->a1*b->a3 + a->a2*b->b3 + a->a3*b->c3 + a->a4*b->d3;
    out->a4 = a->a1*b->a4 + a->a2*b->b4 + a->a3*b->c4 + a->a4*b->d4;

    out->b1 = a->b1*b->a1 + a->b2*b->b1 + a->b3*b->c1 + a->b4*b->d1;
    out->b2 = a->b1*b->a2 + a->b2*b->b2 + a->b3*b->c2 + a->b4*b->d2;
    out->b3 = a->b1*b->a3 + a->b2*b->b3 + a->b3*b->c3 + a->b4*b->d3;
    out->b4 = a->b1*b->a4 + a->b2*b->b4 + a->b3*b->c4 + a->b4*b->d4;

    out->c1 = a->c1*b->a1 + a->c2*b->b1 + a->c3*b->c1 + a->c4*b->d1;
    out->c2 = a->c1*b->a2 + a->c2*b->b2 + a->c3*b->c2 + a->c4*b->d2;
    out->c3 = a->c1*b->a3 + a->c2*b->b3 + a->c3*b->c3 + a->c4*b->d3;
    out->c4 = a->c1*b->a4 + a->c2*b->b4 + a->c3*b->c4 + a->c4*b->d4;

    out->d1 = a->d1*b->a1 + a->d2*b->b1 + a->d3*b->c1 + a->d4*b->d1;
    out->d2 = a->d1*b->a2 + a->d2*b->b2 + a->d3*b->c2 + a->d4*b->d2;
    out->d3 = a->d1*b->a3 + a->d2*b->b3 + a->d3*b->c3 + a->d4*b->d3;
    out->d4 = a->d1*b->a4 + a->d2*b->b4 + a->d3*b->c4 + a->d4*b->d4;
}

void aiTransformVec3(const struct aiMatrix4x4* m, const struct aiVector3D* in, struct aiVector3D* out) {
    out->x = m->a1*in->x + m->a2*in->y + m->a3*in->z + m->a4;
    out->y = m->b1*in->x + m->b2*in->y + m->b3*in->z + m->b4;
    out->z = m->c1*in->x + m->c2*in->y + m->c3*in->z + m->c4;
}

void processNode(struct aiNode* node, const struct aiScene* scene, Model* model, const struct aiMatrix4x4* parentTransform) {
    // compute global transform
    struct aiMatrix4x4 globalTransform;
    aiMultiplyMat4(parentTransform, &node->mTransformation, &globalTransform);

    // Se o nó tem malhas, calcule o AABB para depuração (aplicando transformações)
    if (node->mNumMeshes > 0) {
        float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            struct aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
                struct aiVector3D vt;
                aiTransformVec3(&globalTransform, &mesh->mVertices[v], &vt);
                float vx = vt.x;
                float vy = vt.y;
                float vz = vt.z;
                if (vx < minX) minX = vx;
                if (vy < minY) minY = vy;
                if (vz < minZ) minZ = vz;
                if (vx > maxX) maxX = vx;
                if (vy > maxY) maxY = vy;
                if (vz > maxZ) maxZ = vz;
            }
        }
    const char* n = node->mName.data ? node->mName.data : "(sem nome)";

        // Heurística: detectar superfícies relativamente planas (sizeY pequena)
        float sizeX = maxX - minX;
        float sizeY = maxY - minY;
        float sizeZ = maxZ - minZ;
        int consideredTable = 0;

        // Checa nome explicitamente primeiro
        if (node->mName.length > 0) {
            if (strstr(n, "table") || strstr(n, "Table") || strstr(n, "mesa") || strstr(n, "Mesa") || strstr(n, "desk") || strstr(n, "Desk")) {
                consideredTable = 1;
            }
        }

        // Se nome não explicitou, aplica heurística: altura pequena comparada à largura/comprimento
        if (!consideredTable) {
            float horizontalMax = fmaxf(sizeX, sizeZ);
            // tornar heurística mais permissiva para capturar mesas do seu modelo
            if ((horizontalMax > 0.6f && sizeY < 0.4f * horizontalMax && sizeX > 0.4f && sizeZ > 0.4f) ||
                (sizeX > 1.0f && sizeZ > 0.6f && sizeY < 1.0f)) {
                consideredTable = 1;
            }
        }

        if (consideredTable) {
            // tenta calcular o centro do tampo usando a média dos vértices cuja altura esteja próxima do topo
            float topThreshold = maxY - fminf(0.02f, sizeY * 0.25f);
            double sumX = 0.0, sumZ = 0.0;
            unsigned int countTop = 0;
            for (unsigned int i = 0; i < node->mNumMeshes; i++) {
                struct aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
                    struct aiVector3D vt;
                    aiTransformVec3(&globalTransform, &mesh->mVertices[v], &vt);
                    float vx = vt.x;
                    float vy = vt.y;
                    float vz = vt.z;
                    if (vy >= topThreshold) {
                        sumX += vx;
                        sumZ += vz;
                        countTop++;
                    }
                }
            }
            float centerX, centerZ;
            if (countTop > 0) {
                centerX = (float)(sumX / countTop);
                centerZ = (float)(sumZ / countTop);
            } else {
                centerX = (minX + maxX) * 0.5f;
                centerZ = (minZ + maxZ) * 0.5f;
            }
            float topY = maxY;
            // adicione slot imediatamente se fizermos uma detecção segura (vértices no topo)
            // candidate collection removed
        }

        // Fallback: se ainda não considerado pela heurística, mas tem footprint grande e altura razoável, adicionar slot
        if (!consideredTable) {
            if (sizeX > 0.6f && sizeZ > 0.6f && sizeY < 1.5f) {
                // candidate collection removed
            }
        }
    }

    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        struct aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        model->numMeshes++;
        model->meshes = (Mesh*)realloc(model->meshes, model->numMeshes * sizeof(Mesh));
        model->meshes[model->numMeshes - 1] = processMesh(mesh, scene, model);
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene, model, &globalTransform);
    }
}

Mesh processMesh(struct aiMesh* mesh, const struct aiScene* scene, Model* model) {
    Mesh newMesh = {0};
    
    newMesh.numVertices = mesh->mNumVertices;
    newMesh.vertices = (Vertex*)malloc(newMesh.numVertices * sizeof(Vertex));
    for (unsigned int i = 0; i < newMesh.numVertices; i++) {
        newMesh.vertices[i].position[0] = mesh->mVertices[i].x;
        newMesh.vertices[i].position[1] = mesh->mVertices[i].y;
        newMesh.vertices[i].position[2] = mesh->mVertices[i].z;

        if (mesh->mNormals) {
            newMesh.vertices[i].normal[0] = mesh->mNormals[i].x;
            newMesh.vertices[i].normal[1] = mesh->mNormals[i].y;
            newMesh.vertices[i].normal[2] = mesh->mNormals[i].z;
        }

        if (mesh->mTextureCoords[0]) {
            newMesh.vertices[i].texCoords[0] = mesh->mTextureCoords[0][i].x;
            newMesh.vertices[i].texCoords[1] = mesh->mTextureCoords[0][i].y;
        } else {
            glm_vec2_zero(newMesh.vertices[i].texCoords);
        }
    }

    unsigned int total_indices = 0;
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        total_indices += mesh->mFaces[i].mNumIndices;
    }
    newMesh.numIndices = total_indices;
    newMesh.indices = (unsigned int*)malloc(newMesh.numIndices * sizeof(unsigned int));
    unsigned int index_counter = 0;
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        struct aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            newMesh.indices[index_counter] = face.mIndices[j];
            index_counter++;
        }
    }
    
    if (mesh->mMaterialIndex >= 0) {
        struct aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", &newMesh, model);

        if (newMesh.numTextures == 0) {
            struct aiColor4D color;
            if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
                newMesh.diffuseColor[0] = color.r;
                newMesh.diffuseColor[1] = color.g;
                newMesh.diffuseColor[2] = color.b;
            } else {
                glm_vec3_one(newMesh.diffuseColor);
            }
        }
    }
    
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
    for (unsigned int i = 0; i < model->numMeshes; i++) {
        free(model->meshes[i].vertices);
        free(model->meshes[i].indices);
        if (model->meshes[i].textures) {
            free(model->meshes[i].textures);
        }
    }
    for (unsigned int i = 0; i < model->num_textures_loaded; i++) {
        free(model->textures_loaded[i].path);
        free(model->textures_loaded[i].type);
    }
    free(model->textures_loaded);
    free(model->meshes);
    free(model->directory);
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
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
    // printf("Textura carregada com sucesso: %s\n", filename); // mensagem de log removida
    } else {
        fprintf(stderr, "Falha ao carregar textura: %s\n", filename);
    }
    stbi_image_free(data);
    return textureID;
}

void loadMaterialTextures(struct aiMaterial* mat, enum aiTextureType type, const char* typeName, Mesh* outMesh, Model* model) {
    unsigned int texture_count = aiGetMaterialTextureCount(mat, type);
    outMesh->textures = NULL;
    outMesh->numTextures = 0;
    for (unsigned int i = 0; i < texture_count; i++) {
        struct aiString str;
        aiGetMaterialTexture(mat, type, i, &str, NULL, NULL, NULL, NULL, NULL, NULL);

        int skip = 0;
        for (unsigned int j = 0; j < model->num_textures_loaded; j++) {
            if (strcmp(model->textures_loaded[j].path, str.data) == 0) {
                outMesh->numTextures++;
                outMesh->textures = (Texture*)realloc(outMesh->textures, outMesh->numTextures * sizeof(Texture));
                outMesh->textures[outMesh->numTextures - 1] = model->textures_loaded[j];
                skip = 1;
                break;
            }
        }
        if (!skip) {
            Texture texture;
            texture.id = TextureFromFile(str.data, model->directory);
            texture.type = (char*)malloc(strlen(typeName) + 1);
            strcpy(texture.type, typeName);
            texture.path = (char*)malloc(strlen(str.data) + 1);
            strcpy(texture.path, str.data);
            
            outMesh->numTextures++;
            outMesh->textures = (Texture*)realloc(outMesh->textures, outMesh->numTextures * sizeof(Texture));
            outMesh->textures[outMesh->numTextures - 1] = texture;

            model->num_textures_loaded++;
            model->textures_loaded = (Texture*)realloc(model->textures_loaded, model->num_textures_loaded * sizeof(Texture));
            model->textures_loaded[model->num_textures_loaded - 1] = texture;
        }
    }
}

// Candidate-related code removed for a leaner codebase.

// Carrega exatamente 8 slots de um arquivo (cada linha: x y z [type]). Retorna 1 se OK, 0 caso contrário.
int loadSlotsFromFile(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    float x,y,z;
    int type;
    unsigned int count = 0;
    // preparar arrays temporários
    float tmp[8][3];
    int types[8];
    char line[256];
    while (count < 8 && fgets(line, sizeof(line), f)) {
        // remover newline e possíveis carriage return
        size_t ln = strlen(line);
        while (ln > 0 && (line[ln-1] == '\n' || line[ln-1] == '\r')) { line[--ln] = '\0'; }
        // tenta analisar 3 ou 4 valores por linha com sscanf
        int scanned = sscanf(line, "%f %f %f %d", &x, &y, &z, &type);
        if (scanned == 3 || scanned == 4) {
            tmp[count][0] = x;
            tmp[count][1] = y;
            tmp[count][2] = z;
            if (scanned == 4 && type >= 0 && type <= 3) types[count] = type;
            else types[count] = rand() % 4; // aleatório se não fornecido
            count++;
        } else {
            // linha inválida: ignorar
            continue;
        }
    }
    fclose(f);
    if (count != 8) {
        printf("spots.txt precisa conter exatamente 8 linhas com 'x y z [type]' (encontradas=%u)\n", count);
        return 0;
    }
    // substituir slots atuais
    if (slots) { free(slots); slots = NULL; numSlots = 0; }
    for (unsigned int i = 0; i < 8; i++) {
        addSlotWithType(tmp[i][0], tmp[i][1], tmp[i][2], types[i]);
    }
    // carregamento bem-sucedido, sem mensagens de debug para reduzir poluição do console
    return 1;
}

void drawBonecoAtIndex(unsigned int idx) {
    if (idx >= numSlots) return;
    float x = slots[idx].pos[0];
    float z = slots[idx].pos[2];
    float y = slots[idx].pos[1];
    int type = slots[idx].type % 4;


    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    switch (type) {
        case 0: // padrão
            drawBoneco(x, z);
            break;
        case 1: // cabelo (coroa simples)
            drawBoneco(x, z);
            glPushMatrix();
            glTranslatef(x, y + 1.2f + 0.05f, z);
            glColor3f(0.1f, 0.1f, 0.1f);
            glutSolidTorus(0.03f, 0.18f, 8, 12);
            glPopMatrix();
            break;
        case 2: // capacete (cúpula)
            drawBoneco(x, z);
            glPushMatrix();
            glTranslatef(x, y + 1.2f + 0.05f, z);
            glColor3f(0.3f, 0.3f, 0.6f);
            glutSolidSphere(0.38f, 16, 8);
            glPopMatrix();
            break;
        case 3: // especial: adição de bandeira pequena
            drawBoneco(x, z);
            glPushMatrix();
            glTranslatef(x + 0.35f, y + 1.0f, z);
            glRotatef(-45, 0,1,0);
            glColor3f(1.0f, 0.2f, 0.2f);
            glBegin(GL_TRIANGLES);
                glVertex3f(0, 0.2f, 0);
                glVertex3f(0.4f, 0.1f, 0);
                glVertex3f(0, 0, 0);
            glEnd();
            glPopMatrix();
            break;
    }

    // restaurar cor padrão
    glColor3f(0.8f, 0.6f, 0.3f);
    glPopAttrib();
}