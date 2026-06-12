// ============================================================
//  Interactive 3D Solar System Simulation
//  University Computer Graphics Project
//  OpenGL + GLUT (C++)
// ============================================================
//  Author   : [Your Name]
//  Course   : Computer Graphics
//  Version  : 1.0
// ============================================================
//
//  KEYBOARD CONTROLS:
//  W / S          → Zoom In / Zoom Out
//  A / D          → Move Camera Left / Right
//  Arrow Up/Down  → Rotate Camera Up / Down (Pitch)
//  Arrow L/R      → Rotate Camera Left / Right (Yaw)
//  L              → Toggle Lights ON / OFF
//  P              → Pause / Resume Animation
//  + / -          → Speed Up / Slow Down Orbit
//  R              → Reset Camera to Default
//  ESC            → Exit
// ============================================================

#ifdef _WIN32
  #include <windows.h>
#endif

#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>

// ─────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────
#define PI 3.14159265358979323846
#define NUM_STARS 1500
#define DEG2RAD(d) ((d) * PI / 180.0)

// ─────────────────────────────────────────────
//  Planet data structure
// ─────────────────────────────────────────────
struct Planet {
    const char* name;
    float  orbitRadius;   // distance from Sun (scene units)
    float  size;          // sphere radius
    float  orbitSpeed;    // degrees per idle tick
    float  spinSpeed;     // self-rotation speed
    float  orbitAngle;    // current orbit angle (degrees)
    float  spinAngle;     // current self-rotation angle
    float  r, g, b;       // base colour (used when textures disabled)
    float  tilt;          // axial tilt in degrees
};

// ─────────────────────────────────────────────
//  Global state
// ─────────────────────────────────────────────

// --- Animation ---
bool  g_paused       = false;
float g_speedFactor  = 1.0f;   // multiplier for all orbital speeds

// --- Camera ---
float g_camDist      = 55.0f;  // zoom distance
float g_camX         = 0.0f;   // lateral offset
float g_camPitch     = 20.0f;  // vertical rotation (degrees)
float g_camYaw       = 0.0f;   // horizontal rotation (degrees)

// --- Lighting ---
bool  g_lightsOn     = true;

// --- Stars ---
float g_starX[NUM_STARS];
float g_starY[NUM_STARS];
float g_starZ[NUM_STARS];
float g_starBright[NUM_STARS];

// --- Angles ---
float g_moonAngle    = 0.0f;
float g_moonTilt     = 5.0f;
float g_satRingRot   = 0.0f;
float g_sunPulse     = 0.0f;   // for subtle sun glow animation

// ─────────────────────────────────────────────
//  Planet definitions (Mercury … Neptune)
// ─────────────────────────────────────────────
Planet g_planets[8] = {
    // name        orbitR  size  orbitSpd  spinSpd  orbitAng spinAng  R     G     B    tilt
    // orbitSpeed: degrees per idle tick (greatly reduced for smooth, realistic motion)
    // spinSpeed : self-rotation degrees per tick (slow, comfortable visual speed)
    { "Mercury",    8.0f, 0.35f,  0.240f,  0.0150f,    0.0f,  0.0f, 0.75f,0.70f,0.65f,  0.0f },
    { "Venus",     12.0f, 0.87f,  0.094f, -0.0035f,   45.0f,  0.0f, 0.90f,0.75f,0.40f,177.4f },
    { "Earth",     17.0f, 0.92f,  0.058f,  0.0400f,   90.0f,  0.0f, 0.25f,0.50f,0.90f, 23.4f },
    { "Mars",      23.0f, 0.50f,  0.031f,  0.0390f,  135.0f,  0.0f, 0.80f,0.35f,0.20f, 25.2f },
    { "Jupiter",   33.0f, 2.20f,  0.005f,  0.0960f,  180.0f,  0.0f, 0.90f,0.78f,0.60f,  3.1f },
    { "Saturn",    43.0f, 1.80f,  0.002f,  0.0880f,  220.0f,  0.0f, 0.95f,0.90f,0.70f, 26.7f },
    { "Uranus",    53.0f, 1.20f,  0.0007f,-0.0560f,  270.0f,  0.0f, 0.55f,0.85f,0.95f, 97.8f },
    { "Neptune",   62.0f, 1.10f,  0.0004f, 0.0600f,  315.0f,  0.0f, 0.25f,0.40f,0.90f, 28.3f },
};

