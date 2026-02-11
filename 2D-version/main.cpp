// 2D "Avoid the Obstacles" game using GLUT / OpenGL
// Now includes:
// - Main menu (Start / Exit)
// - Character Select screen (Car, Dinosaur, Cat)
// - Timer, score, speed-up every 30 seconds
// - Patterned background
// - Player composed of multiple shapes

#include <GL/glut.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

bool isDragging = false;


const int WINDOW_WIDTH  = 800;
const int WINDOW_HEIGHT = 600;

// ----------------- GAME STATE -----------------
enum GameState {
    STATE_MENU,
    STATE_CHAR_SELECT,
    STATE_PLAYING,
    STATE_GAMEOVER
};
GameState gameState = STATE_MENU;

// ----------------- CHARACTERS -----------------
enum CharacterType {
    CHAR_CAR,
    CHAR_DINO,
    CHAR_CAT
};
CharacterType currentCharacter = CHAR_CAR;

// ----------------- PLAYER -----------------
float playerW = 60.0f;
float playerH = 50.0f;
float playerX = WINDOW_WIDTH / 2.0f - playerW / 2.0f;
float playerY = 40.0f;
float playerSpeedPixels = 300.0f; // pixels per second (horizontal)

// ----------------- OBSTACLES -----------------
struct Obstacle {
    float x, y;
    float w, h;
    bool active;
};

const int MAX_OBS = 20;
Obstacle obstacles[MAX_OBS];

float gameSpeed = 5.0f;           // logical speed
float spawnInterval = 1.0f;       // seconds between spawns
float spawnTimer   = 0.0f;

// ----------------- TIMER & SCORE -----------------
int   lastTimeMs            = 0;     // last frame time
float elapsedTime           = 0.0f;  // seconds since game start
float lastScoreTime         = 0.0f;  // when we last added to score
float lastSpeedIncreaseTime = 0.0f;  // when we last increased speed
int   score                 = 0;
float lastSpawnIncreaseTime = 0.0f;

const float GAME_DURATION = 60.0f;   // seconds of gameplay

// ----------------- UTILS -----------------
void drawString(void *font, const char *str, float x, float y) {
    glRasterPos2f(x, y);
    for (const char *c = str; *c != '\0'; ++c)
        glutBitmapCharacter(font, *c);
}
int getStringWidth(void *font, const char *str) {
    int w = 0;
    for (const char *c = str; *c != '\0'; ++c) {
        w += glutBitmapWidth(font, *c);
    }
    return w;
}

void drawStringCentered(void *font, const char *str, float centerX, float y) {
    int w = getStringWidth(font, str);
    float startX = centerX - w / 2.0f;
    drawString(font, str, startX, y);
}

// ----------------- RESET / INIT -----------------
void resetObstacles() {
    for (int i = 0; i < MAX_OBS; ++i) {
        obstacles[i].active = false;
    }
}

void resetGame() {
    resetObstacles();
    score                 = 0;
    elapsedTime           = 0.0f;
    lastScoreTime         = 0.0f;
    lastSpeedIncreaseTime = 0.0f;
    gameSpeed             = 5.0f;
    spawnTimer            = 0.0f;
    lastSpawnIncreaseTime = 0.0f;
    spawnInterval = 1.0f;
    playerX = WINDOW_WIDTH / 2.0f - playerW / 2.0f;
    playerY = 40.0f;
}

// ----------------- SPAWNING OBSTACLES -----------------
void spawnObstacle() {
    for (int i = 0; i < MAX_OBS; ++i) {
        if (!obstacles[i].active) {
            obstacles[i].active = true;
            obstacles[i].w = 60.0f;
            obstacles[i].h = 30.0f;
            obstacles[i].x = (float)(rand() % (WINDOW_WIDTH - (int)obstacles[i].w));
            obstacles[i].y = WINDOW_HEIGHT + obstacles[i].h;
            break;
        }
    }
}

// ----------------- COLLISION -----------------
bool checkCollision(const Obstacle &o) {
    if (!o.active) return false;

    if (playerX < o.x + o.w &&
        playerX + playerW > o.x &&
        playerY < o.y + o.h &&
        playerY + playerH > o.y) {
        return true;
    }
    return false;
}

