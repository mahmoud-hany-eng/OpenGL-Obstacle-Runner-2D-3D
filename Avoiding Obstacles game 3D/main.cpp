#include <GL/glut.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")



const int WINDOW_WIDTH  = 800;
const int WINDOW_HEIGHT = 600;

// ------------- GAME STATE -------------
enum GameState {
    STATE_MENU,
    STATE_CHAR_SELECT,
    STATE_PLAYING,
    STATE_GAMEOVER
};
GameState gameState = STATE_MENU;

enum CharacterType {
    CHAR_CAR,
    CHAR_DINO,
    CHAR_CAT
};
CharacterType currentCharacter = CHAR_CAR;
// ---------- POWERUPS ----------
enum PowerupType {
    PWR_NONE,
    PWR_SCORE_X2,
    PWR_SLOW_HALF,
    PWR_INVINCIBLE
};

PowerupType activePowerup   = PWR_NONE;
bool        choosingPowerup = false;     // true when player can click one
float       powerupTimer    = 0.0f;      // counts 0..5 while active
float       nextPowerupTime = 10.0f;     // first offer at 10 seconds

// We separate "base" speed from the actual speed used in movement
float baseGameSpeed = 10.0f;            // increases every 15s
// ------------- LANE / PLAYER -------------
const int   NUM_LANES    = 5;
const float LANE_SPACING = 4.0f;   // world units
int   playerLane = NUM_LANES / 2;  // start in middle lane (index 2)
float playerX    = 0.0f;
float playerY    = 0.5f;
float playerZ    = 0.0f;
float playerSize = 1.2f;           // cube size for collision
float roadOffset = 0.0f;   // how far the road pattern has scrolled
float animTime   = 0.0f;  // global animation time
float carBob     = 0.0f;  // vertical bob for car
float legSwing   = 0.0f;  // angle (degrees) for legs (dino + cat)
int currentWindowWidth  = WINDOW_WIDTH;
int currentWindowHeight = WINDOW_HEIGHT;
float lastShieldHitTime = -100.0f;   // time of last shield usage

// ------------- OBSTACLES -------------
struct Obstacle {
    int   lane;
    float z;       // world Z position
    bool  active;
};

// ---------- HEART (extra lives) ----------
struct HeartPickup {
    int   lane;
    float z;
    bool  active;
};

HeartPickup heartPickup;
int   heartCount          = 0;
const int MAX_HEARTS      = 3;
float nextHeartSpawnTime  = 20.0f;   // first spawn between 20..40s


const int MAX_OBS = 40;
Obstacle obstacles[MAX_OBS];

float obstacleLength = 2.5f;       // depth for collision
float spawnInterval  = 1.0f;       // seconds between spawns
float spawnTimer     = 0.0f;

// ------------- TIME / SCORE / SPEED -------------
int   lastTimeMs              = 0;
float elapsedTime             = 0.0f;
float lastScoreTime           = 0.0f;
float lastSpeedIncreaseTime   = 0.0f;
float lastSpawnIncreaseTime   = 0.0f;
int   score                   = 0;
float gameSpeed               = 10.0f;  // forward speed (world units per second)

// ------------- INPUT -------------
bool isDragging = false;

// ------------- TEXT UTILS -------------
void drawString(void *font, const char *str, float x, float y) {
    glRasterPos2f(x, y);
    for (const char *c = str; *c != '\0'; ++c)
        glutBitmapCharacter(font, *c);
}
int getStringWidth(void *font, const char *str) {
    int w = 0;
    for (const char *c = str; *c != '\0'; ++c)
        w += glutBitmapWidth(font, *c);
    return w;
}
void drawStringCentered(void *font, const char *str, float centerX, float y) {
    int w = getStringWidth(font, str);
    drawString(font, str, centerX - w / 2.0f, y);
}

// ------------- RESET / INIT -------------
void resetObstacles() {
    for (int i = 0; i < MAX_OBS; ++i)
        obstacles[i].active = false;
}

void recalcPlayerX() {
    // lanes: 0..4 mapped to x = -2*spacing .. +2*spacing
    playerX = (playerLane - (NUM_LANES - 1) / 2.0f) * LANE_SPACING;
}

void resetGame() {
    resetObstacles();
    playerLane = NUM_LANES / 2;
    recalcPlayerX();
    playerY = 0.5f;
    playerZ = 0.0f;
      lastShieldHitTime = -100.0f;


      // hearts
    heartPickup.active   = false;
    heartPickup.lane     = 0;
    heartPickup.z        = -80.0f;
    heartCount           = 0;
    nextHeartSpawnTime   = 20.0f;   // first spawn window starts at t≈20

    score                 = 0;
    elapsedTime           = 0.0f;
    lastScoreTime         = 0.0f;
    lastSpeedIncreaseTime = 0.0f;
    lastSpawnIncreaseTime = 0.0f;

    baseGameSpeed = 10.0f;   // <-- new
    gameSpeed     = baseGameSpeed;

    spawnInterval = 1.0f;
    spawnTimer    = 0.0f;
    roadOffset    = 0.0f;

    animTime = 0.0f;
    carBob   = 0.0f;
    legSwing = 0.0f;

    // powerups
    activePowerup   = PWR_NONE;
    choosingPowerup = false;
    powerupTimer    = 0.0f;
    nextPowerupTime = 10.0f;
}


// ------------- SPAWN OBSTACLES -------------
void spawnObstacle() {
    for (int i = 0; i < MAX_OBS; ++i) {
        if (!obstacles[i].active) {
            obstacles[i].active = true;
            obstacles[i].lane   = rand() % NUM_LANES;
            obstacles[i].z      = -80.0f;   // spawn far ahead
            break;
        }
    }
}

// ------------- COLLISION -------------
bool checkCollision(const Obstacle &o) {
    if (!o.active) return false;
    if (o.lane != playerLane) return false;

    // Overlap along Z axis
    float halfPlayer = playerSize * 0.5f;
    float halfObs    = obstacleLength * 0.5f;
    float minZ = playerZ - halfPlayer - halfObs;
    float maxZ = playerZ + halfPlayer + halfObs;

    if (o.z >= minZ && o.z <= maxZ)
        return true;
    return false;
}

// ------------- PROJECTION HELPERS -------------
void set2D() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
}

void set3D() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)WINDOW_WIDTH / (double)WINDOW_HEIGHT, 0.1, 500.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_DEPTH_TEST);
}
void drawShieldPickup3D(float x, float z)
{
    glPushMatrix();

    // --- Position + bobbing animation ---
    float bob = sinf(elapsedTime * 4.0f) * 0.25f;   // up/down motion
    glTranslatef(x, 1.3f + bob, z);
      glRotatef(180.0f, 1, 0, 0);   // flip shield upside-down

    // --- Rotate around the vertical axis (like your drawing) ---
    float rot = fmodf(elapsedTime * 60.0f, 360.0f);
    glRotatef(rot, 0.0f, 1.0f, 0.0f);               // spin around Y

    // slight tilt so it's visible
    glRotatef(-20.0f, 1.0f, 0.0f, 0.0f);

    // --- Scale ---
    glScalef(1.6f, 1.6f, 1.6f);

    // ======================================
    //           DRAW THE SHIELD
    // ======================================

    // FRONT (blue shield surface)
    glBegin(GL_POLYGON);
        glColor3f(0.15f, 0.50f, 1.0f);  // main blue
        glVertex3f( 0.0f,  0.60f, 0.0f);
        glVertex3f( 0.45f, 0.20f, 0.0f);
        glVertex3f( 0.32f,-0.45f, 0.0f);
        glVertex3f(-0.32f,-0.45f, 0.0f);
        glVertex3f(-0.45f, 0.20f, 0.0f);
    glEnd();

    // highlight (brighter blue)
    glBegin(GL_POLYGON);
        glColor3f(0.40f, 0.75f, 1.0f);
        glVertex3f( 0.0f,  0.50f, 0.001f);
        glVertex3f( 0.30f, 0.15f, 0.001f);
        glVertex3f( 0.20f,-0.30f, 0.001f);
        glVertex3f(-0.20f,-0.30f, 0.001f);
        glVertex3f(-0.30f, 0.15f, 0.001f);
    glEnd();

    // METAL BORDER (silver)
    glLineWidth(6);
    glBegin(GL_LINE_LOOP);
        glColor3f(0.85f, 0.85f, 0.90f);  // shiny silver
        glVertex3f( 0.0f,  0.60f, 0.0f);
        glVertex3f( 0.45f, 0.20f, 0.0f);
        glVertex3f( 0.32f,-0.45f, 0.0f);
        glVertex3f(-0.32f,-0.45f, 0.0f);
        glVertex3f(-0.45f, 0.20f, 0.0f);
    glEnd();

    // BACK SIDE (dark silver)
    glBegin(GL_POLYGON);
        glColor3f(0.4f, 0.4f, 0.45f);
        glVertex3f( 0.0f,  0.60f, -0.05f);
        glVertex3f( 0.45f, 0.20f, -0.05f);
        glVertex3f( 0.32f,-0.45f, -0.05f);
        glVertex3f(-0.32f,-0.45f, -0.05f);
        glVertex3f(-0.45f, 0.20f, -0.05f);
    glEnd();
    // after drawing the shield border
glLineWidth(1.0f);   // reset to default thin lines
glPopMatrix();


    glPopMatrix();
}