// ─────────────────────────────────────────────
//  Procedural texture generation
//  (since we cannot load image files portably,
//   we generate all textures procedurally)
// ─────────────────────────────────────────────
GLuint g_texSun;
GLuint g_texPlanet[8];
GLuint g_texMoon;
GLuint g_texSatRing;

// Helper: create a 1D gradient colour between two RGB triples
static void lerpColor(float t,
                      float r0,float g0,float b0,
                      float r1,float g1,float b1,
                      unsigned char* out)
{
    out[0] = (unsigned char)(255*(r0*(1-t)+r1*t));
    out[1] = (unsigned char)(255*(g0*(1-t)+g1*t));
    out[2] = (unsigned char)(255*(b0*(1-t)+b1*t));
}

// Simple noise helper (pseudo-random based on grid)
static float noise2(int x, int y){
    int n = x + y*57;
    n = (n<<13)^n;
    return (1.0f - ((n*(n*n*15731+789221)+1376312589)&0x7fffffff)/1073741824.0f);
}

// Generate Sun texture – radial gradient yellow/orange + sunspot noise
static void makeSunTexture()
{
    const int S = 256;
    unsigned char* img = new unsigned char[S*S*3];
    float cx = S/2.0f, cy = S/2.0f;
    for(int y=0;y<S;y++){
        for(int x=0;x<S;x++){
            float dx=(x-cx)/cx, dy=(y-cy)/cy;
            float r=sqrtf(dx*dx+dy*dy);
            float t=r>1.0f?1.0f:r;
            // base gradient: yellow core → orange rim
            float R=1.0f*(1-t)+1.0f*t;
            float G=0.95f*(1-t)+0.45f*t;
            float B=0.10f*(1-t)+0.00f*t;
            // add turbulence
            float n= (noise2(x/4,y/4)+noise2(x/8,y/8))*0.05f;
            R=fmaxf(0,fminf(1,R+n));
            G=fmaxf(0,fminf(1,G+n*0.5f));
            int idx=(y*S+x)*3;
            img[idx+0]=(unsigned char)(255*R);
            img[idx+1]=(unsigned char)(255*G);
            img[idx+2]=(unsigned char)(255*B);
        }
    }
    glGenTextures(1,&g_texSun);
    glBindTexture(GL_TEXTURE_2D,g_texSun);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,S,S,0,GL_RGB,GL_UNSIGNED_BYTE,img);
    delete[] img;
}

// Generate planet textures (each unique)
static void makePlanetTexture(GLuint& tex,
                              float r0,float g0,float b0,
                              float r1,float g1,float b1,
                              int   stripeFreq,
                              bool  swirl)
{
    const int S = 128;
    unsigned char* img = new unsigned char[S*S*3];
    for(int y=0;y<S;y++){
        for(int x=0;x<S;x++){
            float u=(float)x/S, v=(float)y/S;
            float stripe = sinf(v*PI*stripeFreq + noise2(x/6,y/6)*1.5f)*0.5f+0.5f;
            if(swirl) stripe = sinf((v+u*0.3f)*PI*stripeFreq + noise2(x/8,y/8))*0.5f+0.5f;
            float n = noise2(x/3,y/3)*0.12f;
            float t = fmaxf(0,fminf(1, stripe + n));
            unsigned char c[3];
            lerpColor(t, r0,g0,b0, r1,g1,b1, c);
            int idx=(y*S+x)*3;
            img[idx+0]=c[0]; img[idx+1]=c[1]; img[idx+2]=c[2];
        }
    }
    glGenTextures(1,&tex);
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,S,S,0,GL_RGB,GL_UNSIGNED_BYTE,img);
    delete[] img;
}