// ----------------- DRAWING -----------------
void drawBackgroundPattern() {
    // Simple vertical striped pattern
    float stripeWidth = 50.0f;
    bool dark = false;
    for (float x = 0; x < WINDOW_WIDTH; x += stripeWidth) {
        if (dark)
            glColor3f(0.2f, 0.2f, 0.3f);
        else
            glColor3f(0.1f, 0.1f, 0.2f);
        glBegin(GL_QUADS);
        glVertex2f(x, 0.0f);
        glVertex2f(x + stripeWidth, 0.0f);
        glVertex2f(x + stripeWidth, WINDOW_HEIGHT);
        glVertex2f(x, WINDOW_HEIGHT);
        glEnd();
        dark = !dark;
    }
}

// Each character is made of a few shapes to satisfy "3 shapes" rule.
void drawPlayerCar() {
    float x = playerX;
    float y = playerY;
    float w = playerW;
    float h = playerH;           // overall “height” of car

    // scale factors for a 60x40-ish pixel design
    float sx = w / 60.0f;
    float sy = h / 40.0f;

    // ----- body (red) -----
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    // main rectangle
    glVertex2f(x + 0  * sx, y + 10 * sy);
    glVertex2f(x + 60 * sx, y + 10 * sy);
    glVertex2f(x + 60 * sx, y + 25 * sy);
    glVertex2f(x + 0  * sx, y + 25 * sy);
    glEnd();

    // rear part (to make it longer like sprite)
    glBegin(GL_QUADS);
    glVertex2f(x + 5  * sx, y + 25 * sy);
    glVertex2f(x + 55 * sx, y + 25 * sy);
    glVertex2f(x + 55 * sx, y + 32 * sy);
    glVertex2f(x + 5  * sx, y + 32 * sy);
    glEnd();

    // ----- roof & windows (light blue) -----
    glColor3f(0.7f, 0.9f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x + 10 * sx, y + 25 * sy);
    glVertex2f(x + 50 * sx, y + 25 * sy);
    glVertex2f(x + 45 * sx, y + 35 * sy);
    glVertex2f(x + 15 * sx, y + 35 * sy);
    glEnd();

    // center pillar (black line)
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(x + 30 * sx - 1 * sx, y + 25 * sy);
    glVertex2f(x + 30 * sx + 1 * sx, y + 25 * sy);
    glVertex2f(x + 30 * sx + 1 * sx, y + 35 * sy);
    glVertex2f(x + 30 * sx - 1 * sx, y + 35 * sy);
    glEnd();

    // ----- wheels (black) -----
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    // front wheel
    glVertex2f(x + 10 * sx, y + 0 * sy);
    glVertex2f(x + 22 * sx, y + 0 * sy);
    glVertex2f(x + 22 * sx, y + 10 * sy);
    glVertex2f(x + 10 * sx, y + 10 * sy);
    // rear wheel
    glVertex2f(x + 38 * sx, y + 0 * sy);
    glVertex2f(x + 50 * sx, y + 0 * sy);
    glVertex2f(x + 50 * sx, y + 10 * sy);
    glVertex2f(x + 38 * sx, y + 10 * sy);
    glEnd();

    // wheel centers (dark gray)
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(x + 14 * sx, y + 4 * sy);
    glVertex2f(x + 18 * sx, y + 4 * sy);
    glVertex2f(x + 18 * sx, y + 8 * sy);
    glVertex2f(x + 14 * sx, y + 8 * sy);

    glVertex2f(x + 42 * sx, y + 4 * sy);
    glVertex2f(x + 46 * sx, y + 4 * sy);
    glVertex2f(x + 46 * sx, y + 8 * sy);
    glVertex2f(x + 42 * sx, y + 8 * sy);
    glEnd();

    // ----- headlight (yellow) -----
    glColor3f(1.0f, 1.0f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(x + 0 * sx,  y + 15 * sy);
    glVertex2f(x + 5 * sx,  y + 15 * sy);
    glVertex2f(x + 5 * sx,  y + 20 * sy);
    glVertex2f(x + 0 * sx,  y + 20 * sy);
    glEnd();
}
void drawPlayerDino() {
    float x = playerX;
    float y = playerY;
    float w = playerW;
    float h = playerH + 10.0f;     // a little taller

    float sx = w / 60.0f;
    float sy = h / 50.0f;

    // main body color
    glColor3f(0.2f, 0.5f, 0.2f);

    // ----- body rectangle -----
    glBegin(GL_QUADS);
    glVertex2f(x + 10 * sx, y + 15 * sy);
    glVertex2f(x + 45 * sx, y + 15 * sy);
    glVertex2f(x + 45 * sx, y + 35 * sy);
    glVertex2f(x + 10 * sx, y + 35 * sy);
    glEnd();

    // ----- belly (lighter) -----
    glColor3f(0.6f, 0.8f, 0.4f);
    glBegin(GL_QUADS);
    glVertex2f(x + 22 * sx, y + 15 * sy);
    glVertex2f(x + 35 * sx, y + 15 * sy);
    glVertex2f(x + 35 * sx, y + 30 * sy);
    glVertex2f(x + 22 * sx, y + 30 * sy);
    glEnd();

    // ----- head -----
    glColor3f(0.2f, 0.5f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(x + 40 * sx, y + 30 * sy);
    glVertex2f(x + 60 * sx, y + 30 * sy);
    glVertex2f(x + 60 * sx, y + 45 * sy);
    glVertex2f(x + 40 * sx, y + 45 * sy);
    glEnd();

    // neck
    glBegin(GL_QUADS);
    glVertex2f(x + 35 * sx, y + 30 * sy);
    glVertex2f(x + 40 * sx, y + 30 * sy);
    glVertex2f(x + 40 * sx, y + 40 * sy);
    glVertex2f(x + 35 * sx, y + 40 * sy);
    glEnd();

    // ----- tail -----
    glBegin(GL_TRIANGLES);
    glVertex2f(x + 10 * sx, y + 30 * sy);
    glVertex2f(x + 0  * sx, y + 26 * sy);
    glVertex2f(x + 0  * sx, y + 34 * sy);
    glEnd();

    // ----- legs -----
    glBegin(GL_QUADS);
    // front leg
    glVertex2f(x + 28 * sx, y + 0  * sy);
    glVertex2f(x + 35 * sx, y + 0  * sy);
    glVertex2f(x + 35 * sx, y + 15 * sy);
    glVertex2f(x + 28 * sx, y + 15 * sy);
    // back leg
    glVertex2f(x + 15 * sx, y + 0  * sy);
    glVertex2f(x + 22 * sx, y + 0  * sy);
    glVertex2f(x + 22 * sx, y + 15 * sy);
    glVertex2f(x + 15 * sx, y + 15 * sy);
    glEnd();

    // little arm
    glBegin(GL_QUADS);
    glVertex2f(x + 38 * sx, y + 22 * sy);
    glVertex2f(x + 42 * sx, y + 22 * sy);
    glVertex2f(x + 42 * sx, y + 26 * sy);
    glVertex2f(x + 38 * sx, y + 26 * sy);
    glEnd();

    // ----- face details -----
    // eye
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(x + 52 * sx, y + 38 * sy);
    glVertex2f(x + 54 * sx, y + 38 * sy);
    glVertex2f(x + 54 * sx, y + 40 * sy);
    glVertex2f(x + 52 * sx, y + 40 * sy);
    glEnd();

    // mouth line
    glBegin(GL_LINES);
    glVertex2f(x + 48 * sx, y + 32 * sy);
    glVertex2f(x + 57 * sx, y + 32 * sy);
    glEnd();
}


void drawPlayerCat() {
    float x = playerX;
    float y = playerY;
    float w = playerW;
    float h = playerH + 10.0f;     // give a bit more height

    float sx = w / 50.0f;
    float sy = h / 40.0f;

    // ----- head -----
    glColor3f(0.4f, 0.4f, 0.4f);   // gray
    glBegin(GL_QUADS);
    glVertex2f(x + 5  * sx, y + 10 * sy);
    glVertex2f(x + 45 * sx, y + 10 * sy);
    glVertex2f(x + 45 * sx, y + 40 * sy);
    glVertex2f(x + 5  * sx, y + 40 * sy);
    glEnd();

    // ----- ears (dark gray) -----
    glColor3f(0.35f, 0.35f, 0.35f);
    glBegin(GL_TRIANGLES);
    // left ear
    glVertex2f(x + 8  * sx, y + 40 * sy);
    glVertex2f(x + 15 * sx, y + 52 * sy);
    glVertex2f(x + 22 * sx, y + 40 * sy);
    // right ear
    glVertex2f(x + 28 * sx, y + 40 * sy);
    glVertex2f(x + 35 * sx, y + 52 * sy);
    glVertex2f(x + 42 * sx, y + 40 * sy);
    glEnd();

    // inner ears (pink)
    glColor3f(1.0f, 0.7f, 0.8f);
    glBegin(GL_TRIANGLES);
    glVertex2f(x + 11 * sx, y + 40 * sy);
    glVertex2f(x + 15 * sx, y + 49 * sy);
    glVertex2f(x + 19 * sx, y + 40 * sy);

    glVertex2f(x + 31 * sx, y + 40 * sy);
    glVertex2f(x + 35 * sx, y + 49 * sy);
    glVertex2f(x + 39 * sx, y + 40 * sy);
    glEnd();

    // ----- eyes (black squares with white pixel) -----
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    // left eye
    glVertex2f(x + 15 * sx, y + 25 * sy);
    glVertex2f(x + 22 * sx, y + 25 * sy);
    glVertex2f(x + 22 * sx, y + 32 * sy);
    glVertex2f(x + 15 * sx, y + 32 * sy);
    // right eye
    glVertex2f(x + 28 * sx, y + 25 * sy);
    glVertex2f(x + 35 * sx, y + 25 * sy);
    glVertex2f(x + 35 * sx, y + 32 * sy);
    glVertex2f(x + 28 * sx, y + 32 * sy);
    glEnd();

    // white highlight in eyes
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x + 16 * sx, y + 30 * sy);
    glVertex2f(x + 18 * sx, y + 30 * sy);
    glVertex2f(x + 18 * sx, y + 32 * sy);
    glVertex2f(x + 16 * sx, y + 32 * sy);

    glVertex2f(x + 29 * sx, y + 30 * sy);
    glVertex2f(x + 31 * sx, y + 30 * sy);
    glVertex2f(x + 31 * sx, y + 32 * sy);
    glVertex2f(x + 29 * sx, y + 32 * sy);
    glEnd();

    // ----- nose (pink) -----
    glColor3f(1.0f, 0.7f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(x + 23 * sx, y + 22 * sy);
    glVertex2f(x + 27 * sx, y + 22 * sy);
    glVertex2f(x + 27 * sx, y + 24 * sy);
    glVertex2f(x + 23 * sx, y + 24 * sy);
    glEnd();

    // small mouth (black)
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex2f(x + 25 * sx, y + 22 * sy);
    glVertex2f(x + 25 * sx, y + 18 * sy);
    glVertex2f(x + 22 * sx, y + 18 * sy);
    glVertex2f(x + 28 * sx, y + 18 * sy);
    glEnd();

    // blush (pink squares)
    glColor3f(1.0f, 0.6f, 0.7f);
    glBegin(GL_QUADS);
    glVertex2f(x + 10 * sx, y + 18 * sy);
    glVertex2f(x + 14 * sx, y + 18 * sy);
    glVertex2f(x + 14 * sx, y + 22 * sy);
    glVertex2f(x + 10 * sx, y + 22 * sy);

    glVertex2f(x + 36 * sx, y + 18 * sy);
    glVertex2f(x + 40 * sx, y + 18 * sy);
    glVertex2f(x + 40 * sx, y + 22 * sy);
    glVertex2f(x + 36 * sx, y + 22 * sy);
    glEnd();

    // whiskers
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_LINES);
    // left
    glVertex2f(x + 10 * sx, y + 24 * sy);
    glVertex2f(x + 5  * sx, y + 24 * sy);
    glVertex2f(x + 10 * sx, y + 20 * sy);
    glVertex2f(x + 5  * sx, y + 20 * sy);
    // right
    glVertex2f(x + 40 * sx, y + 24 * sy);
    glVertex2f(x + 45 * sx, y + 24 * sy);
    glVertex2f(x + 40 * sx, y + 20 * sy);
    glVertex2f(x + 45 * sx, y + 20 * sy);
    glEnd();
}


void drawPlayer() {
    switch (currentCharacter) {
        case CHAR_CAR:  drawPlayerCar();  break;
        case CHAR_DINO: drawPlayerDino(); break;
        case CHAR_CAT:  drawPlayerCat();  break;
    }
}

void drawObstacles() {
    glColor3f(0.8f, 0.1f, 0.1f);
    for (int i = 0; i < MAX_OBS; ++i) {
        if (!obstacles[i].active) continue;
        glBegin(GL_QUADS);
        glVertex2f(obstacles[i].x,             obstacles[i].y);
        glVertex2f(obstacles[i].x + obstacles[i].w, obstacles[i].y);
        glVertex2f(obstacles[i].x + obstacles[i].w, obstacles[i].y + obstacles[i].h);
        glVertex2f(obstacles[i].x,             obstacles[i].y + obstacles[i].h);
        glEnd();
    }
}

void drawHUD() {
    char buffer[64];

    glColor3f(1.0f, 1.0f, 1.0f);
    sprintf(buffer, "Score: %d", score);
    drawString(GLUT_BITMAP_HELVETICA_18, buffer, 10.0f, WINDOW_HEIGHT - 30.0f);

    // Timer counts UP
    sprintf(buffer, "Time: %.0f", elapsedTime);
    drawString(GLUT_BITMAP_HELVETICA_18, buffer, 10.0f, WINDOW_HEIGHT - 60.0f);

    sprintf(buffer, "Speed: %.1f", gameSpeed);
    drawString(GLUT_BITMAP_HELVETICA_18, buffer, 10.0f, WINDOW_HEIGHT - 90.0f);
}


// ----------------- MENUS -----------------
void drawMenu() {
    glClear(GL_COLOR_BUFFER_BIT);

    drawBackgroundPattern();

    glColor3f(1.0f, 1.0f, 1.0f);
    drawString(GLUT_BITMAP_HELVETICA_18,
               "AVOID THE OBSTACLES - MAIN MENU",
               WINDOW_WIDTH / 2.0f - 180.0f,
               WINDOW_HEIGHT - 100.0f);

    // Start button
    float btnW = 200.0f, btnH = 50.0f;
    float startX = WINDOW_WIDTH / 2.0f - btnW / 2.0f;
    float startY = WINDOW_HEIGHT / 2.0f + 30.0f;

    glColor3f(0.0f, 0.5f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(startX,          startY);
    glVertex2f(startX + btnW,   startY);
    glVertex2f(startX + btnW,   startY + btnH);
    glVertex2f(startX,          startY + btnH);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    drawString(GLUT_BITMAP_HELVETICA_18,
               "START",
               startX + 70.0f, startY + 18.0f);

    // Exit button
    float exitY = startY - 80.0f;
    glColor3f(0.8f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(startX,        exitY);
    glVertex2f(startX + btnW, exitY);
    glVertex2f(startX + btnW, exitY + btnH);
    glVertex2f(startX,        exitY + btnH);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    drawString(GLUT_BITMAP_HELVETICA_18,
               "EXIT",
               startX + 78.0f, exitY + 18.0f);

    glutSwapBuffers();
}

void drawCharacterSelect() {
    glClear(GL_COLOR_BUFFER_BIT);
    drawBackgroundPattern();

    glColor3f(1.0f, 1.0f, 1.0f);
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "CHOOSE YOUR CHARACTER",
                       WINDOW_WIDTH / 2.0f,
                       WINDOW_HEIGHT - 80.0f);

    // All boxes same size
    float cardW = 180.0f;
    float cardH = 160.0f;

    // Bottom of all cards (so they line up)
    float cardY = WINDOW_HEIGHT / 2.0f - cardH / 2.0f;

    // Positions (left, middle, right)
    float card1X = WINDOW_WIDTH / 2.0f - cardW - 40.0f; // Car
    float card2X = WINDOW_WIDTH / 2.0f - cardW / 2.0f;  // Dino
    float card3X = WINDOW_WIDTH / 2.0f + 40.0f;         // Cat

    float centerY = cardY + cardH / 2.0f;

    // ---- Card 1: CAR ----
    float c1CenterX = card1X + cardW / 2.0f;

    // Filled box
    glColor3f(0.1f, 0.4f, 0.7f);
    glBegin(GL_QUADS);
    glVertex2f(card1X,          cardY);
    glVertex2f(card1X + cardW,  cardY);
    glVertex2f(card1X + cardW,  cardY + cardH);
    glVertex2f(card1X,          cardY + cardH);
    glEnd();

    // Border
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(card1X,          cardY);
    glVertex2f(card1X + cardW,  cardY);
    glVertex2f(card1X + cardW,  cardY + cardH);
    glVertex2f(card1X,          cardY + cardH);
    glEnd();
    glLineWidth(1.0f);

    // Header centered above box
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "CAR",
                       c1CenterX,
                       cardY + cardH + 15.0f);

    // Character centered inside box
    float oldX = playerX, oldY = playerY;
    playerX = c1CenterX - playerW / 2.0f;
    playerY = centerY - playerH / 2.0f;
    drawPlayerCar();
    playerX = oldX; playerY = oldY;

    // ---- Card 2: DINOSAUR ----
    float c2CenterX = card2X + cardW / 2.0f;

    glColor3f(0.0f, 0.5f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(card2X,          cardY);
    glVertex2f(card2X + cardW,  cardY);
    glVertex2f(card2X + cardW,  cardY + cardH);
    glVertex2f(card2X,          cardY + cardH);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(card2X,          cardY);
    glVertex2f(card2X + cardW,  cardY);
    glVertex2f(card2X + cardW,  cardY + cardH);
    glVertex2f(card2X,          cardY + cardH);
    glEnd();
    glLineWidth(1.0f);

    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "DINOSAUR",
                       c2CenterX,
                       cardY + cardH + 15.0f);

    playerX = c2CenterX - playerW / 2.0f;
    playerY = centerY - playerH / 2.0f;
    drawPlayerDino();
    playerX = oldX; playerY = oldY;

    // ---- Card 3: CAT ----
    float c3CenterX = card3X + cardW / 2.0f;

    glColor3f(0.8f, 0.5f, 0.1f);
    glBegin(GL_QUADS);
    glVertex2f(card3X,          cardY);
    glVertex2f(card3X + cardW,  cardY);
    glVertex2f(card3X + cardW,  cardY + cardH);
    glVertex2f(card3X,          cardY + cardH);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(card3X,          cardY);
    glVertex2f(card3X + cardW,  cardY);
    glVertex2f(card3X + cardW,  cardY + cardH);
    glVertex2f(card3X,          cardY + cardH);
    glEnd();
    glLineWidth(1.0f);

    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "CAT",
                       c3CenterX,
                       cardY + cardH + 15.0f);

    playerX = c3CenterX - playerW / 2.0f;
    playerY = centerY - playerH / 2.0f;
    drawPlayerCat();
    playerX = oldX; playerY = oldY;

    glutSwapBuffers();
}