// ------------- 3D DRAWING -------------
void drawRunway() {
    glColor3f(0.15f, 0.15f, 0.18f);

    glBegin(GL_QUADS);
    glVertex3f(-LANE_SPACING * 3, 0.0f,  20.0f);
    glVertex3f( LANE_SPACING * 3, 0.0f,  20.0f);
    glVertex3f( LANE_SPACING * 3, 0.0f, -200.0f);
    glVertex3f(-LANE_SPACING * 3, 0.0f, -200.0f);
    glEnd();

     // lane lines (patterned, scrolling like a treadmill)
    float stripeSpacing = 6.0f;  // distance between dash groups
    float stripeLength  = 3.0f;  // length of each dash
    float offset        = fmod(roadOffset, stripeSpacing);

    glLineWidth(2.0f);
    glColor3f(0.8f, 0.8f, 0.8f);

    for (int i = 1; i < NUM_LANES; ++i) {
        float x = (i - (NUM_LANES / 2.0f)) * LANE_SPACING;

        glBegin(GL_LINES);
        // start at 20 + offset so all dashes move towards camera (+Z)
        for (float z = 20.0f + offset; z > -200.0f; z -= stripeSpacing) {
            glVertex3f(x, 0.01f, z);
            glVertex3f(x, 0.01f, z - stripeLength);
        }
        glEnd();
    }

    glLineWidth(1.0f);
}
// Draw one spike pointing along +Z, starting ON the sphere surface
void drawSpikeForward(float radius, float spikeLen, float spikeR) {
    glPushMatrix();
    // Move to sphere surface: base of cone right on the sphere
    glTranslatef(0.0f, 0.0f, radius);
    // Cone axis is +Z, base at (0,0,0), tip at (0,0,spikeLen)
    glutSolidCone(spikeR, spikeLen, 12, 2);
    glPopMatrix();
}

void drawObstacle(const Obstacle &o) {
    if (!o.active) return;

    // lane position -> X, fixed height -> Y, Z from obstacle
    float x = (o.lane - (NUM_LANES - 1) / 2.0f) * LANE_SPACING;
    float y = 1.2f;          // a bit above ground
    float radius  = 1.0f;    // sphere radius
    float spikeLen = 0.8f;   // length of each spike
    float spikeR   = 0.25f;  // radius of each spike base

    glPushMatrix();
    glTranslatef(x, y, o.z);

    // -------- central black sphere --------
    glColor3f(0.02f, 0.02f, 0.02f);      // dark red core
    glutSolidSphere(radius, 18, 18);

    // slightly brighter highlight sphere (optional)
    glColor3f(0.3f, 0.0f, 0.0f);
    glutWireSphere(radius * 1.01f, 10, 10);

    // -------- spikes around the sphere --------
    glColor3f(1.0f, 0.25f, 0.25f);    // brighter red for spikes

    // front / back / up / down / left / right spikes
    glPushMatrix();                   // +Z
    drawSpikeForward(radius, spikeLen, spikeR);
    glPopMatrix();

    glPushMatrix();                   // -Z
    glRotatef(180.0f, 0, 1, 0);
    drawSpikeForward(radius, spikeLen, spikeR);
    glPopMatrix();

    glPushMatrix();                   // +Y
    glRotatef(-90.0f, 1, 0, 0);
    drawSpikeForward(radius, spikeLen, spikeR);
    glPopMatrix();

    glPushMatrix();                   // -Y
    glRotatef(90.0f, 1, 0, 0);
    drawSpikeForward(radius, spikeLen, spikeR);
    glPopMatrix();

    glPushMatrix();                   // +X
    glRotatef(-90.0f, 0, 1, 0);
    drawSpikeForward(radius, spikeLen, spikeR);
    glPopMatrix();

    glPushMatrix();                   // -X
    glRotatef(90.0f, 0, 1, 0);
    drawSpikeForward(radius, spikeLen, spikeR);
    glPopMatrix();

    // ring of spikes around middle
    for (int i = 0; i < 8; ++i) {
        float ang = i * 45.0f;        // 0,45,...315
        glPushMatrix();
        glRotatef(ang, 0, 1, 0);      // rotate around Y
        drawSpikeForward(radius, spikeLen, spikeR);
        glPopMatrix();
    }

    // upper and lower tilted rings
    for (int i = 0; i < 6; ++i) {
        float ang = i * 60.0f;

        // upper ring
        glPushMatrix();
        glRotatef(25.0f, 1, 0, 0);    // tilt up
        glRotatef(ang, 0, 1, 0);
        drawSpikeForward(radius, spikeLen, spikeR);
        glPopMatrix();

        // lower ring
        glPushMatrix();
        glRotatef(-25.0f, 1, 0, 0);   // tilt down
        glRotatef(ang, 0, 1, 0);
        drawSpikeForward(radius, spikeLen, spikeR);
        glPopMatrix();
    }

    glPopMatrix();
}


void drawObstacles3D() {
    for (int i = 0; i < MAX_OBS; ++i)
        drawObstacle(obstacles[i]);
}
// Simple wheel model for the car
void drawCarWheel() {
    glPushMatrix();
    // cylinder lying sideways (axis along X)
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    glutSolidTorus(0.10f, 0.25f, 12, 16);  // inner radius, outer radius
    glPopMatrix();
}

