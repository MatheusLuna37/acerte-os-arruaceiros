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
vec3 cameraPos   = {30.0f, 8.0f, 0.0f}; // Posição dos "olhos" da professora. Sinta-se à vontade para ajustar!
vec3 cameraFront = {0.0f, 1.0f, 0.0f}; // Direção inicial para onde a câmera olha
vec3 cameraUp    = {0.0f, 1.0f, 0.0f};  // Vetor "para cima"

float cameraYaw = 180.0f; // Yaw inicial para olhar para o centro da sala (eixo -Z)
float cameraPitch = -20.0f;
int lastX, lastY;
int mouse_left_button_down = 0;

// NOVO: Variáveis para a mira automática da câmera
vec3 cameraTargetDirection; // A direção final para onde a câmera deve olhar
bool isCameraTurning = false; // Flag que controla se a câmera está no meio de uma virada
float cameraTurnProgress = 0.0f; // Progresso da virada da câmera (0.0 a 1.0)

bool keyStates[256] = {false};

Model* ourModel = NULL;
Model* menModel = NULL; // Modelo do tronco (MEN.obj)

// NOVO: Variáveis para o Martelo e sua Animação
// NÃO precisamos mais carregar modelo .obj - usaremos primitivas OpenGL!
float hammerAnimationAngle = 0.0f; // Ângulo atual da animação de batida
float hammerAnimationMovingtoTarget = 0.0f; // Progresso da animação (0.0 a 1.0)
// Enum para controlar o estado da animação
typedef enum { IDLE, MOVING_TO_TARGET, SWINGING_DOWN, SWINGING_UP, RETURNING } HammerState;
HammerState hammerState = IDLE;

// Posição do martelo no ESPAÇO 3D DA CENA (coordenadas mundiais)
vec3 hammerPosStart = {35.0f, 6.0f, -6.0f}; // Posição inicial (será atualizada dinamicamente)
vec3 hammerPosCurrent = {35.0f, 6.0f, -6.0f}; // Posição atual no mundo 3D
vec3 hammerPosTarget = {0.0f, 0.0f, 0.0f}; // Posição alvo no mundo 3D (calculada por ray cast)

// Escala do martelo (varia com a distância)
float hammerBaseScale = 1.5f; // Escala base do martelo (aumentada para melhor visibilidade)
float hammerCurrentScale = 1.5f; // Escala atual que aumenta com distância

// --- TEXTURAS PARA CABEÇAS DOS BONECOS ---
GLuint headTextures[4]; // 4 texturas, uma para cada tipo de boneco
int texturesLoaded = 0; // Flag para saber se texturas foram carregadas

// Função para carregar textura de arquivo
GLuint loadTexture(const char* filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);
    
    if (!data) {
        printf("⚠️  Falha ao carregar textura: %s\n", filename);
        return 0;
    }
    
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Configurar parâmetros da textura
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // CRÍTICO: Ajustar alinhamento para imagens RGB (3 canais)
    // OpenGL espera alinhamento de 4 bytes por padrão, mas RGB tem 3 bytes por pixel
    if (channels == 3) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }
    
    // Enviar dados da textura para GPU
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    
    // Restaurar alinhamento padrão
    if (channels == 3) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }
    
    stbi_image_free(data);
    printf("✓ Textura carregada: %s (%dx%d, %d canais)\n", filename, width, height, channels);
    return textureID;
}

// Função para inicializar texturas das cabeças
void initHeadTextures() {
    // Carregar 4 texturas, uma para cada tipo
    headTextures[0] = loadTexture("textures/head_green.jpg");  // Tipo 0 (verde +1)
    headTextures[1] = loadTexture("textures/head_blue.jpg");   // Tipo 1 (azul +2)
    headTextures[2] = loadTexture("textures/head_red.jpg");    // Tipo 2 (vermelho -1)
    headTextures[3] = loadTexture("textures/head_black.jpg");  // Tipo 3 (preto +4)
    
    texturesLoaded = 1;
    printf("✓ Sistema de texturas inicializado!\n");
}

// --- SISTEMA WHACK-A-MOLE: Slots e Bonecos ---
typedef struct {
    vec3 pos; // x, y, z (posição no mundo)
    int clicked; // 0 = não clicado, 1 = já clicado
    int type; // 0..3 tipos diferentes de bonecos (cores)
} Slot;

Slot* slots = NULL;
unsigned int numSlots = 0;
int score = 0;

// Lógica do jogo
int gameActive = 0; // 0 = modo livre (todos visíveis), 1 = modo jogo (um por vez)
int currentActive = -1; // índice do slot atualmente visível
int moleVisible = 0; // se o boneco está visível
unsigned int moleShowMs = 1500; // tempo visível em ms (1.5 segundos)
unsigned int moleIntervalMs = 600; // tempo entre aparições (0.6 segundos)