void drawGameOver() {
    glClear(GL_COLOR_BUFFER_BIT);

    drawBackgroundPattern();

    glColor3f(1.0f, 1.0f, 0.0f);
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "GAME OVER!",
                       WINDOW_WIDTH / 2.0f,
                       WINDOW_HEIGHT / 2.0f + 80.0f);

    char buffer[64];

    // Show final score
    sprintf(buffer, "Final Score: %d", score);
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       buffer,
                       WINDOW_WIDTH / 2.0f,
                       WINDOW_HEIGHT / 2.0f + 40.0f);

    // Show total survival time
    sprintf(buffer, "Time Alive: %.0f seconds", elapsedTime);
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       buffer,
                       WINDOW_WIDTH / 2.0f,
                       WINDOW_HEIGHT / 2.0f);

    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "Press SPACE to play again, or ESC to quit.",
                       WINDOW_WIDTH / 2.0f,
                       WINDOW_HEIGHT / 2.0f - 60.0f);

    glutSwapBuffers();
}


// ----------------- DISPLAY -----------------
void display() {
    if (gameState == STATE_MENU) {
        drawMenu();
        return;
    }
    if (gameState == STATE_CHAR_SELECT) {
        drawCharacterSelect();
        return;
    }
    if (gameState == STATE_GAMEOVER) {
        drawGameOver();
        return;
    }

    // STATE_PLAYING
    glClear(GL_COLOR_BUFFER_BIT);
    drawBackgroundPattern();
    drawPlayer();
    drawObstacles();
    drawHUD();

    glutSwapBuffers();
}