// Generate Moon texture – grey craters
static void makeMoonTexture()
{
    const int S=64;
    unsigned char* img=new unsigned char[S*S*3];
    for(int y=0;y<S;y++){
        for(int x=0;x<S;x++){
            float n=(noise2(x/2,y/2)+noise2(x,y)*0.5f);
            unsigned char v=(unsigned char)(190+40*n);
            int idx=(y*S+x)*3;
            img[idx+0]=v; img[idx+1]=v; img[idx+2]=v;
        }
    }
    glGenTextures(1,&g_texMoon);
    glBindTexture(GL_TEXTURE_2D,g_texMoon);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,S,S,0,GL_RGB,GL_UNSIGNED_BYTE,img);
    delete[] img;
}

// Generate Saturn ring texture (radial gradient, semi-transparent)
static void makeSatRingTexture()
{
    const int S=256;
    unsigned char* img=new unsigned char[S*4]; // 1D strip RGBA
    for(int x=0;x<S;x++){
        float t=(float)x/S;
        float alpha = (t<0.1f||t>0.9f)?0.0f:
                      (t<0.3f?((t-0.1f)/0.2f):
                       t<0.7f?1.0f:
                       (1-(t-0.7f)/0.2f));
        alpha*=0.75f;
        float ring_r=0.95f,ring_g=0.88f,ring_b=0.70f;
        img[x*4+0]=(unsigned char)(255*ring_r);
        img[x*4+1]=(unsigned char)(255*ring_g);
        img[x*4+2]=(unsigned char)(255*ring_b);
        img[x*4+3]=(unsigned char)(255*alpha);
    }
    glGenTextures(1,&g_texSatRing);
    glBindTexture(GL_TEXTURE_1D,g_texSatRing);
    glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage1D(GL_TEXTURE_1D,0,GL_RGBA,S,0,GL_RGBA,GL_UNSIGNED_BYTE,img);
    delete[] img;
}

// ─────────────────────────────────────────────
//  initTextures – creates all procedural textures
// ─────────────────────────────────────────────
void initTextures()
{
    makeSunTexture();

    // Mercury – grey/brown rocky
    makePlanetTexture(g_texPlanet[0], 0.60f,0.55f,0.50f, 0.85f,0.80f,0.75f, 5, false);
    // Venus – yellow/orange cloudy swirl
    makePlanetTexture(g_texPlanet[1], 0.90f,0.75f,0.35f, 0.98f,0.90f,0.60f, 8, true);
    // Earth – blue/green
    makePlanetTexture(g_texPlanet[2], 0.10f,0.35f,0.80f, 0.15f,0.65f,0.25f, 6, true);
    // Mars – red/brown
    makePlanetTexture(g_texPlanet[3], 0.75f,0.25f,0.10f, 0.90f,0.55f,0.30f, 4, false);
    // Jupiter – orange/cream bands
    makePlanetTexture(g_texPlanet[4], 0.95f,0.85f,0.65f, 0.80f,0.50f,0.30f,14, false);
    // Saturn – pale gold bands
    makePlanetTexture(g_texPlanet[5], 0.98f,0.92f,0.70f, 0.85f,0.78f,0.50f,10, false);
    // Uranus – cyan uniform
    makePlanetTexture(g_texPlanet[6], 0.45f,0.82f,0.92f, 0.60f,0.90f,0.98f, 3, true);
    // Neptune – deep blue
    makePlanetTexture(g_texPlanet[7], 0.10f,0.20f,0.75f, 0.20f,0.40f,0.95f, 5, true);

    makeMoonTexture();
    makeSatRingTexture();
}