int drawCubeMode = 1; // 1 = desenha bonecos, 0 = desenha quadrados verdes
float slotOffsetX = 0.0f; // offset para ajuste fino
float slotOffsetZ = 0.0f;

// Protótipos de Model (devem vir antes de drawBoneco)
void Model_Draw(Model* model);
Model* Model_Create(const char* path);
void Model_Destroy(Model* model);

// Protótipos whack-a-mole
void gameTick(int value);
void startGame();
void stopGame();
void addSlot(float centerX, float topY, float centerZ);
void addSlotWithType(float centerX, float topY, float centerZ, int type);
void drawSlot(float x, float z);
void drawBoneco(float x, float z);
void drawBonecoAtIndex(unsigned int idx);
int loadSlotsFromFile(const char* path);

// ===================================================================================
// FUNÇÕES DE INPUT (TECLADO)
// ===================================================================================

// Chamada quando uma tecla é pressionada
void keyboardDown(unsigned char key, int x, int y) {
    if ((unsigned char)key < 256) keyStates[(unsigned char)key] = true;
    
    // Teclas especiais do jogo
    if (key == 'b' || key == 'B') {
        if (gameActive) {
            stopGame();
            printf("⏸ Jogo pausado\n");
        } else {
            startGame();
            printf("▶ Jogo iniciado!\n");
        }
    } else if (key == 'v' || key == 'V') {
        drawCubeMode = !drawCubeMode;
        printf("Modo visual: %s\n", drawCubeMode ? "Bonecos 3D" : "Quadrados verdes");
        glutPostRedisplay();
    }
}

// Chamada quando uma tecla é solta
void keyboardUp(unsigned char key, int x, int y) {
    if ((unsigned char)key < 256) keyStates[(unsigned char)key] = false;
}

// Processa o input do teclado a cada quadro
void processKeyboard() {
    float movementSpeed = 0.1f;

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

// Função para desenhar o martelo com primitivas OpenGL
void drawHammer() {
    GLUquadric* quadric = gluNewQuadric();
    
    // Cor do cabo (madeira marrom)
    glColor3f(0.55f, 0.27f, 0.07f);
    
    // CABO: Cilindro vertical (de baixo para cima)
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f); // Rotaciona para ficar vertical
    gluCylinder(quadric, 0.15, 0.15, 2.5, 16, 1); // raio, raio_topo, altura, slices, stacks
    glPopMatrix();
    
    // Cor da cabeça (metal cinza)
    glColor3f(0.6f, 0.6f, 0.65f);
    
    // CABEÇA DO MARTELO: Cubo/Paralelepípedo no topo
    glPushMatrix();
    glTranslatef(0.0f, 2.5f, 0.0f); // Move para o topo do cabo
    
    // Desenha um cubo achatado (cabeça do martelo)
    glPushMatrix();
    glScalef(0.8f, 0.4f, 0.4f); // Largura, altura, profundidade
    glutSolidCube(1.0);
    glPopMatrix();
    
    glPopMatrix();
    
    // PONTA DE IMPACTO: Pequeno cilindro na ponta da cabeça
    glColor3f(0.5f, 0.5f, 0.55f);
    glPushMatrix();
    glTranslatef(0.0f, 2.5f, 0.0f);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f); // Rotaciona para apontar para frente
    glTranslatef(0.0f, 0.0f, -0.4f);
    gluCylinder(quadric, 0.15, 0.18, 0.3, 16, 1); // Ponta levemente cônica
    glPopMatrix();
    
    gluDeleteQuadric(quadric);
}

// ===================================================================================
// IMPLEMENTAÇÃO WHACK-A-MOLE
// ===================================================================================

void gameTick(int value) {
    if (!gameActive) return;
    if (moleVisible) {
        moleVisible = 0;
        currentActive = -1;
        glutTimerFunc(moleIntervalMs, gameTick, 0);
    } else {
        if (numSlots == 0) {
            glutTimerFunc(moleIntervalMs, gameTick, 0);
            return;
        }
        // Escolhe slot aleatório (DIFERENTE do anterior sempre)
        int next = rand() % (int)numSlots;
        int attempts = 0;
        while (numSlots > 1 && next == currentActive && attempts < 10) {
            next = rand() % (int)numSlots;
            attempts++;
        }
        
        currentActive = next;
        moleVisible = 1;
        slots[currentActive].clicked = 0;
        
        // MUDA O TIPO DO BONECO ALEATORIAMENTE a cada aparição!
        slots[currentActive].type = rand() % 4; // 0=verde, 1=azul, 2=vermelho, 3=preto
        
        glutTimerFunc(moleShowMs, gameTick, 0);
    }
    glutPostRedisplay();
}