void drawCarBackModel() {
    glPushMatrix();

    // vertical bobbing to simulate engine vibration
    glTranslatef(0.0f, carBob, 0.0f);

    // ===== BODY =====
    // base body
    glColor3f(1.0f, 0.0f, 0.0f); // red
    glPushMatrix();
    glScalef(1.8f, 0.7f, 3.0f);  // wider in X, longer in Z (forward)
    glutSolidCube(1.0f);
    glPopMatrix();

    // roof (toward the front, -Z)
    glColor3f(0.8f, 0.8f, 0.9f);
    glPushMatrix();
    glTranslatef(0.0f, 0.6f, -0.4f);
    glScalef(1.2f, 0.6f, 1.2f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // back trunk (toward +Z, closest to camera)
    glColor3f(0.9f, 0.0f, 0.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.25f, 1.1f);
    glScalef(1.4f, 0.4f, 0.8f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // bumper
    glColor3f(0.2f, 0.2f, 0.2f);
    glPushMatrix();
    glTranslatef(0.0f, -0.1f, 1.6f);
    glScalef(1.2f, 0.2f, 0.3f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // ===== WHEELS =====
    glColor3f(0.05f, 0.05f, 0.05f); // dark wheel color

    // front-left (-x, -z)
    glPushMatrix();
    glTranslatef(-0.9f, -0.55f, -1.0f);
    drawCarWheel();
    glPopMatrix();

    // front-right (+x, -z)
    glPushMatrix();
    glTranslatef( 0.9f, -0.55f, -1.0f);
    drawCarWheel();
    glPopMatrix();

    // rear-left (-x, +z)
    glPushMatrix();
    glTranslatef(-0.9f, -0.55f, 1.0f);
    drawCarWheel();
    glPopMatrix();

    // rear-right (+x, +z)
    glPushMatrix();
    glTranslatef( 0.9f, -0.55f, 1.0f);
    drawCarWheel();
    glPopMatrix();

    glPopMatrix();
}


void drawDinoBackModel() {
    // main body – medium green
    glColor3f(0.2f, 0.6f, 0.2f);

    // torso
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.0f);
    glScalef(1.6f, 1.0f, 2.4f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // hips (rear, toward +Z)
    glPushMatrix();
    glTranslatef(0.0f, -0.1f, 0.9f);
    glScalef(1.4f, 0.9f, 1.0f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // tail going back/up (toward +Z, camera side)
    glPushMatrix();
    glTranslatef(0.0f, 0.2f, 1.8f);
    glRotatef(-30.0f, 1.0f, 0.0f, 0.0f);
    glScalef(0.4f, 0.4f, 1.8f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // shoulders (toward -Z)
    glPushMatrix();
    glTranslatef(0.0f, 0.2f, -0.9f);
    glScalef(1.3f, 1.0f, 1.0f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // head (forward, -Z)
    glPushMatrix();
    glTranslatef(0.0f, 0.9f, -1.1f);
    glScalef(0.9f, 0.9f, 0.9f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // ---------- legs with animation ----------
    glColor3f(0.2f, 0.6f, 0.2f);
    // right leg
    glPushMatrix();
    glTranslatef(0.5f, -0.7f, 0.4f);     // hip position
    glRotatef( legSwing, 1.0f, 0.0f, 0.0f); // swing forward/back along Z
    glScalef(0.4f, 1.2f, 0.4f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // left leg (opposite phase)
    glPushMatrix();
    glTranslatef(-0.5f, -0.7f, 0.4f);
    glRotatef(-legSwing, 1.0f, 0.0f, 0.0f);
    glScalef(0.4f, 1.2f, 0.4f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // ---------- dark green stripes ----------
    glColor3f(0.05f, 0.35f, 0.05f);

    // stripes across back
    for (int i = 0; i < 4; ++i) {
        float z = -0.7f + i * 0.7f; // from shoulders to hips
        glPushMatrix();
        glTranslatef(0.0f, 0.7f, z);
        glScalef(1.7f, 0.25f, 0.2f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    // stripes on tail
    for (int i = 0; i < 3; ++i) {
        float offset = -0.3f + i * 0.4f;
        glPushMatrix();
        glTranslatef(0.0f, 0.4f, 1.5f + offset);
        glScalef(0.5f, 0.2f, 0.4f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }
}

void drawCatBackModel() {
    // orange tabby body
    glColor3f(0.95f, 0.65f, 0.2f);   // orange
    glPushMatrix();
    glTranslatef(0.0f, -0.1f, 0.2f); // slightly toward camera
    glScalef(1.5f, 1.0f, 1.6f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // head (forward, -Z)
    glPushMatrix();
    glTranslatef(0.0f, 0.7f, -0.6f);
    glScalef(1.1f, 1.0f, 1.0f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // ears (on head, visible from back)
    glPushMatrix();
    glTranslatef(-0.4f, 1.2f, -0.6f);
    glScalef(0.3f, 0.6f, 0.3f);
    glutSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef( 0.4f, 1.2f, -0.6f);
    glScalef(0.3f, 0.6f, 0.3f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // tail (up and back toward camera, +Z)
    glPushMatrix();
    glTranslatef(0.0f, 0.4f, 1.0f);
    glRotatef(-60.0f, 1.0f, 0.0f, 0.0f);
    glScalef(0.2f, 0.2f, 1.6f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // little shoulders in front (-Z)
    glPushMatrix();
    glTranslatef(0.0f, 0.3f, -0.4f);
    glScalef(1.1f, 0.7f, 0.8f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // ---------- legs with animation ----------
    glColor3f(0.85f, 0.55f, 0.15f); // slightly darker orange for legs

    // right leg
    glPushMatrix();
    glTranslatef(0.4f, -0.7f, 0.3f);  // hip
    glRotatef( legSwing, 1.0f, 0.0f, 0.0f);
    glScalef(0.35f, 1.2f, 0.35f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // left leg (opposite phase)
    glPushMatrix();
    glTranslatef(-0.4f, -0.7f, 0.3f);
    glRotatef(-legSwing, 1.0f, 0.0f, 0.0f);
    glScalef(0.35f, 1.2f, 0.35f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // ---------- brown stripes on body ----------
    glColor3f(0.5f, 0.25f, 0.05f);   // dark brown

    // horizontal stripes across the back
    for (int i = 0; i < 3; ++i) {
        float z = -0.1f + i * 0.45f;
        glPushMatrix();
        glTranslatef(0.0f, 0.25f, z);
        glScalef(1.6f, 0.25f, 0.2f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    // stripes on tail
    for (int i = 0; i < 3; ++i) {
        float z = 1.1f + i * 0.3f;
        glPushMatrix();
        glTranslatef(0.0f, 0.55f, z);
        glScalef(0.25f, 0.25f, 0.25f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }
}
void drawShieldAuraAroundPlayer() {
    if (heartCount <= 0) return;  // no shields -> no aura
      bool isInvincible = (activePowerup == PWR_INVINCIBLE);


    int rings = heartCount;
    if (rings > MAX_HEARTS) rings = MAX_HEARTS;

    for (int i = 0; i < rings; ++i) {
        float baseRadius = 1.6f + i * 0.3f;
        float pulse      = 0.08f * sinf(elapsedTime * 5.0f + i * 1.5f);
        float radius     = baseRadius + pulse;

        // color: more shields = brighter / more blue
        float g = 0.8f + 0.05f * i;
        float b = 0.6f + 0.1f  * i;
        if (g > 1.0f) g = 1.0f;
        if (b > 1.0f) b = 1.0f;
        if (isInvincible) {
            glColor3f(1.0f, 1.0f, 0.0f);  // bright yellow for invincible
        } else {
            glColor3f(0.0f, g, b);
        }

        glColor3f(0.0f, g, b);

        // each ring rotates at a slightly different speed
        float angle = fmodf(elapsedTime * (60.0f + i * 20.0f), 360.0f);

        glPushMatrix();
        glRotatef(angle, 0.0f, 1.0f, 0.0f);

        // draw wire sphere as bubble shell
        glPushMatrix();
        glScalef(radius, radius, radius);
        glutWireSphere(1.0f, 14, 14);
        glPopMatrix();

        glPopMatrix();
    }
}

void drawPlayer3D() {
    glPushMatrix();
    glTranslatef(playerX, playerY + 1.0f, playerZ);

    // draw character
    switch (currentCharacter) {
        case CHAR_CAR:  drawCarBackModel();  break;
        case CHAR_DINO: drawDinoBackModel(); break;
        case CHAR_CAT:  drawCatBackModel();  break;
    }

    // draw shield aura *around* the character if we have shields
    drawShieldAuraAroundPlayer();   // <- uses the new fancy aura

    glPopMatrix();
}

void drawPowerupIcon(float x, float y, float w, float h,
                     const char* text,
                     PowerupType type,
                     bool highlight,
                     PowerupType activePowerup)
{
    // always use thin lines for HUD borders
    glLineWidth(1.0f);

    // Background colors per type
    if (type == PWR_SCORE_X2)       glColor3f(0.1f, 0.35f, 0.1f);
    else if (type == PWR_SLOW_HALF) glColor3f(0.1f, 0.1f, 0.35f);
    else if (type == PWR_INVINCIBLE)glColor3f(0.35f, 0.1f, 0.1f);

    // background panel
    glBegin(GL_QUADS);
        glVertex2f(x,     y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x,     y + h);
    glEnd();

    // border color
    if (highlight) {
        glColor3f(1.0f, 1.0f, 0.0f);  // yellow = selecting
    } else if (activePowerup == type) {
        glColor3f(0.0f, 1.0f, 0.0f);  // green = active
    } else {
        glColor3f(1.0f, 1.0f, 1.0f);  // normal white
    }

    // border
    glBegin(GL_LINE_LOOP);
        glVertex2f(x,     y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x,     y + h);
    glEnd();

    // text
    glColor3f(1,1,1);
    drawStringCentered(GLUT_BITMAP_HELVETICA_12,
                       text,
                       x + w/2.0f,
                       y + h/2.0f - 4.0f);
}

void drawPowerupsHUD() {
    float iconW   = 90.0f;
    float iconH   = 35.0f;
    float spacing = 8.0f;
    float margin  = 10.0f;

    float baseX = WINDOW_WIDTH  - margin - iconW;
    float baseY = margin;

    bool highlight = (choosingPowerup && activePowerup == PWR_NONE);

    // header
    glColor3f(1,1,1);
    drawStringCentered(GLUT_BITMAP_HELVETICA_12,
                       "POWERUPS",
                       baseX + iconW/2.0f,
                       baseY + iconH*3 + spacing*3 + 12.0f);

    // Icon Y positions
    float y1 = baseY;
    float y2 = baseY + iconH + spacing;
    float y3 = baseY + 2*(iconH + spacing);

    // Draw the icons (no lambda)
  drawPowerupIcon(baseX, y1, iconW, iconH, " E: x2 SCORE",
                PWR_SCORE_X2, highlight, activePowerup);
drawPowerupIcon(baseX, y2, iconW, iconH, " R: 1/2 SPEED",
                PWR_SLOW_HALF, highlight, activePowerup);
drawPowerupIcon(baseX, y3, iconW, iconH, " T: INVINCIBLE",
                PWR_INVINCIBLE, highlight, activePowerup);


    // hint text
    if (highlight) {
        glColor3f(1,1,0);
        drawStringCentered(GLUT_BITMAP_HELVETICA_12,
                           "Click a powerup!",
                           baseX + iconW/2.0f,
                           y3 + iconH + 16.0f);
    }
}
void drawShieldIcon2D(float x, float y, float size, bool active) {
    if (active) glColor3f(0.0f, 1.0f, 0.7f); // active = cyan-green
    else        glColor3f(0.3f, 0.3f, 0.3f); // inactive = grey

    float r = size * 0.5f;

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= 32; ++i) {
        float a = i * (2 * 3.14159f / 32);
        glVertex2f(x + cosf(a) * r, y + sinf(a) * r);
    }
    glEnd();
}


// ------------- HUD / MENUS (2D) -------------
void drawHUD() {
    set2D();
    char buffer[64];

    glColor3f(1.0f, 1.0f, 1.0f);
    sprintf(buffer, "Score: %d", score);
    drawString(GLUT_BITMAP_HELVETICA_18, buffer, 10.0f, WINDOW_HEIGHT - 30.0f);

    sprintf(buffer, "Time: %.0f", elapsedTime);
    drawString(GLUT_BITMAP_HELVETICA_18, buffer, 10.0f, WINDOW_HEIGHT - 60.0f);

    sprintf(buffer, "Speed: %.1f", gameSpeed);
    drawString(GLUT_BITMAP_HELVETICA_18, buffer, 10.0f, WINDOW_HEIGHT - 90.0f);

      // Hearts (top left under HUD text)
    float heartSize = 26.0f;
    float heartsY   = WINDOW_HEIGHT - 120.0f;
    float heartsX0  = 40.0f;

   for (int i = 0; i < MAX_HEARTS; i++) {
    drawShieldIcon2D(40 + i * 25, WINDOW_HEIGHT - 120, 20.0f, i < heartCount);
}
    drawPowerupsHUD();
}

void drawBackground2D() {
    // simple vertical stripes for patterned background
    float stripeWidth = 50.0f;
    bool dark = false;
    for (float x = 0; x < WINDOW_WIDTH; x += stripeWidth) {
        if (dark) glColor3f(0.2f, 0.2f, 0.3f);
        else      glColor3f(0.1f, 0.1f, 0.2f);
        glBegin(GL_QUADS);
        glVertex2f(x, 0.0f);
        glVertex2f(x + stripeWidth, 0.0f);
        glVertex2f(x + stripeWidth, WINDOW_HEIGHT);
        glVertex2f(x, WINDOW_HEIGHT);
        glEnd();
        dark = !dark;
    }
}
// Small collage of car/dino/cat icons in the background


void drawCarPreview2D(float cx, float cy, float s) {
    float w = 80.0f * s;
    float h = 35.0f * s;

    float x1 = cx - w / 2.0f;
    float y1 = cy - h / 2.0f;

    // main body (side)
    glColor3f(0.9f, 0.0f, 0.0f);           // bright red side
    glBegin(GL_QUADS);
    glVertex2f(x1,          y1 + h * 0.25f);
    glVertex2f(x1 + w,      y1 + h * 0.25f);
    glVertex2f(x1 + w,      y1 + h * 0.85f);
    glVertex2f(x1,          y1 + h * 0.85f);
    glEnd();

    // top of car (roof) – lighter red to look 3D
    glColor3f(1.0f, 0.25f, 0.25f);
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.20f, y1 + h * 0.55f);
    glVertex2f(x1 + w * 0.80f, y1 + h * 0.55f);
    glVertex2f(x1 + w * 0.70f, y1 + h * 0.95f);
    glVertex2f(x1 + w * 0.30f, y1 + h * 0.95f);
    glEnd();

    // windows
    glColor3f(0.8f, 0.9f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.25f, y1 + h * 0.60f);
    glVertex2f(x1 + w * 0.75f, y1 + h * 0.60f);
    glVertex2f(x1 + w * 0.68f, y1 + h * 0.90f);
    glVertex2f(x1 + w * 0.32f, y1 + h * 0.90f);
    glEnd();

    // wheels (with inner darker circle)
    float rw = h * 0.55f;
    float ry = y1 + h * 0.12f;

    glColor3f(0.05f, 0.05f, 0.05f);
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.20f - rw*0.5f, ry);
    glVertex2f(x1 + w * 0.20f + rw*0.5f, ry);
    glVertex2f(x1 + w * 0.20f + rw*0.5f, ry + rw);
    glVertex2f(x1 + w * 0.20f - rw*0.5f, ry + rw);

    glVertex2f(x1 + w * 0.80f - rw*0.5f, ry);
    glVertex2f(x1 + w * 0.80f + rw*0.5f, ry);
    glVertex2f(x1 + w * 0.80f + rw*0.5f, ry + rw);
    glVertex2f(x1 + w * 0.80f - rw*0.5f, ry + rw);
    glEnd();

    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.20f - rw*0.25f, ry + rw*0.25f);
    glVertex2f(x1 + w * 0.20f + rw*0.25f, ry + rw*0.25f);
    glVertex2f(x1 + w * 0.20f + rw*0.25f, ry + rw*0.75f);
    glVertex2f(x1 + w * 0.20f - rw*0.25f, ry + rw*0.75f);

    glVertex2f(x1 + w * 0.80f - rw*0.25f, ry + rw*0.25f);
    glVertex2f(x1 + w * 0.80f + rw*0.25f, ry + rw*0.25f);
    glVertex2f(x1 + w * 0.80f + rw*0.25f, ry + rw*0.75f);
    glVertex2f(x1 + w * 0.80f - rw*0.25f, ry + rw*0.75f);
    glEnd();

    // little front light
    glColor3f(1.0f, 1.0f, 0.3f);
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.02f, y1 + h * 0.40f);
    glVertex2f(x1 + w * 0.08f, y1 + h * 0.40f);
    glVertex2f(x1 + w * 0.08f, y1 + h * 0.60f);
    glVertex2f(x1 + w * 0.02f, y1 + h * 0.60f);
    glEnd();
}

void drawDinoPreview2D(float cx, float cy, float s) {
    float w = 80.0f * s;
    float h = 55.0f * s;
    float x1 = cx - w / 2.0f;
    float y1 = cy - h / 2.0f;

    // torso (side)
    glColor3f(0.2f, 0.6f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.15f, y1 + h * 0.30f);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.30f);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.70f);
    glVertex2f(x1 + w * 0.15f, y1 + h * 0.70f);
    glEnd();

    // darker belly / underside
    glColor3f(0.15f, 0.45f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.15f, y1 + h * 0.30f);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.30f);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.45f);
    glVertex2f(x1 + w * 0.15f, y1 + h * 0.45f);
    glEnd();

    // tail (backwards, up a bit)
    glColor3f(0.2f, 0.6f, 0.2f);
    glBegin(GL_TRIANGLES);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.55f);
    glVertex2f(x1 + w * 1.05f, y1 + h * 0.60f);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.40f);
    glEnd();

    // head (front)
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.02f, y1 + h * 0.55f);
    glVertex2f(x1 + w * 0.25f, y1 + h * 0.55f);
    glVertex2f(x1 + w * 0.25f, y1 + h * 0.85f);
    glVertex2f(x1 + w * 0.02f, y1 + h * 0.85f);
    glEnd();

    // small snout
    glBegin(GL_QUADS);
    glVertex2f(x1 - w * 0.05f, y1 + h * 0.60f);
    glVertex2f(x1 + w * 0.02f, y1 + h * 0.60f);
    glVertex2f(x1 + w * 0.02f, y1 + h * 0.78f);
    glVertex2f(x1 - w * 0.05f, y1 + h * 0.78f);
    glEnd();

    // stripes on back (dark green)
    glColor3f(0.05f, 0.35f, 0.05f);
    for (int i = 0; i < 3; ++i) {
        float t = 0.30f + i * 0.15f;
        glBegin(GL_QUADS);
        glVertex2f(x1 + w * (0.25f + i * 0.18f), y1 + h * 0.72f);
        glVertex2f(x1 + w * (0.40f + i * 0.18f), y1 + h * 0.72f);
        glVertex2f(x1 + w * (0.40f + i * 0.18f), y1 + h * 0.82f);
        glVertex2f(x1 + w * (0.25f + i * 0.18f), y1 + h * 0.82f);
        glEnd();
    }

    // legs (two, slightly offset to look 3D)
    glColor3f(0.18f, 0.55f, 0.18f);
    // back leg
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.55f, y1 + h * 0.05f);
    glVertex2f(x1 + w * 0.65f, y1 + h * 0.05f);
    glVertex2f(x1 + w * 0.65f, y1 + h * 0.35f);
    glVertex2f(x1 + w * 0.55f, y1 + h * 0.35f);
    glEnd();
    // front leg
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.35f, y1 + h * 0.05f);
    glVertex2f(x1 + w * 0.45f, y1 + h * 0.05f);
    glVertex2f(x1 + w * 0.45f, y1 + h * 0.35f);
    glVertex2f(x1 + w * 0.35f, y1 + h * 0.35f);
    glEnd();
}


void drawCatPreview2D(float cx, float cy, float s) {
    float w = 65.0f * s;
    float h = 50.0f * s;
    float x1 = cx - w / 2.0f;
    float y1 = cy - h / 2.0f;

    // body
    glColor3f(0.95f, 0.65f, 0.2f);   // orange
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.15f, y1 + h * 0.25f);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.25f);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.70f);
    glVertex2f(x1 + w * 0.15f, y1 + h * 0.70f);
    glEnd();

    // head (front)
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.02f, y1 + h * 0.45f);
    glVertex2f(x1 + w * 0.30f, y1 + h * 0.45f);
    glVertex2f(x1 + w * 0.30f, y1 + h * 0.85f);
    glVertex2f(x1 + w * 0.02f, y1 + h * 0.85f);
    glEnd();

    // ears
    glBegin(GL_TRIANGLES);
    glVertex2f(x1 + w * 0.06f, y1 + h * 0.85f);
    glVertex2f(x1 + w * 0.12f, y1 + h * 1.00f);
    glVertex2f(x1 + w * 0.18f, y1 + h * 0.85f);

    glVertex2f(x1 + w * 0.22f, y1 + h * 0.85f);
    glVertex2f(x1 + w * 0.28f, y1 + h * 1.00f);
    glVertex2f(x1 + w * 0.34f, y1 + h * 0.85f);
    glEnd();

    // tail (back, up)
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.45f);
    glVertex2f(x1 + w * 0.95f, y1 + h * 0.45f);
    glVertex2f(x1 + w * 0.95f, y1 + h * 0.95f);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.95f);
    glEnd();

    // darker belly
    glColor3f(0.85f, 0.55f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.15f, y1 + h * 0.25f);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.25f);
    glVertex2f(x1 + w * 0.85f, y1 + h * 0.40f);
    glVertex2f(x1 + w * 0.15f, y1 + h * 0.40f);
    glEnd();

    // stripes on body
    glColor3f(0.5f, 0.25f, 0.05f);
    for (int i = 0; i < 3; ++i) {
        float t = 0.30f + i * 0.12f;
        glBegin(GL_QUADS);
        glVertex2f(x1 + w * 0.35f, y1 + h * t);
        glVertex2f(x1 + w * 0.80f, y1 + h * t);
        glVertex2f(x1 + w * 0.80f, y1 + h * (t + 0.08f));
        glVertex2f(x1 + w * 0.35f, y1 + h * (t + 0.08f));
        glEnd();
    }

    // legs (two, side view)
    glColor3f(0.85f, 0.55f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(x1 + w * 0.35f, y1 + h * 0.05f);
    glVertex2f(x1 + w * 0.45f, y1 + h * 0.05f);
    glVertex2f(x1 + w * 0.45f, y1 + h * 0.25f);
    glVertex2f(x1 + w * 0.35f, y1 + h * 0.25f);

    glVertex2f(x1 + w * 0.60f, y1 + h * 0.05f);
    glVertex2f(x1 + w * 0.70f, y1 + h * 0.05f);
    glVertex2f(x1 + w * 0.70f, y1 + h * 0.25f);
    glVertex2f(x1 + w * 0.60f, y1 + h * 0.25f);
    glEnd();
}
void drawMenuCharacterCollage() {
    // We are already in 2D (set2D called in drawMenu)
    float startX = 80.0f;
    float startY = 110.0f;
    float stepX  = 120.0f;
    float stepY  = 80.0f;
    float scale  = 0.55f;

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 6; ++col) {
            float cx = startX + col * stepX;
            float cy = startY + row * stepY;

            // leave the center area a bit cleaner (where title/buttons are)
            if (cx > WINDOW_WIDTH * 0.30f && cx < WINDOW_WIDTH * 0.70f &&
                cy > WINDOW_HEIGHT * 0.30f && cy < WINDOW_HEIGHT * 0.80f) {
                continue;
            }

            int which = (row + col) % 3;
            if (which == 0)
                drawCarPreview2D(cx, cy, scale);
            else if (which == 1)
                drawDinoPreview2D(cx, cy, scale);
            else
                drawCatPreview2D(cx, cy, scale);
        }
    }
}