// ─────────────────────────────────────────────
//  initStars – scatter random stars in a sphere
// ─────────────────────────────────────────────
void initStars()
{
    srand(42); // deterministic seed for reproducibility
    for(int i=0;i<NUM_STARS;i++){
        float theta = ((float)rand()/RAND_MAX)*2*PI;
        float phi   = ((float)rand()/RAND_MAX)*PI;
        float radius= 200.0f + ((float)rand()/RAND_MAX)*100.0f;
        g_starX[i] = radius*sinf(phi)*cosf(theta);
        g_starY[i] = radius*cosf(phi);
        g_starZ[i] = radius*sinf(phi)*sinf(theta);
        g_starBright[i] = 0.4f + 0.6f*((float)rand()/RAND_MAX);
    }
}

// ─────────────────────────────────────────────
//  setupLighting
// ─────────────────────────────────────────────
void setupLighting()
{
    // Ambient scene light (very dim – deep space)
    GLfloat ambientScene[] = {0.04f,0.04f,0.06f,1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientScene);

    // LIGHT 0 – Sun: white-yellow point light at origin
    GLfloat sunPos[]      = {0.0f,0.0f,0.0f,1.0f};
    GLfloat sunDiffuse[]  = {1.0f,0.95f,0.75f,1.0f};
    GLfloat sunSpecular[] = {1.0f,1.0f,0.85f,1.0f};
    GLfloat sunAmbient[]  = {0.15f,0.10f,0.05f,1.0f};
    glLightfv(GL_LIGHT0,GL_POSITION, sunPos);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,  sunDiffuse);
    glLightfv(GL_LIGHT0,GL_SPECULAR, sunSpecular);
    glLightfv(GL_LIGHT0,GL_AMBIENT,  sunAmbient);
    glLightf (GL_LIGHT0,GL_CONSTANT_ATTENUATION,  0.3f);
    glLightf (GL_LIGHT0,GL_LINEAR_ATTENUATION,    0.002f);
    glLightf (GL_LIGHT0,GL_QUADRATIC_ATTENUATION, 0.0001f);
    glEnable (GL_LIGHT0);

    // LIGHT 1 – Subtle blue fill from behind camera
    GLfloat fillPos[]      = {0.0f, 50.0f, 100.0f, 0.0f}; // directional
    GLfloat fillDiffuse[]  = {0.05f,0.05f, 0.15f, 1.0f};
    GLfloat fillAmbient[]  = {0.00f,0.00f, 0.05f, 1.0f};
    GLfloat fillSpecular[] = {0.00f,0.00f, 0.00f, 1.0f};
    glLightfv(GL_LIGHT1,GL_POSITION, fillPos);
    glLightfv(GL_LIGHT1,GL_DIFFUSE,  fillDiffuse);
    glLightfv(GL_LIGHT1,GL_AMBIENT,  fillAmbient);
    glLightfv(GL_LIGHT1,GL_SPECULAR, fillSpecular);
    glEnable (GL_LIGHT1);

    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT,GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
}

// ─────────────────────────────────────────────
//  drawStars
// ─────────────────────────────────────────────
void drawStars()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glPointSize(1.5f);
    glBegin(GL_POINTS);
    for(int i=0;i<NUM_STARS;i++){
        float b=g_starBright[i];
        glColor3f(b,b,fminf(1.0f,b*1.05f)); // slight blue tint
        glVertex3f(g_starX[i],g_starY[i],g_starZ[i]);
    }
    glEnd();
    glPointSize(1.0f);
    if(g_lightsOn) glEnable(GL_LIGHTING);
}

// ─────────────────────────────────────────────
//  drawOrbitRing – thin dashed circle on XZ plane
// ─────────────────────────────────────────────
void drawOrbitRing(float radius)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.4f,0.5f,0.7f,0.3f);
    glLineStipple(1,0xAAAA);
    glEnable(GL_LINE_STIPPLE);
    glBegin(GL_LINE_LOOP);
    for(int i=0;i<180;i++){
        float a=DEG2RAD(i*2.0f);
        glVertex3f(radius*cosf(a), 0.0f, radius*sinf(a));
    }
    glEnd();
    glDisable(GL_LINE_STIPPLE);
    if(g_lightsOn) glEnable(GL_LIGHTING);
}