void startGame() {
    if (gameActive) return;
    gameActive = 1;
    score = 0;
    currentActive = -1;
    moleVisible = 0;
    glutTimerFunc(moleIntervalMs, gameTick, 0);
}

void stopGame() {
    if (!gameActive) return;
    gameActive = 0;
    currentActive = -1;
    moleVisible = 0;
    glutPostRedisplay();
}

void addSlot(float centerX, float topY, float centerZ) {
    numSlots++;
    slots = (Slot*)realloc(slots, numSlots * sizeof(Slot));
    slots[numSlots - 1].pos[0] = centerX;
    slots[numSlots - 1].pos[1] = topY;
    slots[numSlots - 1].pos[2] = centerZ;
    slots[numSlots - 1].clicked = 0;
}

void addSlotWithType(float centerX, float topY, float centerZ, int type) {
    addSlot(centerX, topY, centerZ);
    if (numSlots > 0) slots[numSlots - 1].type = type % 4;
}

void drawSlot(float x, float z) {
    float y = 0.01f;
    for (unsigned int i = 0; i < numSlots; i++) {
        if (fabsf(slots[i].pos[0] - x) < 0.0001f && fabsf(slots[i].pos[2] - z) < 0.0001f) {
            y = slots[i].pos[1];
            break;
        }
    }
    
    float half = 0.30f;
    x += slotOffsetX;
    z += slotOffsetZ;
    
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
    // Salva coordenadas originais ANTES de aplicar offsets
    float origX = x;
    float origZ = z;
    
    float y = 0.01f;
    for (unsigned int i = 0; i < numSlots; i++) {
        if (fabsf(slots[i].pos[0] - origX) < 0.0001f && fabsf(slots[i].pos[2] - origZ) < 0.0001f) {
            y = slots[i].pos[1];
            break;
        }
    }
    
    float trunkWidth = 0.9f;
    float trunkHeight = 1.4f;
    float trunkDepth = 0.6f;
    float headRadius = 1.40f; // DOBRADO NOVAMENTE: era 0.70f (original 0.35f)
    
    x += slotOffsetX - 2.0f;  // Move todo o boneco (avançou de -3 para -2)
    z += slotOffsetZ;
    
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    // Busca o tipo do boneco usando coordenadas ORIGINAIS (antes dos offsets)
    int bonecoType = 0;
    float trunkR = 0.8f, trunkG = 0.6f, trunkB = 0.3f;
    for (unsigned int i = 0; i < numSlots; i++) {
        if (fabsf(slots[i].pos[0] - origX) < 0.0001f && fabsf(slots[i].pos[2] - origZ) < 0.0001f) {
            int t = slots[i].type % 4;
            bonecoType = t;
            if (t == 0) { trunkR = 0.0f; trunkG = 0.9f; trunkB = 0.0f; }
            else if (t == 1) { trunkR = 0.0f; trunkG = 0.0f; trunkB = 0.9f; }
            else if (t == 2) { trunkR = 0.9f; trunkG = 0.0f; trunkB = 0.0f; }
            else { trunkR = 0.05f; trunkG = 0.05f; trunkB = 0.05f; }
            break;
        }
    }
    
    // Desenha a cabeça PRIMEIRO (para manter billboard funcionando)
    glPushMatrix();
    // Centraliza cabeça com o tronco
    float headX = x;
    float headZ = z + 0.3f;  // Pequeno ajuste para frente
    glTranslatef(headX, y + trunkHeight + headRadius, headZ);
    
    // === BILLBOARD: Faz a cabeça sempre olhar para a câmera ===
    // Calcula vetor da cabeça para a câmera
    vec3 headPos = {headX, y + trunkHeight + headRadius, headZ};
    vec3 toCamera;
    glm_vec3_sub(cameraPos, headPos, toCamera);
    
    // Billboard cilíndrico (só gira no eixo Y - mais natural para personagens)
    // Ignora diferença de altura (Y) para rotação horizontal apenas
    float dx = toCamera[0];
    float dz = toCamera[2];
    
    // Calcula ângulo corretamente: atan2(x, z) retorna ângulo do vetor (x,z)
    // Precisamos inverter o sinal para rotação correta do OpenGL
    float angleY = glm_deg(atan2f(dx, dz));
    
    // Aplica rotação Y para fazer a cabeça olhar para a câmera
    glRotatef(angleY, 0.0f, 1.0f, 0.0f);
    
    if (texturesLoaded) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, headTextures[bonecoType]);
        glColor3f(1.0f, 1.0f, 1.0f);
        
        // Rotações base para orientar textura (após billboard)
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);  // Corrige orientação vertical
        glRotatef(0.0f, 0.0f, 0.0f, 1.0f);     // Sem rotação Z (billboard já controla orientação)
        
        GLUquadric* quad = gluNewQuadric();
        gluQuadricTexture(quad, GL_TRUE);
        gluQuadricOrientation(quad, GLU_OUTSIDE);
        gluSphere(quad, headRadius, 32, 32);
        gluDeleteQuadric(quad);
        
        glDisable(GL_TEXTURE_2D);
    } else {
        glutSolidSphere(headRadius, 16, 16);
    }
    
    glPopMatrix();
    
    // Desenha o tronco (MEN.obj ou cubo se não carregado)
    glPushMatrix();
    glTranslatef(x, y - trunkHeight * 0.7f, z); // Posiciona o tronco entre o chão e a cabeça
    glColor3f(trunkR, trunkG, trunkB);
    
    if (menModel != NULL) {
        glEnable(GL_LIGHTING);
        glRotatef(90.0f, 0.0f, 1.0f, 0.0f); // Rotação de 90° no eixo Y
        glScalef(2.0f, 2.0f, 2.0f); // Escala 2x
        Model_Draw(menModel);
        glDisable(GL_LIGHTING);
    } else {
        // Fallback: desenha cubo se MEN.obj não carregar
        glScalef(trunkWidth, trunkHeight, trunkDepth);
        glutSolidCube(1.0f);
    }
    glPopMatrix();
    
    glPopAttrib();
}