// ----------------- UPDATE (TIMER) -----------------
void update(int value) {
    (void)value;

    int currentTimeMs = glutGet(GLUT_ELAPSED_TIME);
    float dt = (currentTimeMs - lastTimeMs) / 1000.0f;
    if (dt < 0.0f) dt = 0.0f;
    lastTimeMs = currentTimeMs;

    if (gameState == STATE_PLAYING) {
        // Time & score
        elapsedTime += dt;
                        // --------- SCORE LOGIC ---------
            // Every 30 seconds, the score rate doubles:
            // 0–30s:  +1 every 1.0s
            // 30–60s: +1 every 0.5s
            // 60–90s: +1 every 0.25s
            // etc.
            int level = (int)(elapsedTime / 30.0f);  // 0,1,2,...
            if (level < 0) level = 0;

            // base interval = 1.0, then /2 each level
            float scoreInterval = 1.0f;
            for (int i = 0; i < level; ++i) {
                scoreInterval *= 0.5f;  // divide by 2 each 30 seconds
            }

            while (elapsedTime - lastScoreTime >= scoreInterval) {
                score += 1;
                lastScoreTime += scoreInterval;
            }

                        // --------- SPEED LOGIC ---------
            // Every 15 seconds: speed increases by 2
            while (elapsedTime - lastSpeedIncreaseTime >= 15.0f) {
                gameSpeed += 2.0f;
                lastSpeedIncreaseTime += 15.0f;
            }
        // Move obstacles downward (speed scaled)
        float pixelSpeed = gameSpeed * 40.0f; // convert logical speed to pixels
        for (int i = 0; i < MAX_OBS; ++i) {
            if (!obstacles[i].active) continue;
            obstacles[i].y -= pixelSpeed * dt;
            if (obstacles[i].y + obstacles[i].h < 0) {
                obstacles[i].active = false; // went off screen
            }
        }
                    while (elapsedTime - lastSpawnIncreaseTime >= 30.0f) {
                spawnInterval /= 1.5f;   // faster spawns
                lastSpawnIncreaseTime += 30.0f;
            }

        // Spawn new obstacles
        spawnTimer -= dt;
        if (spawnTimer <= 0.0f) {
            spawnObstacle();
            spawnTimer = spawnInterval;
        }

        // Check collisions
        for (int i = 0; i < MAX_OBS; ++i) {
            if (checkCollision(obstacles[i])) {
                gameState = STATE_GAMEOVER;
                break;
            }
        }

        // Timer controls game over

    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0); // ~60 FPS
}