// ─────────────────────────────────────────────
//  drawSun – emissive glowing sphere
// ─────────────────────────────────────────────
void drawSun()
{
    glPushMatrix();

    // Sun is fixed at the origin – no axial rotation applied.
    // Only the corona pulse animates (g_sunPulse), making the Sun
    // a stable, stationary light source at the scene centre.

    // Sun is self-luminous – disable lighting so it's always bright
    glDisable(GL_LIGHTING);

    // Bind sun texture
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_texSun);

    // Glow colour modulation
    float pulse = 0.97f + 0.03f*sinf(g_sunPulse);
    glColor3f(1.0f*pulse, 0.85f*pulse, 0.3f*pulse);

    GLUquadric* q = gluNewQuadric();
    gluQuadricTexture(q, GL_TRUE);
    gluQuadricNormals(q, GLU_SMOOTH);
    gluSphere(q,3.5f,48,48);
    gluDeleteQuadric(q);

    glDisable(GL_TEXTURE_2D);

    // Corona glow rings (additive, semi-transparent)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    for(int k=1;k<=4;k++){
        float alpha = 0.10f/(k*k);
        float r = 3.5f + k*0.45f;
        glColor4f(1.0f,0.7f,0.2f,alpha);
        GLUquadric* qg=gluNewQuadric();
        gluDisk(qg,r-0.05f,r,64,1);
        gluDeleteQuadric(qg);
    }
    glDisable(GL_BLEND);

    if(g_lightsOn) glEnable(GL_LIGHTING);
    glPopMatrix();
}

// ─────────────────────────────────────────────
//  drawSaturnRings – flat textured disk
// ─────────────────────────────────────────────
void drawSaturnRings()
{
    glPushMatrix();

    // Ring is on the equatorial plane → rotate by Saturn's tilt
    glRotatef(g_planets[5].tilt, 0,0,1);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    float innerR = g_planets[5].size * 1.25f;
    float outerR = g_planets[5].size * 2.60f;
    int   segs   = 128;

    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=segs;i++){
        float angle = DEG2RAD(i * 360.0f / segs);
        float ca=cosf(angle), sa=sinf(angle);

        // Use ring colour with alpha (gradient)
        float t_inner = 0.15f;
        float t_outer = 0.85f;
        // inner vertex
        float ai = t_inner;
        float ao = t_outer;
        glColor4f(0.95f,0.90f,0.70f, 0.6f*(0.2f+0.8f*sinf(PI*ai)));
        glVertex3f(innerR*ca, 0, innerR*sa);
        glColor4f(0.95f,0.88f,0.65f, 0.6f*(0.2f+0.8f*sinf(PI*ao)));
        glVertex3f(outerR*ca, 0, outerR*sa);
    }
    glEnd();

    // Ring dividers (Cassini gap approximation)
    float gapR = (innerR+outerR)*0.54f;
    glColor4f(0.0f,0.0f,0.0f,0.35f);
    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=segs;i++){
        float angle=DEG2RAD(i*360.0f/segs);
        float ca=cosf(angle), sa=sinf(angle);
        glVertex3f((gapR-0.1f)*ca,0,(gapR-0.1f)*sa);
        glVertex3f((gapR+0.1f)*ca,0,(gapR+0.1f)*sa);
    }
    glEnd();

    glDisable(GL_BLEND);
    if(g_lightsOn) glEnable(GL_LIGHTING);
    glPopMatrix();
}

// ─────────────────────────────────────────────
//  drawMoon – orbiting Earth's moon
// ─────────────────────────────────────────────
void drawMoon()
{
    glPushMatrix();

    // Orbit around Earth (already inside Earth's matrix)
    glRotatef(g_moonTilt,  1,0,0);              // orbital inclination
    glRotatef(g_moonAngle, 0,1,0);              // current position
    glTranslatef(1.8f,     0,0);               // distance from Earth

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_texMoon);
    glColor3f(0.9f,0.9f,0.9f);

    GLUquadric* q=gluNewQuadric();
    gluQuadricTexture(q,GL_TRUE);
    gluQuadricNormals(q,GLU_SMOOTH);
    gluSphere(q,0.25f,24,24);
    gluDeleteQuadric(q);
    glDisable(GL_TEXTURE_2D);

    glPopMatrix();
}