void drawBonecoAtIndex(unsigned int idx) {
    if (idx >= numSlots) return;
    float x = slots[idx].pos[0];
    float z = slots[idx].pos[2];
    drawBoneco(x, z);
}

int loadSlotsFromFile(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    
    float x,y,z;
    int type;
    unsigned int count = 0;
    float tmp[8][3];
    int types[8];
    char line[256];
    
    while (count < 8 && fgets(line, sizeof(line), f)) {
        size_t ln = strlen(line);
        while (ln > 0 && (line[ln-1] == '\n' || line[ln-1] == '\r')) { line[--ln] = '\0'; }
        
        int scanned = sscanf(line, "%f %f %f %d", &x, &y, &z, &type);
        if (scanned == 3 || scanned == 4) {
            tmp[count][0] = x;
            tmp[count][1] = y;
            tmp[count][2] = z;
            if (scanned == 4 && type >= 0 && type <= 3) types[count] = type;
            else types[count] = rand() % 4;
            count++;
        }
    }
    fclose(f);
    
    if (count != 8) {
        printf("spots.txt precisa conter 8 linhas (encontradas=%u)\n", count);
        return 0;
    }
    
    if (slots) { free(slots); slots = NULL; numSlots = 0; }
    for (unsigned int i = 0; i < 8; i++) {
        addSlotWithType(tmp[i][0], tmp[i][1], tmp[i][2], types[i]);
        printf("  Slot %d: (%.2f, %.2f, %.2f) Tipo=%d\n", i, tmp[i][0], tmp[i][1], tmp[i][2], types[i]);
    }
    printf("✓ 8 slots carregados de %s\n", path);
    return 1;
}

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
    
    // Carrega modelo da sala
    ourModel = Model_Create(argv[1]);
    if (!ourModel) {
        fprintf(stderr, "Falha ao carregar o modelo da sala.\n");
        return -1;
    }
    
    // Carrega modelo do tronco (MEN.obj)
    printf("\n--- Carregando modelo do tronco dos bonecos ---\n");
    menModel = Model_Create("MEN.obj");
    if (!menModel) {
        fprintf(stderr, "⚠️  Aviso: Falha ao carregar MEN.obj - usando cubos para troncos\n");
    } else {
        printf("✓ Modelo MEN.obj carregado com sucesso!\n");
    }

    // Martelo agora é desenhado com primitivas OpenGL - não precisa carregar modelo!
    printf("Martelo será desenhado com primitivas OpenGL (não requer arquivo .obj)\n");
    
    // Inicializa gerador de números aleatórios ANTES de carregar slots
    srand((unsigned int)time(NULL));
    
    // Carrega slots (topeiras/bonecos) do arquivo
    FILE* fspots = fopen("spots.txt", "r");
    if (fspots) {
        fclose(fspots);
        if (loadSlotsFromFile("spots.txt")) {
            printf("✓ Slots carregados! Iniciando jogo automaticamente...\n");
            printf("════════════════════════════════════════════════════════\n");
            printf("  🎮 MODO WHACK-A-MOLE ATIVO!\n");
            printf("  📍 Bonecos aparecem ALEATORIAMENTE em posições diferentes\n");
            printf("  🎨 Tipos ALEATÓRIOS a cada aparição:\n");
            printf("     🟢 Verde  = +1 ponto\n");
            printf("     🔵 Azul   = +2 pontos\n");
            printf("     🔴 Vermelho = -1 ponto (EVITE!)\n");
            printf("     ⚫ Preto  = +4 pontos (RARO!)\n");
            printf("  ⌨️  Tecla B = iniciar jogo | V = modo visual\n");
            printf("════════════════════════════════════════════════════════\n");
            // NÃO inicia o jogo automaticamente - pressione B para começar
            // startGame();
        }
    } else {
        printf("⚠ spots.txt não encontrado - jogo sem bonecos\n");
    }
    
    // Inicializa texturas das cabeças dos bonecos
    initHeadTextures();
    
    // --- Registro de Callbacks e Loop Principal ---
    glutDisplayFunc(renderScene);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouseButton);
    glutPassiveMotionFunc(mouseMove);  // Movimento SEM clicar
    glutMotionFunc(mouseMove);          // Movimento COM clicar (mantido para compatibilidade)
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
    processKeyboard();

    // 1. Atualiza a animação do martelo (lógica movida para o topo)
    float moveSpeed = 0.02f; // Velocidade de movimento
    float swingSpeed = 1.2f; // Velocidade de batida
    
    if (hammerState == MOVING_TO_TARGET) {
        // Só move o martelo se a câmera já tiver virado pelo menos 60%
        float cameraThreshold = 1.0f;
        if (!isCameraTurning || cameraTurnProgress >= cameraThreshold) {
            // Move o martelo em direção ao alvo no espaço 3D
            hammerAnimationMovingtoTarget += moveSpeed;
            if (hammerAnimationMovingtoTarget >= 1.0f) {
                hammerAnimationMovingtoTarget = 1.0f;
                hammerState = SWINGING_DOWN;
                printf("⚡ Martelo chegou no alvo, iniciando swing!\n");
            }
            
            // Interpolação da posição 3D (lerp vetorial)
            glm_vec3_lerp(hammerPosStart, hammerPosTarget, hammerAnimationMovingtoTarget, hammerPosCurrent);
            
            // Aumenta a escala com distância para compensar perspectiva
            // Mantém tamanho visual mais constante independente da distância
            float distanceToCamera = glm_vec3_distance(cameraPos, hammerPosCurrent);
            // Cresce até 2.5x em distâncias grandes (50 unidades+)
            float distanceFactor = 1.0f + ((distanceToCamera - 10.0f) / 20.0f);
            distanceFactor = glm_clamp(distanceFactor, 1.0f, 4.5f);
            hammerCurrentScale = hammerBaseScale * distanceFactor;
        }
        
    } else if (hammerState == SWINGING_DOWN) {
        hammerAnimationAngle += swingSpeed;
        if (hammerAnimationAngle >= 90.0f) {
            hammerAnimationAngle = 90.0f;
            hammerState = SWINGING_UP;
            
            // DETECÇÃO DE COLISÃO NO MOMENTO DO IMPACTO!
            float hitRadius = 20.0f; // Range equilibrado
            int hit = 0;
            float minDist = 999999.0f;
            int closestSlot = -1;
            
            for (unsigned int i = 0; i < numSlots; i++) {
                // Se jogo ativo, só considera o boneco ativo e visível
                if (gameActive && ((int)i != currentActive || !moleVisible)) continue;
                
                // Verifica distância 2D (X e Z) entre martelo e boneco
                // IGNORA Y porque o raycast aponta pro chão (Y=0.5) mas slots estão em Y=2.0
                float dx = slots[i].pos[0] - hammerPosTarget[0];
                float dz = slots[i].pos[2] - hammerPosTarget[2];
                float dist = sqrtf(dx*dx + dz*dz);
                
                // Rastreia o slot mais próximo
                if (dist < minDist) {
                    minDist = dist;
                    closestSlot = i;
                }
                
                if (dist <= hitRadius && !slots[i].clicked) {
                    slots[i].clicked = 1;
                    int points = (slots[i].type == 0) ? 1 : 
                                (slots[i].type == 1) ? 2 :
                                (slots[i].type == 2) ? -1 : 4;
                    score += points;
                    printf("💥 ACERTOU! Slot %d Tipo %d = %+d pts | Score: %d | Dist: %.2f\n", 
                           i, slots[i].type, points, score, dist);
                    
                    if (gameActive) {
                        moleVisible = 0; // Esconde imediatamente
                    }
                    hit = 1;
                    break;
                }
            }
            
            // Se errou, mostra a distância até o mais próximo
            if (!hit && closestSlot >= 0) {
                printf("❌ ERROU! Mais próximo: Slot %d a %.2f unidades (precisa < %.1f)\n", 
                       closestSlot, minDist, hitRadius);
            }
        }
        
    } else if (hammerState == SWINGING_UP) {
        hammerAnimationAngle -= swingSpeed;
        if (hammerAnimationAngle <= -10.0f) {
            hammerAnimationAngle = -10.0f;
            hammerState = RETURNING;
        }
        
    } else if (hammerState == RETURNING) {
        // Retorna o martelo para a posição inicial
        hammerAnimationMovingtoTarget -= moveSpeed;
        if (hammerAnimationMovingtoTarget <= 0.0f) {
            hammerAnimationMovingtoTarget = 0.0f;
            hammerState = IDLE;
            hammerCurrentScale = hammerBaseScale;
        } else {
            // Interpolação da posição 3D durante retorno
            glm_vec3_lerp(hammerPosStart, hammerPosTarget, hammerAnimationMovingtoTarget, hammerPosCurrent);
            
            // Mesma lógica de escala crescente durante retorno
            float distanceToCamera = glm_vec3_distance(cameraPos, hammerPosCurrent);
            float distanceFactor = 1.0f + ((distanceToCamera - 10.0f) / 40.0f);
            distanceFactor = glm_clamp(distanceFactor, 1.0f, 2.5f);
            hammerCurrentScale = hammerBaseScale * distanceFactor;
        }
    }
    
    // Quando IDLE ou RETURNING perto do fim, martelo acompanha a câmera (estilo FPS)
    if (hammerState == IDLE || (hammerState == RETURNING && hammerAnimationMovingtoTarget < 0.1f)) {
        // Calcula posição do martelo relativa à câmera
        // Posiciona à direita e abaixo do centro da visão
        vec3 right, down, forward;
        
        // Vetor para direita (perpendicular a cameraFront e cameraUp)
        glm_vec3_cross(cameraFront, cameraUp, right);
        glm_vec3_normalize(right);
        
        // Vetor para baixo (inverso do up)
        glm_vec3_negate_to(cameraUp, down);
        
        // Copia direção frontal
        glm_vec3_copy(cameraFront, forward);
        
        // Posição base = câmera + um pouco à frente
        glm_vec3_copy(cameraPos, hammerPosCurrent);
        glm_vec3_scale(forward, 3.0f, forward);  // 3 unidades à frente
        glm_vec3_add(hammerPosCurrent, forward, hammerPosCurrent);
        
        // Desloca para direita (2 unidades)
        glm_vec3_scale(right, 2.0f, right);
        glm_vec3_add(hammerPosCurrent, right, hammerPosCurrent);
        
        // Desloca para baixo (0.5 unidades) - REDUZIDO para ficar mais visível
        glm_vec3_scale(down, 0.5f, down);
        glm_vec3_add(hammerPosCurrent, down, hammerPosCurrent);
        
        // Atualiza hammerPosStart para quando iniciar próximo ataque
        glm_vec3_copy(hammerPosCurrent, hammerPosStart);
        
        hammerCurrentScale = hammerBaseScale * 0.8f; // Menor quando em repouso
    }

    // 2. Atualiza a mira da câmera se ela estiver virando
    if (isCameraTurning) {
        // Interpola suavemente a direção atual para a direção alvo
        float interpolationSpeed = 0.08f; // Velocidade de virada mais controlada
        
        vec3 oldFront;
        glm_vec3_copy(cameraFront, oldFront);
        
        glm_vec3_lerp(cameraFront, cameraTargetDirection, interpolationSpeed, cameraFront);
        glm_vec3_normalize(cameraFront);

        // Calcula o progresso da virada (0.0 = início, 1.0 = completo)
        float distanceToTarget = glm_vec3_distance(cameraFront, cameraTargetDirection);
        float initialDistance = glm_vec3_distance(oldFront, cameraTargetDirection);
        if (initialDistance > 0.001f) {
            cameraTurnProgress = 1.0f - (distanceToTarget / initialDistance);
            if (cameraTurnProgress > 1.0f) cameraTurnProgress = 1.0f;
            if (cameraTurnProgress < 0.0f) cameraTurnProgress = 0.0f;
        }

        // Atualiza os ângulos Yaw e Pitch a partir do novo vetor de direção
        cameraPitch = glm_deg(asin(cameraFront[1]));
        cameraYaw = glm_deg(atan2(cameraFront[2], cameraFront[0]));

        // Para a virada quando estiver perto o suficiente do alvo
        if (distanceToTarget < 0.05f) {
            isCameraTurning = false;
            cameraTurnProgress = 1.0f;
        }
    }

    // Força o redesenho continuamente para martelo acompanhar câmera
    // (mesmo em IDLE, o martelo precisa seguir os movimentos da câmera)
    glutPostRedisplay();

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

    // --- Desenha os Bonecos (Whack-a-Mole) ---
    if (gameActive) {
        // Modo jogo: desenha apenas o boneco ativo
        if (currentActive >= 0 && (unsigned int)currentActive < numSlots && moleVisible) {
            if (drawCubeMode) drawBonecoAtIndex(currentActive);
            else drawSlot(slots[currentActive].pos[0], slots[currentActive].pos[2]);
        }
    } else {
        // Modo livre: desenha todos os bonecos
        for (unsigned int i = 0; i < numSlots; i++) {
            if (drawCubeMode) drawBonecoAtIndex(i);
            else drawSlot(slots[i].pos[0], slots[i].pos[2]);
        }
    }

    // --- Desenha o Martelo com Primitivas OpenGL no Espaço 3D ---
    // IMPORTANTE: Desabilita depth test para o martelo sempre ficar visível (não ser coberto)
    glDisable(GL_DEPTH_TEST);
    
    glPushMatrix(); // Salva a matriz atual

    // Move o martelo para sua posição atual no mundo 3D
    glTranslatef(hammerPosCurrent[0], hammerPosCurrent[1], hammerPosCurrent[2]);
    
    // Orienta o martelo baseado no estado
    if (hammerState == IDLE || (hammerState == RETURNING && hammerAnimationMovingtoTarget < 0.1f)) {
        // Em IDLE: aponta na mesma direção da câmera
        float yaw = glm_deg(atan2f(cameraFront[0], cameraFront[2]));
        float pitch = glm_deg(asinf(-cameraFront[1]));
        
        glRotatef(yaw, 0.0f, 1.0f, 0.0f);   // Rotação horizontal (segue câmera)
        glRotatef(pitch, 1.0f, 0.0f, 0.0f); // Rotação vertical (segue câmera)
        glRotatef(90.0f, 0.0f, 1.0f, 0.0f); // Gira 90° para ficar de frente
        
    } else {
        // Em ataque: aponta para o alvo
        vec3 hammerToTarget;
        glm_vec3_sub(hammerPosTarget, hammerPosCurrent, hammerToTarget);
        
        if (glm_vec3_norm(hammerToTarget) > 0.01f) {
            glm_vec3_normalize(hammerToTarget);
            
            // Calcula ângulos de rotação para apontar para o alvo
            float yaw = glm_deg(atan2f(hammerToTarget[0], hammerToTarget[2]));
            float pitch = glm_deg(asinf(-hammerToTarget[1]));
            
            glRotatef(yaw, 0.0f, 1.0f, 0.0f);   // Rotação horizontal
            glRotatef(pitch, 1.0f, 0.0f, 0.0f); // Rotação vertical
        }
    }

    // Escala o martelo baseada na distância (perspectiva automática)
    glScalef(hammerCurrentScale, hammerCurrentScale, hammerCurrentScale);

    // ESTRATÉGIA: Fazer o cabo ser o pivô fixo e a cabeça girar para atingir o alvo
    // Ordem de transformações (lembre: OpenGL aplica de BAIXO para CIMA):
    
    glTranslatef(0.0f, -2.5f, 0.0f); // Move o martelo para BAIXO (cabo em 0,0,0)
    // 1. Rotaciona PRIMEIRO em torno da origem (marretada)
    //    A origem será onde o cabo fica (pivô fixo)
    glRotatef(hammerAnimationAngle, 0.0f, 0.0f, 1.0f);
    
    // 2. Move o martelo para BAIXO para que a CABEÇA fique no alvo
    //    drawHammer() desenha cabo em (0,0,0) e cabeça em (0,2.5,0)
    //    Movemos -2.5 em Y: cabo em (0,-2.5,0) e cabeça em (0,0,0) = ALVO!

    // Desenha o martelo com primitivas OpenGL
    drawHammer();

    glPopMatrix(); // Restaura a matriz original
    
    // Reabilita depth test para os demais objetos
    glEnable(GL_DEPTH_TEST);

    // --- Desenha Score HUD ---
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
    for (char* c = scoreStr; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

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
        
        // Inicia a animação ao pressionar o botão
        if (state == GLUT_DOWN && hammerState == IDLE) {
            // --- RAY CASTING PARA ENCONTRAR PONTO 3D NO MUNDO ---
            
            // 1. Pega as matrizes e o viewport atuais do OpenGL
            GLdouble modelview[16];
            GLdouble projection[16];
            GLint viewport[4];
            glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
            glGetDoublev(GL_PROJECTION_MATRIX, projection);
            glGetIntegerv(GL_VIEWPORT, viewport);

            // 2. Converte coordenadas da janela (Y invertido)
            float winY = (float)viewport[3] - (float)y;
            
            // 3. "Desprojeta" o clique para obter um raio 3D no mundo
            GLdouble near_x, near_y, near_z;
            GLdouble far_x, far_y, far_z;
            gluUnProject(x, winY, 0.0, modelview, projection, viewport, &near_x, &near_y, &near_z);
            gluUnProject(x, winY, 1.0, modelview, projection, viewport, &far_x, &far_y, &far_z);

            vec3 ray_origin = {(float)near_x, (float)near_y, (float)near_z};
            vec3 ray_far = {(float)far_x, (float)far_y, (float)far_z};
            
            vec3 ray_dir;
            glm_vec3_sub(ray_far, ray_origin, ray_dir);
            glm_vec3_normalize(ray_dir);

            // 4. Calcula a interseção do raio com o plano das CABEÇAS dos bonecos (y≈2.8)
            bool targetFound = false;
            // Altura do centro das cabeças: base (0.01) + trunk (1.4) + headRadius (1.4) = 2.81
            float targetHeight = 2.8f;
            
            if (fabs(ray_dir[1]) > 0.001f) {
                // Calcula t para interseção com plano y = targetHeight
                // Equação: ray_origin.y + t * ray_dir.y = targetHeight
                float t = (targetHeight - ray_origin[1]) / ray_dir[1];
                if (t > 0) { // Apenas se o ponto está à frente da câmera
                    vec3 targetPoint;
                    glm_vec3_scale(ray_dir, t, targetPoint);
                    glm_vec3_add(ray_origin, targetPoint, targetPoint);
                    
                    // Define o alvo 3D do martelo na altura das cabeças
                    glm_vec3_copy(targetPoint, hammerPosTarget);
                    hammerPosTarget[1] = targetHeight; // Mantém altura das cabeças
                    targetFound = true;
                    
                    printf("🎯 Alvo do martelo: (%.2f, %.2f, %.2f)\n", 
                           hammerPosTarget[0], hammerPosTarget[1], hammerPosTarget[2]);

                    // 5. Define a direção alvo para a câmera e inicia a virada
                    glm_vec3_sub(targetPoint, cameraPos, cameraTargetDirection);
                    glm_vec3_normalize(cameraTargetDirection);
                    isCameraTurning = true;
                }
            }

            // 6. Inicia a animação do martelo (se encontrou um alvo válido)
            if (targetFound) {
                // Calcula posição inicial do martelo: próximo da câmera, ligeiramente à direita e abaixo
                vec3 rightVector;
                vec3 upVector = {0.0f, 1.0f, 0.0f};
                vec3 forwardOffset;
                
                // Calcula vetor direita (perpendicular a front e up)
                glm_vec3_cross(cameraFront, upVector, rightVector);
                glm_vec3_normalize(rightVector);
                
                glm_vec3_scale(cameraFront, 2.0f, forwardOffset); // 2 unidades à frente (mais próximo)
                
                glm_vec3_copy(cameraPos, hammerPosStart);
                glm_vec3_add(hammerPosStart, forwardOffset, hammerPosStart); // Move para frente
                
                // Adiciona offset para direita e para baixo
                vec3 rightOffset;
                glm_vec3_scale(rightVector, 0.8f, rightOffset); // 0.8 unidades à direita
                glm_vec3_add(hammerPosStart, rightOffset, hammerPosStart);
                
                hammerPosStart[1] += 0.5f; // Eleva apenas 0.5 unidades (mais baixo na tela)
                
                hammerAnimationMovingtoTarget = 0.0f;
                hammerAnimationAngle = 0.0f;
                glm_vec3_copy(hammerPosStart, hammerPosCurrent);
                hammerCurrentScale = hammerBaseScale;
                cameraTurnProgress = 0.0f; // Reseta o progresso da câmera
                hammerState = MOVING_TO_TARGET;
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

    // Aplica a rotação SEMPRE (sem precisar clicar)
    float sensitivity = 0.1f; // Sensibilidade pode ser ajustada
    cameraYaw += dx * sensitivity;
    cameraPitch += dy * sensitivity;

    // Limita a rotação vertical para não "virar de cabeça para baixo"
    if (cameraPitch > 89.0f) cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;

    glutPostRedisplay(); // Solicita um redesenho da cena
}

void cleanup(void) {
    printf("Limpando recursos...\n");
    Model_Destroy(ourModel);
    Model_Destroy(menModel); // Libera modelo do tronco
    // Martelo agora é primitiva OpenGL - não precisa destruir modelo
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