// ----------------- INPUT -----------------
void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y; // unused

    if (key == 27) { // ESC
        exit(0);
    }

    if (gameState == STATE_GAMEOVER && key == ' ') {
        // Restart: go back to character select to choose again
        gameState = STATE_CHAR_SELECT;
    }
}

void specialKeyboard(int key, int x, int y) {
    (void)x; (void)y;

    if (gameState != STATE_PLAYING) return;

    if (key == GLUT_KEY_LEFT) {
        playerX -= playerSpeedPixels * 0.05f; // step
        if (playerX < 0) playerX = 0;
    } else if (key == GLUT_KEY_RIGHT) {
        playerX += playerSpeedPixels * 0.05f;
        if (playerX + playerW > WINDOW_WIDTH)
            playerX = WINDOW_WIDTH - playerW;
    }
}

void mouse(int button, int state, int x, int y) {
    int my = WINDOW_HEIGHT - y; // convert to ortho coords (for menus)

    // ---------- PLAYING: drag to move ----------
    if (gameState == STATE_PLAYING) {
        if (button == GLUT_LEFT_BUTTON) {
            if (state == GLUT_DOWN) {
                isDragging = true;

                // Place player under mouse X immediately
                playerX = x - playerW / 2.0f;
                if (playerX < 0) playerX = 0;
                if (playerX + playerW > WINDOW_WIDTH)
                    playerX = WINDOW_WIDTH - playerW;
            }
            else if (state == GLUT_UP) {
                isDragging = false;
            }
        }
        return; // don't treat clicks as menu clicks while playing
    }

    // ---------- MENU & CHAR SELECT: normal click handling ----------
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

    if (gameState == STATE_MENU) {
        float btnW = 200.0f, btnH = 50.0f;
        float startX = WINDOW_WIDTH / 2.0f - btnW / 2.0f;
        float startY = WINDOW_HEIGHT / 2.0f + 30.0f;
        float exitY  = startY - 80.0f;

        // Start button -> character select
        if (x >= startX && x <= startX + btnW &&
            my >= startY && my <= startY + btnH) {
            gameState = STATE_CHAR_SELECT;
            return;
        }

        // Exit button
        if (x >= startX && x <= startX + btnW &&
            my >= exitY && my <= exitY + btnH) {
            exit(0);
        }
    }
    else if (gameState == STATE_CHAR_SELECT) {
        float cardW = 180.0f;
        float cardH = 160.0f;
        float cardY = WINDOW_HEIGHT / 2.0f - cardH / 2.0f;

        float card1X = WINDOW_WIDTH / 2.0f - cardW - 40.0f;
        float card2X = WINDOW_WIDTH / 2.0f - cardW / 2.0f;
        float card3X = WINDOW_WIDTH / 2.0f + 40.0f;

        // CAR
        if (x >= card1X && x <= card1X + cardW &&
            my >= cardY && my <= cardY + cardH) {
            currentCharacter = CHAR_CAR;
            resetGame();
            gameState = STATE_PLAYING;
        }
        // DINO
        else if (x >= card2X && x <= card2X + cardW &&
                 my >= cardY && my <= cardY + cardH) {
            currentCharacter = CHAR_DINO;
            resetGame();
            gameState = STATE_PLAYING;
        }
        // CAT
        else if (x >= card3X && x <= card3X + cardW &&
                 my >= cardY && my <= cardY + cardH) {
            currentCharacter = CHAR_CAT;
            resetGame();
            gameState = STATE_PLAYING;
        }
    }
}
void mouseMotion(int x, int y) {
    if (!isDragging || gameState != STATE_PLAYING)
        return;

    // Move player to follow mouse X
    playerX = x - playerW / 2.0f;

    // Clamp to window
    if (playerX < 0) playerX = 0;
    if (playerX + playerW > WINDOW_WIDTH)
        playerX = WINDOW_WIDTH - playerW;
}
// ----------------- RESHAPE -----------------
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ----------------- MAIN -----------------
int main(int argc, char **argv) {
    srand((unsigned int)time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("Avoid the Obstacles - 2D");

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    resetGame();
    lastTimeMs = glutGet(GLUT_ELAPSED_TIME);

      glutDisplayFunc(display);
      glutReshapeFunc(reshape);
      glutKeyboardFunc(keyboard);
      glutSpecialFunc(specialKeyboard);
      glutMouseFunc(mouse);
      glutMotionFunc(mouseMotion);   // NEW
      glutTimerFunc(16, update, 0);

    glutMainLoop();
    return 0;
}