// ─────────────────────────────────────────────
//  setPlanetMaterial – realistic shading params
// ─────────────────────────────────────────────
void setPlanetMaterial(int idx)
{
    GLfloat spec[]  = {0.3f,0.3f,0.3f,1.0f};
    GLfloat shine[] = {30.0f};
    glMaterialfv(GL_FRONT,GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT,GL_SHININESS,shine);
}

// ─────────────────────────────────────────────
//  drawPlanet – draws one planet with all features
// ─────────────────────────────────────────────
void drawPlanet(int idx)
{
    Planet& p = g_planets[idx];

    glPushMatrix();

    // ── 1. Revolution: move to orbit position ──
    glRotatef(p.orbitAngle, 0,1,0);           // orbit in XZ plane
    glTranslatef(p.orbitRadius, 0, 0);        // distance from Sun

    // ── 2. Axial tilt ──
    glRotatef(p.tilt, 0,0,1);

    // ── 3. Self-rotation ──
    glRotatef(p.spinAngle, 0,1,0);

    // ── 4. Scale (already encoded in size) ──
    // (gluSphere radius = p.size, so scaling done implicitly)

    // ── 5. Material & texture ──
    setPlanetMaterial(idx);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_texPlanet[idx]);
    glColor3f(p.r, p.g, p.b);

    GLUquadric* q=gluNewQuadric();
    gluQuadricTexture(q,GL_TRUE);
    gluQuadricNormals(q,GLU_SMOOTH);
    gluSphere(q, p.size, 36, 36);
    gluDeleteQuadric(q);
    glDisable(GL_TEXTURE_2D);

    // ── Extra features per planet ──
    if(idx == 2) { // Earth → draw Moon
        drawMoon();
    }
    if(idx == 5) { // Saturn → draw rings
        drawSaturnRings();
    }

    // Atmosphere glow for gas giants
    if(idx==4||idx==5||idx==6||idx==7){
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_LIGHTING);
        float ar=p.r*0.8f, ag=p.g*0.8f, ab=p.b*0.8f;
        glColor4f(ar,ag,ab,0.08f);
        GLUquadric* qa=gluNewQuadric();
        gluSphere(qa,p.size*1.07f,32,32);
        gluDeleteQuadric(qa);
        glDisable(GL_BLEND);
        if(g_lightsOn) glEnable(GL_LIGHTING);
    }

    glPopMatrix();
}

// ─────────────────────────────────────────────
//  drawAsteroidBelt – simple particle band
//  between Mars and Jupiter
// ─────────────────────────────────────────────
void drawAsteroidBelt()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glPointSize(1.2f);
    glBegin(GL_POINTS);
    srand(1234);
    for(int i=0;i<800;i++){
        float angle  = DEG2RAD(i * (360.0f/800.0f)*1.3f);
        float radius = 27.5f + 2.5f*((float)rand()/RAND_MAX-0.5f);
        float height =  0.3f*((float)rand()/RAND_MAX-0.5f);
        float bright = 0.35f+0.25f*((float)rand()/RAND_MAX);
        glColor3f(bright*0.9f, bright*0.85f, bright*0.75f);
        glVertex3f(radius*cosf(angle), height, radius*sinf(angle));
    }
    glEnd();
    glPointSize(1.0f);
    if(g_lightsOn) glEnable(GL_LIGHTING);
}

