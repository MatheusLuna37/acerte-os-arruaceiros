#include <windows.h>
#include <GL/glut.h>

// NOTE: Le code a été structuré pour permettre de basculer facilement entre les exemples
// en commentant et décommentant les appels de fonction dans la fonction 'main'.

// Fonction d'éclairage pour les Exemples 1, 2 et 3
void lighting_example1_2_3() {
    // Une source de lumière locale
    float position[4] = {2.0f, 2.0f, 2.0f, 1.0f};
    float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, black); // Ia
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white); // Il_diff
    glLightfv(GL_LIGHT0, GL_SPECULAR, white); // Il_spec

    // --- Pour l'Exemple 2: Spécification des coefficients d'atténuation radiale ---
    // Décommentez les lignes suivantes pour activer l'atténuation
    // glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.5f); // définit a0
    // glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.15f); // définit a1
    // glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.1f); // définit a2


    // --- Pour l'Exemple 3: Paramètres d'éclairage globaux ---
    // Décommentez les lignes suivantes pour un éclairage ambiant global
    // float global_ambient[4] = {0.9f, 0.9f, 0.9f, 1.0f};
    // glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);

    // Active l'éclairage
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
}


int init() {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // définit la couleur de fond
    glEnable(GL_DEPTH_TEST); // active le test de profondeur

    glMatrixMode(GL_MODELVIEW); // définit que la matrice est la model view
    glLoadIdentity(); // charge la matrice identité
    gluLookAt(0.0, 0.0, 5.0, // position de la caméra
              0.0, 0.0, 0.0, // vers où la caméra pointe
              0.0, 1.0, 0.0); // vecteur view-up

    glMatrixMode(GL_PROJECTION); // définit que la matrice est la projection
    glLoadIdentity(); // charge la matrice identité
    glOrtho(-2.0, 2.0, -2.0, 2.0, -5.0, 5.0);

    lighting_example1_2_3();
    return 0;
}

void display() {
    // efface le tampon
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- Pour l'Exemple 4: Propriétés de la surface ---
    // Décommentez les lignes suivantes pour définir les propriétés du matériau de la sphère
    // float kd[4] = {0.65f, 0.65f, 0.0f, 1.0f};
    // float ks[4] = {0.9f, 0.9f, 0.9f, 1.0f};
    // float ns = 65.0f;
    // glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, kd);
    // glMaterialfv(GL_FRONT, GL_SPECULAR, ks);
    // glMaterialf(GL_FRONT, GL_SHININESS, ns);


    // définit que la matrice est la model view
    glMatrixMode(GL_MODELVIEW);
    glutSolidSphere(1.5, 40, 40);

    // force le dessin des primitives
    glFlush();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv); // initialise GLUT
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB | GLUT_DEPTH); // configure le mode d'affichage
    glutInitWindowPosition(200, 0); // définit la position initiale de la fenêtre
    glutInitWindowSize(400, 400); // configure la largeur et la hauteur de la fenêtre d'affichage
    glutCreateWindow("Acerte os arruaceiros"); // crée la fenêtre d'affichage

    init(); // exécute la fonction d'initialisation
    glutDisplayFunc(display); // enregistre la fonction de rappel d'affichage
    glutMainLoop(); // entre dans la boucle principale de traitement des événements GLUT
    return 0;
}