void drawMenu() {
    set2D();
    glClear(GL_COLOR_BUFFER_BIT);

    // base striped background
    drawBackground2D();

    // collage of small characters
    drawMenuCharacterCollage();

    // ----- central panel behind title + buttons -----
    float panelW = 360.0f;
    float panelH = 260.0f;
    float panelX = (WINDOW_WIDTH  - panelW) / 2.0f;
    float panelY = WINDOW_HEIGHT / 2.0f - panelH / 2.5f;

    glColor3f(0.03f, 0.05f, 0.12f);
    glBegin(GL_QUADS);
    glVertex2f(panelX,           panelY);
    glVertex2f(panelX + panelW,  panelY);
    glVertex2f(panelX + panelW,  panelY + panelH);
    glVertex2f(panelX,           panelY + panelH);
    glEnd();

    glColor3f(0.7f, 0.7f, 0.9f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(panelX,           panelY);
    glVertex2f(panelX + panelW,  panelY);
    glVertex2f(panelX + panelW,  panelY + panelH);
    glVertex2f(panelX,           panelY + panelH);
    glEnd();

    // ----- flashy title in a box -----
    float titleBoxW = panelW - 40.0f;
    float titleBoxH = 70.0f;
    float titleBoxX = panelX + (panelW - titleBoxW) / 2.0f;
    float titleBoxY = panelY + panelH - titleBoxH - 20.0f;

    glColor3f(0.09f, 0.12f, 0.25f);
    glBegin(GL_QUADS);
    glVertex2f(titleBoxX,              titleBoxY);
    glVertex2f(titleBoxX + titleBoxW,  titleBoxY);
    glVertex2f(titleBoxX + titleBoxW,  titleBoxY + titleBoxH);
    glVertex2f(titleBoxX,              titleBoxY + titleBoxH);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(titleBoxX,              titleBoxY);
    glVertex2f(titleBoxX + titleBoxW,  titleBoxY);
    glVertex2f(titleBoxX + titleBoxW,  titleBoxY + titleBoxH);
    glVertex2f(titleBoxX,              titleBoxY + titleBoxH);
    glEnd();

    float centerX = WINDOW_WIDTH / 2.0f;
    float titleY  = titleBoxY + titleBoxH / 2.0f + 8.0f;

    // main title line
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "AVOID THE OBSTACLES",
                       centerX,
                       titleY);

    // subtitle line
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "3D RUNNER",
                       centerX,
                       titleY - 26.0f);

    // ----- buttons -----
    float btnW = 220.0f;
    float btnH = 50.0f;
    float startX = centerX - btnW / 2.0f;
    float startY = panelY + panelH * 0.40f;

    // shadow under buttons
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(startX + 3.0f,        startY - 3.0f);
    glVertex2f(startX + btnW + 3.0f, startY - 3.0f);
    glVertex2f(startX + btnW + 3.0f, startY + btnH - 3.0f);
    glVertex2f(startX + 3.0f,        startY + btnH - 3.0f);
    glEnd();

    // START button
    glColor3f(0.0f, 0.55f, 0.90f);
    glBegin(GL_QUADS);
    glVertex2f(startX,        startY);
    glVertex2f(startX+btnW,   startY);
    glVertex2f(startX+btnW,   startY+btnH);
    glVertex2f(startX,        startY+btnH);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "START",
                       centerX,
                       startY + 18.0f);

    // EXIT button (below)
    float exitY = startY - 80.0f;

    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(startX + 3.0f,        exitY - 3.0f);
    glVertex2f(startX + btnW + 3.0f, exitY - 3.0f);
    glVertex2f(startX + btnW + 3.0f, exitY + btnH - 3.0f);
    glVertex2f(startX + 3.0f,        exitY + btnH - 3.0f);
    glEnd();

    glColor3f(0.85f, 0.20f, 0.20f);
    glBegin(GL_QUADS);
    glVertex2f(startX,        exitY);
    glVertex2f(startX+btnW,   exitY);
    glVertex2f(startX+btnW,   exitY+btnH);
    glVertex2f(startX,        exitY+btnH);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "EXIT",
                       centerX,
                       exitY + 18.0f);

    glutSwapBuffers();
}