// ─────────────────────────────────────────────
//  drawNebula – faint translucent background cloud
// ─────────────────────────────────────────────
void drawNebula()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // Draw layered translucent quads far in background
    float colours[3][4] = {
        {0.4f,0.1f,0.6f,0.012f},
        {0.1f,0.3f,0.7f,0.010f},
        {0.6f,0.2f,0.1f,0.008f},
    };
    float dist = 150.0f;
    for(int c=0;c<3;c++){
        glColor4fv(colours[c]);
        glBegin(GL_QUADS);
          glVertex3f(-dist,-dist,-(dist+c*20));
          glVertex3f( dist,-dist,-(dist+c*20));
          glVertex3f( dist, dist,-(dist+c*20));
          glVertex3f(-dist, dist,-(dist+c*20));
        glEnd();
    }

    glDisable(GL_BLEND);
    if(g_lightsOn) glEnable(GL_LIGHTING);
}

// ─────────────────────────────────────────────
//  drawHUD – on-screen help text
// ─────────────────────────────────────────────
void drawHUD(int winW, int winH)
{
    // Switch to 2D orthographic projection
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Title
    glColor3f(1.0f,0.85f,0.3f);
    glRasterPos2i(10, winH-20);
    const char* title = "Interactive 3D Solar System  |  OpenGL + GLUT";
    for(const char* c=title;*c;c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,*c);

    // Controls
    const char* lines[] = {
        "W/S: Zoom   A/D: Pan   Arrows: Rotate",
        "L: Lights   P: Pause   +/-: Speed   R: Reset   ESC: Quit",
        NULL
    };
    glColor3f(0.7f,0.8f,1.0f);
    int ly = winH-42;
    for(int i=0;lines[i];i++,ly-=18){
        glRasterPos2i(10,ly);
        for(const char* c=lines[i];*c;c++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12,*c);
    }

    // Status line
    char status[128];
    sprintf(status,"Speed: %.1fx   Lights: %s   %s",
            g_speedFactor,
            g_lightsOn?"ON":"OFF",
            g_paused?"[PAUSED]":"");
    glColor3f(0.5f,1.0f,0.6f);
    glRasterPos2i(10,12);
    for(const char* c=status;*c;c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12,*c);

    // Planet labels (small dot indicators)
    // Restore
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    if(g_lightsOn) glEnable(GL_LIGHTING);
}

// ─────────────────────────────────────────────
//  setupCamera
// ─────────────────────────────────────────────
void setupCamera()
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Compute eye position from spherical coordinates
    float pitch = DEG2RAD(g_camPitch);
    float yaw   = DEG2RAD(g_camYaw);

    float eyeX = g_camX + g_camDist * cosf(pitch) * sinf(yaw);
    float eyeY =          g_camDist * sinf(pitch);
    float eyeZ =          g_camDist * cosf(pitch) * cosf(yaw);

    gluLookAt(eyeX, eyeY, eyeZ,   // eye
              g_camX, 0,  0,       // target (always look at Sun area)
              0,      1,  0);      // up
}

// ─────────────────────────────────────────────
//  display callback
// ─────────────────────────────────────────────
int g_winW=1200, g_winH=700;

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ── Camera (viewing transform) ──
    setupCamera();

    // ── Re-apply sun light position in view coords ──
    if(g_lightsOn){
        GLfloat sunPos[]={0,0,0,1};
        glLightfv(GL_LIGHT0,GL_POSITION,sunPos);
    }

    // ── Deep space background ──
    drawNebula();
    drawStars();

    // ── Orbit rings (cosmetic guides) ──
    glColor4f(1,1,1,1);
    for(int i=0;i<8;i++) drawOrbitRing(g_planets[i].orbitRadius);

    // ── Sun ──
    drawSun();

    // ── Asteroid belt ──
    drawAsteroidBelt();

    // ── Planets ──
    for(int i=0;i<8;i++) drawPlanet(i);

    // ── HUD overlay ──
    drawHUD(g_winW, g_winH);

    glutSwapBuffers();
}

// ─────────────────────────────────────────────
//  reshape callback
// ─────────────────────────────────────────────
void reshape(int w, int h)
{
    g_winW=w; g_winH=h;
    if(h==0) h=1;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(50.0, (double)w/h, 0.5, 600.0);
    glMatrixMode(GL_MODELVIEW);
}

