#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/osmesa.h>
#include <GL/glu.h>

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>
#include <psp2shell.h>

#define printf psp2shell_print

#define WIDTH 960
#define HEIGHT 544

static vita2d_texture *vtex = NULL;
static OSMesaContext ctx = NULL;

static void Sphere(float radius, int slices, int stacks) {
    GLUquadric *q = gluNewQuadric();
    gluQuadricNormals(q, GLU_SMOOTH);
    gluSphere(q, radius, slices, stacks);
    gluDeleteQuadric(q);
}


static void Cone(float base, float height, int slices, int stacks) {
    GLUquadric *q = gluNewQuadric();
    gluQuadricDrawStyle(q, GLU_FILL);
    gluQuadricNormals(q, GLU_SMOOTH);
    gluCylinder(q, base, 0.0, height, slices, stacks);
    gluDeleteQuadric(q);
}


static void Torus(float innerRadius, float outerRadius, int sides, int rings) {
    /* from GLUT... */
    int i, j;
    GLfloat theta, phi, theta1;
    GLfloat cosTheta, sinTheta;
    GLfloat cosTheta1, sinTheta1;
    const GLfloat ringDelta = 2.0 * M_PI / rings;
    const GLfloat sideDelta = 2.0 * M_PI / sides;

    theta = 0.0;
    cosTheta = 1.0;
    sinTheta = 0.0;
    for (i = rings - 1; i >= 0; i--) {
        theta1 = theta + ringDelta;
        cosTheta1 = cos(theta1);
        sinTheta1 = sin(theta1);
        glBegin(GL_QUAD_STRIP);
        phi = 0.0;
        for (j = sides; j >= 0; j--) {
            GLfloat cosPhi, sinPhi, dist;

            phi += sideDelta;
            cosPhi = cos(phi);
            sinPhi = sin(phi);
            dist = outerRadius + innerRadius * cosPhi;

            glNormal3f(cosTheta1 * cosPhi, -sinTheta1 * cosPhi, sinPhi);
            glVertex3f(cosTheta1 * dist, -sinTheta1 * dist, innerRadius * sinPhi);
            glNormal3f(cosTheta * cosPhi, -sinTheta * cosPhi, sinPhi);
            glVertex3f(cosTheta * dist, -sinTheta * dist, innerRadius * sinPhi);
        }
        glEnd();
        theta = theta1;
        cosTheta = cosTheta1;
        sinTheta = sinTheta1;
    }
}


static void Cube(float size) {
    size = 0.5 * size;

    glBegin(GL_QUADS);
    /* +X face */
    glNormal3f(1, 0, 0);
    glVertex3f(size, -size, size);
    glVertex3f(size, -size, -size);
    glVertex3f(size, size, -size);
    glVertex3f(size, size, size);

    /* -X face */
    glNormal3f(-1, 0, 0);
    glVertex3f(-size, size, size);
    glVertex3f(-size, size, -size);
    glVertex3f(-size, -size, -size);
    glVertex3f(-size, -size, size);

    /* +Y face */
    glNormal3f(0, 1, 0);
    glVertex3f(-size, size, size);
    glVertex3f(size, size, size);
    glVertex3f(size, size, -size);
    glVertex3f(-size, size, -size);

    /* -Y face */
    glNormal3f(0, -1, 0);
    glVertex3f(-size, -size, -size);
    glVertex3f(size, -size, -size);
    glVertex3f(size, -size, size);
    glVertex3f(-size, -size, size);

    /* +Z face */
    glNormal3f(0, 0, 1);
    glVertex3f(-size, -size, size);
    glVertex3f(size, -size, size);
    glVertex3f(size, size, size);
    glVertex3f(-size, size, size);

    /* -Z face */
    glNormal3f(0, 0, -1);
    glVertex3f(-size, size, -size);
    glVertex3f(size, size, -size);
    glVertex3f(size, -size, -size);
    glVertex3f(-size, -size, -size);

    glEnd();
}


/**
 * Draw red/green gradient across bottom of image.
 * Read pixels to check deltas.
 */
static void render_gradient(void) {
    GLfloat row[WIDTH][4];

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBegin(GL_POLYGON);
    glColor3f(1, 0, 0);
    glVertex2f(-1, -1.0);
    glVertex2f(-1, -0.9);
    glColor3f(0, 1, 0);
    glVertex2f(1, -0.9);
    glVertex2f(1, -1.0);
    glEnd();
    glFinish();

    glReadPixels(0, 0, WIDTH, 1, GL_RGBA, GL_FLOAT, row);
}