void drawCharacterSelect() {
    set2D();
    glClear(GL_COLOR_BUFFER_BIT);
    drawBackground2D();

    // ----- Title -----
    glColor3f(1.0f, 1.0f, 1.0f);
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "CHOOSE YOUR CHARACTER",
                       WINDOW_WIDTH / 2.0f,
                       WINDOW_HEIGHT - 80.0f);

    // ----- Card layout -----
    float cardW      = 190.0f;
    float cardH      = 190.0f;
    float cardSpace  = 30.0f;    // space between cards
    float totalRowW  = 3.0f * cardW + 2.0f * cardSpace;
    float rowStartX  = (WINDOW_WIDTH - totalRowW) / 2.0f;
    float cardY      = WINDOW_HEIGHT / 2.0f - cardH / 2.0f - 10.0f;
    float centerY    = cardY + cardH / 2.0f;

    float card1X = rowStartX;
    float card2X = rowStartX + cardW + cardSpace;
    float card3X = rowStartX + 2.0f * (cardW + cardSpace);

    float c1CenterX = card1X + cardW / 2.0f;
    float c2CenterX = card2X + cardW / 2.0f;
    float c3CenterX = card3X + cardW / 2.0f;

    // ----- Background panel behind all cards -----
    glColor3f(0.05f, 0.05f, 0.12f);
    glBegin(GL_QUADS);
    glVertex2f(rowStartX - 15.0f,          cardY - 20.0f);
    glVertex2f(rowStartX + totalRowW + 15.0f, cardY - 20.0f);
    glVertex2f(rowStartX + totalRowW + 15.0f, cardY + cardH + 25.0f);
    glVertex2f(rowStartX - 15.0f,          cardY + cardH + 25.0f);
    glEnd();

    glColor3f(0.7f, 0.7f, 0.8f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(rowStartX - 15.0f,          cardY - 20.0f);
    glVertex2f(rowStartX + totalRowW + 15.0f, cardY - 20.0f);
    glVertex2f(rowStartX + totalRowW + 15.0f, cardY + cardH + 25.0f);
    glVertex2f(rowStartX - 15.0f,          cardY + cardH + 25.0f);
    glEnd();

    // ---------- CAR CARD ----------
    glColor3f(0.07f, 0.09f, 0.22f);
    glBegin(GL_QUADS);
    glVertex2f(card1X,          cardY);
    glVertex2f(card1X + cardW,  cardY);
    glVertex2f(card1X + cardW,  cardY + cardH);
    glVertex2f(card1X,          cardY + cardH);
    glEnd();

    glColor3f(0.8f, 0.8f, 0.9f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(card1X,          cardY);
    glVertex2f(card1X + cardW,  cardY);
    glVertex2f(card1X + cardW,  cardY + cardH);
    glVertex2f(card1X,          cardY + cardH);
    glEnd();

    drawStringCentered(GLUT_BITMAP_HELVETICA_18, "CAR",
                       c1CenterX,
                       cardY + cardH + 20.0f);
    drawCarPreview2D(c1CenterX, centerY, 1.1f);

    // ---------- DINO CARD ----------
    glColor3f(0.05f, 0.20f, 0.09f);
    glBegin(GL_QUADS);
    glVertex2f(card2X,          cardY);
    glVertex2f(card2X + cardW,  cardY);
    glVertex2f(card2X + cardW,  cardY + cardH);
    glVertex2f(card2X,          cardY + cardH);
    glEnd();

    glColor3f(0.8f, 0.8f, 0.9f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(card2X,          cardY);
    glVertex2f(card2X + cardW,  cardY);
    glVertex2f(card2X + cardW,  cardY + cardH);
    glVertex2f(card2X,          cardY + cardH);
    glEnd();

    drawStringCentered(GLUT_BITMAP_HELVETICA_18, "DINOSAUR",
                       c2CenterX,
                       cardY + cardH + 20.0f);
    drawDinoPreview2D(c2CenterX, centerY, 1.1f);

    // ---------- CAT CARD ----------
    glColor3f(0.18f, 0.18f, 0.22f);
    glBegin(GL_QUADS);
    glVertex2f(card3X,          cardY);
    glVertex2f(card3X + cardW,  cardY);
    glVertex2f(card3X + cardW,  cardY + cardH);
    glVertex2f(card3X,          cardY + cardH);
    glEnd();

    glColor3f(0.8f, 0.8f, 0.9f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(card3X,          cardY);
    glVertex2f(card3X + cardW,  cardY);
    glVertex2f(card3X + cardW,  cardY + cardH);
    glVertex2f(card3X,          cardY + cardH);
    glEnd();

    drawStringCentered(GLUT_BITMAP_HELVETICA_18, "CAT",
                       c3CenterX,
                       cardY + cardH + 20.0f);
    drawCatPreview2D(c3CenterX, centerY, 1.1f);

    glutSwapBuffers();
}


void drawGameOver() {
    set2D();
    glClear(GL_COLOR_BUFFER_BIT);
    drawBackground2D();

    glColor3f(1.0f, 1.0f, 0.0f);
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       "GAME OVER!",
                       WINDOW_WIDTH / 2.0f,
                       WINDOW_HEIGHT / 2.0f + 80.0f);

    char buffer[64];
    sprintf(buffer, "Final Score: %d", score);
    drawStringCentered(GLUT_BITMAP_HELVETICA_18,
                       buffer,
                       WINDOW_WIDTH / 2.0f,
                       WINDOW_HEIGHT / 2.0f + 40.0f);

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

// ------------- DISPLAY -------------
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

    // STATE_PLAYING: 3D + HUD
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    set3D();

    // camera looking down the runway
    gluLookAt(0.0, 6.0, 18.0,
              0.0, 1.0, 0.0,
              0.0, 1.0, 0.0);

    drawRunway();
    drawObstacles3D();
    if (heartPickup.active)
{
    float x = (heartPickup.lane - (NUM_LANES - 1) / 2.0f) * LANE_SPACING;
    float z = heartPickup.z;

    drawShieldPickup3D(x, z);
}

    drawPlayer3D();
    drawHUD();

    glutSwapBuffers();
}

// ------------- UPDATE -------------
// ------------- UPDATE -------------
void update(int value) {
    (void)value;

    int currentTimeMs = glutGet(GLUT_ELAPSED_TIME);
    float dt = (currentTimeMs - lastTimeMs) / 1000.0f;
    if (dt < 0.0f) dt = 0.0f;
    lastTimeMs = currentTimeMs;

    if (gameState == STATE_PLAYING) {
        elapsedTime += dt;

        // ---------- HEART/SHIELD SPAWN (every 20..40 seconds) ----------
        if (!heartPickup.active && elapsedTime >= nextHeartSpawnTime) {
            heartPickup.active = true;
            heartPickup.lane   = rand() % NUM_LANES;
            heartPickup.z      = -80.0f;  // far ahead

            // schedule next spawn between 20 and 40 seconds from now
            float interval = 20.0f + (float)(rand() % 21); // 20..40
            nextHeartSpawnTime = elapsedTime + interval;
        }

        // ---------- POWERUP AVAILABILITY ----------
        if (!choosingPowerup && activePowerup == PWR_NONE &&
            elapsedTime >= nextPowerupTime) {
            choosingPowerup = true;
        }

        // ---------- POWERUP ACTIVE TIMER ----------
        if (activePowerup != PWR_NONE) {
            powerupTimer += dt;
            if (powerupTimer >= 5.0f) {
                activePowerup = PWR_NONE;
                powerupTimer  = 0.0f;
                nextPowerupTime = elapsedTime + 10.0f; // cooldown starts now
            }
        }

        // ---------- ANIMATIONS ----------
        animTime += dt;
        carBob   = 0.12f * sinf(animTime * 12.0f);
        legSwing = 40.0f  * sinf(animTime * 10.0f);

        // road treadmill offset
        float stripeSpacing = 6.0f;
        roadOffset += gameSpeed * dt;
        if (roadOffset > stripeSpacing)
            roadOffset = fmod(roadOffset, stripeSpacing);

        // ---------- SCORE (interval halves every 30s) ----------
        int level = (int)(elapsedTime / 30.0f);
        if (level < 0) level = 0;
        float scoreInterval = 1.0f;
        for (int i = 0; i < level; ++i)
            scoreInterval *= 0.5f;

        while (elapsedTime - lastScoreTime >= scoreInterval) {
            int add = (activePowerup == PWR_SCORE_X2) ? 2 : 1;
            score += add;
            lastScoreTime += scoreInterval;
        }

        // ---------- SPEED INCREASE EVERY 15s ----------
        while (elapsedTime - lastSpeedIncreaseTime >= 15.0f) {
            baseGameSpeed += 2.0f;
            lastSpeedIncreaseTime += 15.0f;
        }

        float speedMultiplier = (activePowerup == PWR_SLOW_HALF) ? 0.5f : 1.0f;
        gameSpeed = baseGameSpeed * speedMultiplier;

        // ---------- SPAWN RATE FASTER EVERY 30s ----------
        while (elapsedTime - lastSpawnIncreaseTime >= 30.0f) {
            spawnInterval /= 1.5f;
            if (spawnInterval < 0.1f) spawnInterval = 0.1f;
            lastSpawnIncreaseTime += 30.0f;
        }

        // ---------- MOVE OBSTACLES ----------
        for (int i = 0; i < MAX_OBS; ++i) {
            if (!obstacles[i].active) continue;
            obstacles[i].z += gameSpeed * dt;
            if (obstacles[i].z > 25.0f)
                obstacles[i].active = false;
        }

        // ---------- MOVE SHIELD PICKUP ----------
        if (heartPickup.active) {
            heartPickup.z += gameSpeed * dt;
            if (heartPickup.z > 25.0f) {
                heartPickup.active = false; // went past player
            }
        }

        // ---------- SPAWN NEW OBSTACLES ----------
        spawnTimer -= dt;
        if (spawnTimer <= 0.0f) {
            spawnObstacle();
            spawnTimer = spawnInterval;
        }

        // ---------- SHIELD PICKUP COLLISION ----------
        if (heartPickup.active && heartPickup.lane == playerLane) {
            float halfPlayer = playerSize * 0.5f;
            float pickupLen  = 1.5f;
            float halfPick   = pickupLen * 0.5f;

            float minZ = playerZ - halfPlayer - halfPick;
            float maxZ = playerZ + halfPlayer + halfPick;

            if (heartPickup.z >= minZ && heartPickup.z <= maxZ) {
                if (heartCount < MAX_HEARTS) {
                    heartCount++;
                }
                heartPickup.active = false;
            }
        }

        // ---------- OBSTACLE COLLISION (USE SHIELDS) ----------
        if (activePowerup != PWR_INVINCIBLE) {
            // small grace period so we don't lose multiple shields instantly
            bool recentlyHit = (elapsedTime - lastShieldHitTime < 0.4f);

            if (!recentlyHit) {
                for (int i = 0; i < MAX_OBS; ++i) {
                    if (!obstacles[i].active) continue;

                    if (checkCollision(obstacles[i])) {

                        if (heartCount > 0) {
                            // use one shield instead of dying
                            heartCount--;
                            lastShieldHitTime = elapsedTime;  // remember when we got hit
                            obstacles[i].active = false;
                            // optional: small effect here
                        } else {
                            // no shields -> game over
                            gameState = STATE_GAMEOVER;
                        }

                        break;  // stop checking more obstacles this frame
                    }
                }
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}


// ------------- INPUT -------------

void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;

    // ESC always quits
    if (key == 27) {
        exit(0);
    }

    // SPACE on game over -> back to character select
    if (gameState == STATE_GAMEOVER && key == ' ') {
        gameState = STATE_CHAR_SELECT;
        return;
    }

    // -------- POWERUP KEYBINDS (E, R, T) --------
    if (gameState == STATE_PLAYING &&
        choosingPowerup &&
        activePowerup == PWR_NONE)
    {
        PowerupType chosen = PWR_NONE;

        if (key == 'e' || key == 'E') {
            chosen = PWR_SCORE_X2;      // x2 score
        } else if (key == 'r' || key == 'R') {
            chosen = PWR_SLOW_HALF;     // half speed
        } else if (key == 't' || key == 'T') {
            chosen = PWR_INVINCIBLE;    // invincible
        }

        if (chosen != PWR_NONE) {
            activePowerup   = chosen;
            choosingPowerup = false;
            powerupTimer    = 0.0f;
            // cooldown is handled when it expires in update()
            return;
        }
    }
}


void specialKeyboard(int key, int x, int y) {
    (void)x; (void)y;
    if (gameState != STATE_PLAYING) return;

    if (key == GLUT_KEY_LEFT) {
        if (playerLane > 0) {
            playerLane--;
            recalcPlayerX();
        }
    } else if (key == GLUT_KEY_RIGHT) {
        if (playerLane < NUM_LANES - 1) {
            playerLane++;
            recalcPlayerX();
        }
    }
}

void mouse(int button, int state, int x, int y)
{
    // Convert from actual window pixels -> virtual 800x600 game space
    float gx = (float)x * (float)WINDOW_WIDTH  / (float)currentWindowWidth;
    float gy = (float)y * (float)WINDOW_HEIGHT / (float)currentWindowHeight;

    int mx = (int)gx;
    int my = (int)(WINDOW_HEIGHT - gy); // flip Y for game coords

    // ----------------- PLAYING STATE (lanes + powerups) -----------------
    if (gameState == STATE_PLAYING) {
        if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {

            // 1) powerup icons on HUD (bottom-right)
            if (choosingPowerup && activePowerup == PWR_NONE) {
                float iconW   = 70.0f;
                float iconH   = 35.0f;
                float spacing = 8.0f;
                float margin  = 10.0f;

                float baseX = WINDOW_WIDTH  - margin - iconW;
                float baseY = margin;

                float y1 = baseY;
                float y2 = baseY + iconH + spacing;
                float y3 = baseY + 2 * (iconH + spacing);

                if (mx >= baseX && mx <= baseX + iconW) {
                    PowerupType chosen = PWR_NONE;

                    if (my >= y1 && my <= y1 + iconH)
                        chosen = PWR_SCORE_X2;
                    else if (my >= y2 && my <= y2 + iconH)
                        chosen = PWR_SLOW_HALF;
                    else if (my >= y3 && my <= y3 + iconH)
                        chosen = PWR_INVINCIBLE;

                    if (chosen != PWR_NONE) {
                        activePowerup   = chosen;
                        choosingPowerup = false;
                        powerupTimer    = 0.0f;
                        return;   // do NOT start dragging if we clicked a powerup
                    }
                }
            }

            // 2) otherwise: start drag to move between lanes
            isDragging = true;
        }
        else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
            isDragging = false;
        }

        return; // only exit early for PLAYING state
    }

    // ----------------- OTHER STATES (menu, char select, game over) -----------------
    // For menus we only care about left button DOWN
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

    // ----- MAIN MENU -----
    if (gameState == STATE_MENU) {
        // must match drawMenu()
        const float panelH  = 260.0f;
        const float panelY  = WINDOW_HEIGHT / 2.0f - panelH / 2.5f;

        const float btnW    = 220.0f;
        const float btnH    = 50.0f;
        const float centerX = WINDOW_WIDTH / 2.0f;
        const float startX  = centerX - btnW / 2.0f;
        const float startY  = panelY + panelH * 0.40f;
        const float exitY   = startY - 80.0f;

        // START -> character select
        if (mx >= startX && mx <= startX + btnW &&
            my >= startY && my <= startY + btnH) {
            gameState = STATE_CHAR_SELECT;
            return;
        }

        // EXIT
        if (mx >= startX && mx <= startX + btnW &&
            my >= exitY && my <= exitY + btnH) {
            exit(0);
        }

        return;
    }

    // ----- CHARACTER SELECT -----
    else if (gameState == STATE_CHAR_SELECT) {
        // *** must exactly match your drawCharacterSelect() math ***

        float cardW      = 190.0f;
        float cardH      = 190.0f;
        float cardSpace  = 30.0f;
        float totalRowW  = 3.0f * cardW + 2.0f * cardSpace;
        float rowStartX  = (WINDOW_WIDTH - totalRowW) / 2.0f;
        float cardY      = WINDOW_HEIGHT / 2.0f - cardH / 2.0f - 10.0f;

        float card1X = rowStartX;
        float card2X = rowStartX + cardW + cardSpace;
        float card3X = rowStartX + 2.0f * (cardW + cardSpace);

        // CAR
        if (mx >= card1X && mx <= card1X + cardW &&
            my >= cardY  && my <= cardY  + cardH) {
            currentCharacter = CHAR_CAR;
            resetGame();
            gameState = STATE_PLAYING;
            return;
        }
        // DINO
        if (mx >= card2X && mx <= card2X + cardW &&
            my >= cardY  && my <= cardY  + cardH) {
            currentCharacter = CHAR_DINO;
            resetGame();
            gameState = STATE_PLAYING;
            return;
        }
        // CAT
        if (mx >= card3X && mx <= card3X + cardW &&
            my >= cardY  && my <= cardY  + cardH) {
            currentCharacter = CHAR_CAT;
            resetGame();
            gameState = STATE_PLAYING;
            return;
        }

        return;
    }

    // ----- GAME OVER POPUP -----
    else if (gameState == STATE_GAMEOVER) {
        float panelW = 400.0f;
        float panelH = 220.0f;
        float panelX = (WINDOW_WIDTH  - panelW) / 2.0f;
        float panelY = (WINDOW_HEIGHT - panelH) / 2.0f;

        float btnW = 120.0f;
        float btnH = 40.0f;

        float restartX = panelX + 40.0f;
        float restartY = panelY + 30.0f;

        float exitX    = panelX + panelW - btnW - 40.0f;
        float exitY    = panelY + 30.0f;

        // Restart
        if (mx >= restartX && mx <= restartX + btnW &&
            my >= restartY && my <= restartY + btnH) {
            resetGame();
            gameState = STATE_CHAR_SELECT;   // or STATE_PLAYING if you prefer
            return;
        }

        // Exit
        if (mx >= exitX && mx <= exitX + btnW &&
            my >= exitY && my <= exitY + btnH) {
            exit(0);
        }

        return;
    }
}


void mouseMotion(int x, int y) {
    (void)y;
    if (!isDragging || gameState != STATE_PLAYING) return;

    // scale real window pixels -> virtual 800 width
    float gx = (float)x * (float)WINDOW_WIDTH / (float)currentWindowWidth;

    // map mouse X across *virtual* window to lane index 0..NUM_LANES-1
    float fx = gx / (float)WINDOW_WIDTH;   // 0..1
    int lane = (int)(fx * NUM_LANES);

    if (lane < 0) lane = 0;
    if (lane >= NUM_LANES) lane = NUM_LANES - 1;

    playerLane = lane;
    recalcPlayerX();
}


// ------------- RESHAPE -------------
void reshape(int w, int h) {
    if (h == 0) h = 1; // avoid division by zero

    currentWindowWidth  = w;
    currentWindowHeight = h;

    glViewport(0, 0, w, h);

    // Keep using the same virtual 800x600 world
    // (so your HUD / buttons math using WINDOW_WIDTH/HEIGHT stays valid)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ------------- MAIN -------------
int main(int argc, char **argv) {
    srand((unsigned int)time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("Avoid the Obstacles - 3D Runner");

    PlaySound(TEXT("assets/soundtrack.wav"),
          NULL,
          SND_FILENAME | SND_ASYNC | SND_LOOP);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    resetGame();
    lastTimeMs = glutGet(GLUT_ELAPSED_TIME);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMotion);
    glutTimerFunc(16, update, 0);

    glutMainLoop();
    return 0;
}
