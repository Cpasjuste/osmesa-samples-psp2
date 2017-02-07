#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <GL/osmesa.h>

#include <psp2/power.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>
#include <psp2shell.h>

#define printf psp2shell_print

#define WIDTH 960
#define HEIGHT 544

static vita2d_texture *vtex = NULL;
static OSMesaContext ctx = NULL;

static void gl_swap();

struct timeval start;
#define GLUT_ELAPSED_TIME 0

int glutGet(GLenum state) {

    if (state == GLUT_ELAPSED_TIME) {
        unsigned int ticks;
        struct timeval now;
        gettimeofday(&now, NULL);

        double start_m = (start.tv_sec) * 1000 + (start.tv_usec) / 1000;
        double now_m = (now.tv_sec) * 1000 + (now.tv_usec) / 1000;

        ticks = (unsigned int) (now_m - start_m);
        return ticks;
    }
    return 0;
}

#ifndef M_PI
#define M_PI 3.14159265
#endif

static GLint T0 = 0;
static GLint Frames = 0;
static GLint autoexit = 0;
static GLfloat viewDist = 60.0;

/**

  Draw a gear wheel.  You'll probably want to call this function when
  building a display list since we do a lot of trig here.

  Input:  inner_radius - radius of hole at center
          outer_radius - radius at center of teeth
          width - width of gear
          teeth - number of teeth
          tooth_depth - depth of tooth

 **/

static void
gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
     GLint teeth, GLfloat tooth_depth) {
    GLint i;
    GLfloat r0, r1, r2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0;
    r2 = outer_radius + tooth_depth / 2.0;

    da = 2.0 * M_PI / teeth / 4.0;

    glShadeModel(GL_FLAT);

    glNormal3f(0.0, 0.0, 1.0);

    /* draw front face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;
        glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
        if (i < teeth) {
            glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
            glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
        }
    }
    glEnd();

    /* draw front sides of teeth */
    glBegin(GL_QUADS);
    da = 2.0 * M_PI / teeth / 4.0;
    for (i = 0; i < teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
    }
    glEnd();

    glNormal3f(0.0, 0.0, -1.0);

    /* draw back face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;
        glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
        if (i < teeth) {
            glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
            glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
        }
    }
    glEnd();

    /* draw back sides of teeth */
    glBegin(GL_QUADS);
    da = 2.0 * M_PI / teeth / 4.0;
    for (i = 0; i < teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -width * 0.5);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
    }
    glEnd();

    /* draw outward faces of teeth */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
        u = r2 * cos(angle + da) - r1 * cos(angle);
        v = r2 * sin(angle + da) - r1 * sin(angle);
        len = sqrt(u * u + v * v);
        u /= len;
        v /= len;
        glNormal3f(v, -u, 0.0);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
        glNormal3f(cos(angle), sin(angle), 0.0);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
        glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -width * 0.5);
        u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
        v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
        glNormal3f(v, -u, 0.0);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
        glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
        glNormal3f(cos(angle), sin(angle), 0.0);
    }

    glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
    glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);

    glEnd();

    glShadeModel(GL_SMOOTH);

    /* draw inside radius cylinder */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.0 * M_PI / teeth;
        glNormal3f(-cos(angle), -sin(angle), 0.0);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
        glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
    }
    glEnd();

}

static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;

static void
cleanup(void) {
    glDeleteLists(gear1, 1);
    glDeleteLists(gear2, 1);
    glDeleteLists(gear3, 1);
}

static void
draw(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();

    glTranslatef(0.0, 0.0, -viewDist);

    glRotatef(view_rotx, 1.0, 0.0, 0.0);
    glRotatef(view_roty, 0.0, 1.0, 0.0);
    glRotatef(view_rotz, 0.0, 0.0, 1.0);

    glPushMatrix();
    glTranslatef(-3.0, -2.0, 0.0);
    glRotatef(angle, 0.0, 0.0, 1.0);
    glCallList(gear1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(3.1, -2.0, 0.0);
    glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
    glCallList(gear2);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-3.1, 4.2, 0.0);
    glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
    glCallList(gear3);
    glPopMatrix();

    glPopMatrix();

    gl_swap();

    Frames++;

    {
        GLint t = glutGet(GLUT_ELAPSED_TIME);
        if (t - T0 >= 5000) {
            GLfloat seconds = (t - T0) / 1000.0;
            GLfloat fps = Frames / seconds;
            printf("%d frames in %6.3f seconds = %6.3f FPS\n", Frames, seconds, fps);
            fflush(stdout);
            T0 = t;
            Frames = 0;
            if ((t >= 999.0 * autoexit) && (autoexit)) {
                cleanup();
                exit(0);
            }
        }
    }
}


static void
idle(void) {
    static double t0 = -1.;
    double dt, t = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
    if (t0 < 0.0)
        t0 = t;
    dt = t - t0;
    t0 = t;

    angle += 70.0 * dt;  /* 70 degrees per second */
    angle = fmod(angle, 360.0); /* prevents eventual overflow */
}

/* new window size or exposure */
static void
reshape(int width, int height) {
    GLfloat h = (GLfloat) height / (GLfloat) width;

    glViewport(0, 0, (GLint) width, (GLint) height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -h, h, 5.0, 200.0);
    glMatrixMode(GL_MODELVIEW);
}

static void
init() {
    static GLfloat pos[4] = {5.0, 5.0, 10.0, 0.0};
    static GLfloat red[4] = {0.8, 0.1, 0.0, 1.0};
    static GLfloat green[4] = {0.0, 0.8, 0.2, 1.0};
    static GLfloat blue[4] = {0.2, 0.2, 1.0, 1.0};
    //GLint i;

    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);

    /* make the gears */
    gear1 = glGenLists(1);
    glNewList(gear1, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
    gear(1.0, 4.0, 1.0, 20, 0.7);
    glEndList();

    gear2 = glGenLists(1);
    glNewList(gear2, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
    gear(0.5, 2.0, 2.0, 10, 0.7);
    glEndList();

    gear3 = glGenLists(1);
    glNewList(gear3, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
    gear(1.3, 2.0, 0.5, 10, 0.7);
    glEndList();

    glEnable(GL_NORMALIZE);

    printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
    printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
    printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
    printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
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

    // wee need max performances here
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    gl_init(WIDTH, HEIGHT);

    init();
    reshape(WIDTH, HEIGHT);

    gettimeofday(&start, NULL);

    while (1) {
        draw();
        idle();
    }
    cleanup();

    sceKernelDelayThread(100 * 1000000);

    gl_exit();

    psp2shell_exit();
    sceKernelExitProcess(0);
    return 0;
}