static void render_image(void) {
    static const GLfloat light_ambient[4] = {0.0, 0.0, 0.0, 1.0};
    static const GLfloat light_diffuse[4] = {1.0, 1.0, 1.0, 1.0};
    static const GLfloat light_specular[4] = {1.0, 1.0, 1.0, 1.0};
    static const GLfloat light_position[4] = {1.0, 1.0, 1.0, 0.0};
    static const GLfloat red_mat[4] = {1.0, 0.2, 0.2, 1.0};
    static const GLfloat green_mat[4] = {0.2, 1.0, 0.2, 1.0};
    static const GLfloat blue_mat[4] = {0.2, 0.2, 1.0, 1.0};
#if 0
    static const GLfloat yellow_mat[4]  = { 0.8, 0.8, 0.0, 1.0 };
#endif
    static const GLfloat purple_mat[4] = {0.8, 0.4, 0.8, 0.6};

    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHT0);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -1.0, 1.0, 2.0, 50.0);
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0, 0.5, -7);

    glClearColor(0.3, 0.3, 0.7, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();
    glRotatef(20.0, 1.0, 0.0, 0.0);

    /* ground */
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_POLYGON);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0);
    glVertex3f(-5, -1, -5);
    glTexCoord2f(1, 0);
    glVertex3f(5, -1, -5);
    glTexCoord2f(1, 1);
    glVertex3f(5, -1, 5);
    glTexCoord2f(0, 1);
    glVertex3f(-5, -1, 5);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_LIGHTING);

    glPushMatrix();
    glTranslatef(-1.5, 0.5, 0.0);
    glRotatef(90.0, 1.0, 0.0, 0.0);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, red_mat);
    Torus(0.275, 0.85, 20, 20);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-1.5, -0.5, 0.0);
    glRotatef(270.0, 1.0, 0.0, 0.0);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, green_mat);
    Cone(1.0, 2.0, 16, 1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.95, 0.0, -0.8);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, blue_mat);
    glLineWidth(2.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    Sphere(1.2, 20, 20);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-0.25, 0.0, 2.5);
    glRotatef(40, 0, 1, 0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, purple_mat);
    Cube(1.0);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glPopMatrix();

    glDisable(GL_LIGHTING);

    glPopMatrix();

    glDisable(GL_DEPTH_TEST);
}


static void init_context(void) {
    const GLint texWidth = 64, texHeight = 64;
    GLubyte *texImage;
    int i, j;

    /* checker image */
    texImage = malloc(texWidth * texHeight * 4);
    for (i = 0; i < texHeight; i++) {
        for (j = 0; j < texWidth; j++) {
            int k = (i * texWidth + j) * 4;
            if ((i % 5) == 0 || (j % 5) == 0) {
                texImage[k + 0] = 200;
                texImage[k + 1] = 200;
                texImage[k + 2] = 200;
                texImage[k + 3] = 255;
            } else {
                if ((i % 5) == 1 || (j % 5) == 1) {
                    texImage[k + 0] = 50;
                    texImage[k + 1] = 50;
                    texImage[k + 2] = 50;
                    texImage[k + 3] = 255;
                } else {
                    texImage[k + 0] = 100;
                    texImage[k + 1] = 100;
                    texImage[k + 2] = 100;
                    texImage[k + 3] = 255;
                }
            }
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, texImage);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    free(texImage);
}

static GLboolean render_scene() {

    init_context();
    render_image();
    render_gradient();

    return 1;
}

static int gl_init(int w, int h) {

    const GLint z = 16, stencil = 0, accum = 0;
    GLint cBits;

    ctx = OSMesaCreateContextExt(OSMESA_RGBA, z, stencil, accum, NULL);
    if (!ctx) {
        printf("OSMesaCreateContextExt() failed!\n");
        return 0;
    }

    vita2d_init();
    vtex = vita2d_create_empty_texture_format(w, h, SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8);
    void *buffer = vita2d_texture_get_datap(vtex);

    /* Bind the buffer to the context and make it current */
    if (!OSMesaMakeCurrent(ctx, buffer, GL_UNSIGNED_BYTE, w, h)) {
        printf("OSMesaMakeCurrent (8 bits/channel) failed!\n");
        vita2d_free_texture(vtex);
        OSMesaDestroyContext(ctx);
        return 0;
    }

    /* sanity checks */
    glGetIntegerv(GL_RED_BITS, &cBits);
    if (cBits != 8) {
        fprintf(stderr, "Unable to create 8-bit/channel renderbuffer.\n");
        fprintf(stderr, "May need to recompile Mesa with CHAN_BITS=16 or 32.\n");
        return 0;
    }
    glGetIntegerv(GL_GREEN_BITS, &cBits);
    assert(cBits == 8);
    glGetIntegerv(GL_BLUE_BITS, &cBits);
    assert(cBits == 8);
    glGetIntegerv(GL_ALPHA_BITS, &cBits);
    assert(cBits == 8);

    OSMesaColorClamp(GL_TRUE);

    return 1;
}

static void gl_swap() {

    /* Make sure buffered commands are finished! */
    glFinish();

    vita2d_start_drawing();
    vita2d_clear_screen();
    vita2d_draw_texture_scale_rotate(vtex, WIDTH / 2, HEIGHT / 2, -1, 1, 180 * 0.0174532925f);
    vita2d_end_drawing();
    vita2d_wait_rendering_done();
    vita2d_swap_buffers();
}

static int gl_exit() {

    vita2d_wait_rendering_done();
    vita2d_fini();
    if (vtex) {
        vita2d_free_texture(vtex);
    }
    OSMesaDestroyContext(ctx);

    return 1;
}

int main(int argc, char *argv[]) {

    psp2shell_init(3333, 5);
    printf("Hello, (GL)world!\n");

    gl_init(WIDTH, HEIGHT);
    render_scene();
    gl_swap();

    sceKernelDelayThread(100 * 1000000);

    gl_exit();

    psp2shell_exit();
    sceKernelExitProcess(0);
    return 0;
}