// ─────────────────────────────────────────────
//  idle callback – animation engine
// ─────────────────────────────────────────────
void idle()
{
    if(g_paused) return;

    // Advance planet angles
    for(int i=0;i<8;i++){
        g_planets[i].orbitAngle += g_planets[i].orbitSpeed * g_speedFactor;
        g_planets[i].spinAngle  += g_planets[i].spinSpeed  * g_speedFactor * 2.0f;
        if(g_planets[i].orbitAngle >= 360.0f) g_planets[i].orbitAngle -= 360.0f;
        if(g_planets[i].spinAngle  >= 360.0f) g_planets[i].spinAngle  -= 360.0f;
    }

    // Moon (orbits Earth at a gentle pace)
    g_moonAngle  += 0.18f * g_speedFactor;
    if(g_moonAngle>=360.0f) g_moonAngle-=360.0f;

    // Sun pulse
    g_sunPulse += 0.02f;

    glutPostRedisplay();
}

// ─────────────────────────────────────────────
//  keyboard callback (ASCII keys)
// ─────────────────────────────────────────────
void keyboard(unsigned char key, int /*x*/, int /*y*/)
{
    switch(key){
        // Zoom
        case 'w': case 'W': g_camDist -= 2.0f; if(g_camDist<5)  g_camDist=5;   break;
        case 's': case 'S': g_camDist += 2.0f; if(g_camDist>200) g_camDist=200; break;

        // Pan
        case 'a': case 'A': g_camX -= 1.5f; break;
        case 'd': case 'D': g_camX += 1.5f; break;

        // Toggle lights
        case 'l': case 'L':
            g_lightsOn = !g_lightsOn;
            if(g_lightsOn){ glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_LIGHT1); }
            else           { glDisable(GL_LIGHTING); }
            break;

        // Pause
        case 'p': case 'P':
            g_paused = !g_paused;
            break;

        // Speed up
        case '+': case '=':
            g_speedFactor *= 1.25f;
            if(g_speedFactor>20.0f) g_speedFactor=20.0f;
            break;

        // Slow down
        case '-': case '_':
            g_speedFactor /= 1.25f;
            if(g_speedFactor<0.05f) g_speedFactor=0.05f;
            break;

        // Reset camera
        case 'r': case 'R':
            g_camDist=55.0f; g_camX=0; g_camPitch=20; g_camYaw=0;
            break;

        // Exit
        case 27: // ESC
            exit(0);
            break;
    }
    glutPostRedisplay();
}

// ─────────────────────────────────────────────
//  special key callback (arrow keys)
// ─────────────────────────────────────────────
void specialKey(int key, int /*x*/, int /*y*/)
{
    switch(key){
        case GLUT_KEY_UP:    g_camPitch += 3.0f; if(g_camPitch> 89) g_camPitch= 89; break;
        case GLUT_KEY_DOWN:  g_camPitch -= 3.0f; if(g_camPitch<-89) g_camPitch=-89; break;
        case GLUT_KEY_LEFT:  g_camYaw   -= 3.0f; break;
        case GLUT_KEY_RIGHT: g_camYaw   += 3.0f; break;
    }
    glutPostRedisplay();
}

// ─────────────────────────────────────────────
//  OpenGL initialisation
// ─────────────────────────────────────────────
void initGL()
{
    glClearColor(0.0f,0.0f,0.02f,1.0f); // near-black deep space

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glShadeModel(GL_SMOOTH);

    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT,            GL_NICEST);
    glHint(GL_POINT_SMOOTH_HINT,           GL_NICEST);

    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);

    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    setupLighting();
    initTextures();
    initStars();
}

// ─────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(g_winW, g_winH);
    glutInitWindowPosition(80,50);
    glutCreateWindow("Interactive 3D Solar System Simulation");

    initGL();

    glutDisplayFunc (display);
    glutReshapeFunc (reshape);
    glutIdleFunc    (idle);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc (specialKey);

    glutMainLoop();
    return 0;
}
// ============================================================
//  END OF FILE
// ============================================================
