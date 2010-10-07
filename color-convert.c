#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <GL/glew.h>
#include <GL/glx.h>
#include <GL/glext.h>

#define WIDTH 1280
#define HEIGHT 720

#define FILENAME "sample.yuv420"
#define CANVAS_FILENAME "canvas.png"
#define FRAME_TIME_MS 1000 / 30

static uint8_t y_data[WIDTH*HEIGHT];
static uint8_t u_data[WIDTH * HEIGHT / 4];
static uint8_t v_data[WIDTH * HEIGHT / 4];
static GdkPixbuf *canvas_pixbuf;
static GRand *grand;
static float opacity = 0.5;

enum {
    TEX_Y,
    TEX_U,
    TEX_V,
    TEX_CANVAS,
    N_TEX
};

GLuint textures[N_TEX];

static const char *program=
  "uniform sampler2D Ytex;\n"
  "uniform sampler2D Utex,Vtex;\n"
  "uniform sampler2D canvas_tex;\n"
  "uniform float opacity;\n"
  "void main(void) {\n"
  "    float r,g,b,y,u,v;\n"
  "    vec4 txl,ux,vx;\n"
  "    vec2 nxy;\n"

  "    nxy = gl_TexCoord[0].xy;\n"
  "    y = texture2D(Ytex, nxy).r;\n"
  "    u = texture2D(Utex, nxy*0.5).r;\n"
  "    v = texture2D(Vtex, nxy*0.5).r;\n"

  "    y = 1.1643 * (y - 0.0625);\n"
  "    u = u - 0.5;\n"
  "    v = v - 0.5;\n"

  "    r = y + 1.5958*v;\n"
  "    g = y - 0.39173*u - 0.81290*v;\n"
  "    b = y + 2.017*u;\n"

  "    gl_FragColor = mix(vec4(r, g, b, 1.0), texture2D(canvas_tex, nxy), opacity);\n"
  "}\n";

static void
draw_scene(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBegin(GL_QUADS);
    {
        glTexCoord2i(0, 0);
        glVertex2i(0, 0);

        glTexCoord2i(1, 0);
        glVertex2i(WIDTH, 0);

        glTexCoord2i(1, 1);
        glVertex2i(WIDTH, HEIGHT);

        glTexCoord2i(0, 1);
        glVertex2i(0, HEIGHT);
    }
    glEnd();
}

#define MINIMUM_REGION_WIDTH 100
#define MINIMUM_REGION_HEIGHT 100

static void
upload_data()
{
    gboolean alpha;

    glActiveTexture(GL_TEXTURE0 + TEX_Y);
    glBindTexture(GL_TEXTURE_2D, textures[TEX_Y]);
    glTexSubImage2D(GL_TEXTURE_2D,
                    0, 0, 0,
                    WIDTH, HEIGHT,
                    GL_LUMINANCE,
                    GL_UNSIGNED_BYTE,
                    y_data);

    glActiveTexture(GL_TEXTURE0 + TEX_U);
    glBindTexture(GL_TEXTURE_2D, textures[TEX_U]);

    glTexSubImage2D(GL_TEXTURE_2D,
                    0, 0, 0,
                    WIDTH / 2, HEIGHT / 2,
                    GL_LUMINANCE,
                    GL_UNSIGNED_BYTE,
                    u_data);

    glActiveTexture(GL_TEXTURE0 + TEX_V);
    glBindTexture(GL_TEXTURE_2D, textures[TEX_V]);
    glTexSubImage2D(GL_TEXTURE_2D,
                    0, 0, 0,
                    WIDTH / 2, HEIGHT / 2,
                    GL_LUMINANCE,
                    GL_UNSIGNED_BYTE,
                    v_data);

    alpha = gdk_pixbuf_get_has_alpha(canvas_pixbuf);

    glActiveTexture(GL_TEXTURE0 + TEX_CANVAS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, textures[TEX_CANVAS]);

    {
        int canvas_width = gdk_pixbuf_get_width(canvas_pixbuf);
        int canvas_height = gdk_pixbuf_get_height(canvas_pixbuf);

        int x = g_rand_int_range(grand, 0, canvas_width - MINIMUM_REGION_WIDTH - 1);
        int y = g_rand_int_range(grand, 0, canvas_height - MINIMUM_REGION_HEIGHT - 1);

        int width = g_rand_int_range(grand, MINIMUM_REGION_WIDTH, canvas_width - x - 1);
        int height = g_rand_int_range(grand, MINIMUM_REGION_HEIGHT, canvas_height - y - 1);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, x);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, y);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, canvas_width);
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        x,
                        y,
                        width,
                        height,
                        alpha?GL_RGBA:GL_RGB,
                        GL_UNSIGNED_BYTE,
                        gdk_pixbuf_get_pixels(canvas_pixbuf));

        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
}

static void
load_data()
{
    FILE *file;
    size_t read_bytes;

    file = fopen(FILENAME, "rb");
    g_assert(file != NULL);

    read_bytes = fread(y_data, sizeof(y_data), 1, file);
    g_assert(read_bytes == 1);

    read_bytes = fread(u_data, sizeof(u_data), 1, file);
    g_assert(read_bytes == 1);

    read_bytes = fread(v_data, sizeof(v_data), 1, file);
    g_assert(read_bytes == 1);

    //g_assert(feof(file));

    fclose(file);
}

static void
setup_textures(GLuint program)
{
    GLint location;
    gboolean alpha;

    glGenTextures(N_TEX, textures);

    /* Select texture unit 0 as the active unit and bind the Y texture. */
    glActiveTexture(GL_TEXTURE0 + TEX_Y);
    location = glGetUniformLocation(program,"Ytex");
    glUniform1i(location, TEX_Y);  /* Bind Ytex to texture unit 0 */

    glBindTexture(GL_TEXTURE_2D, textures[TEX_Y]);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 WIDTH, HEIGHT,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 y_data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    /* Select texture unit 1 as the active unit and bind the U texture. */
    glActiveTexture(GL_TEXTURE0 + TEX_U);
    location = glGetUniformLocation(program, "Utex");
    glUniform1i(location, TEX_U);  /* Bind Utex to texture unit 1 */

    glBindTexture(GL_TEXTURE_2D, textures[TEX_U]);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 WIDTH / 2, HEIGHT / 2,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 u_data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    /* Select texture unit 2 as the active unit and bind the V texture. */
    glActiveTexture(GL_TEXTURE0 + TEX_V);
    location = glGetUniformLocation(program, "Vtex");
    glUniform1i(location, TEX_V);  /* Bind Vtext to texture unit 2 */

    glBindTexture(GL_TEXTURE_2D, textures[TEX_V]);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 WIDTH / 2, HEIGHT / 2,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 v_data);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

    /* Select texture unit 3 as the active unit and bind the canvas texture. */
    glActiveTexture(GL_TEXTURE0 + TEX_CANVAS);
    location = glGetUniformLocation(program, "canvas_tex");
    glUniform1i(location, TEX_CANVAS);  /* Bind Vtext to texture unit 2 */

    glBindTexture(GL_TEXTURE_2D, textures[TEX_CANVAS]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    alpha = gdk_pixbuf_get_has_alpha(canvas_pixbuf);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 alpha?GL_RGBA8:GL_RGB,
                 gdk_pixbuf_get_width(canvas_pixbuf), gdk_pixbuf_get_height(canvas_pixbuf),
                 0,
                 alpha?GL_RGBA:GL_RGB,
                 GL_UNSIGNED_BYTE, NULL);

}

static GLuint
compile_program(void)
{
    GLuint handle;
    GLuint shader;
    GLchar log[4096];
    GLsizei log_size;

    handle = glCreateProgram();
    shader = glCreateShader(GL_FRAGMENT_SHADER);

    /* Compile the shader. */
    glShaderSource(shader, 1, &program,NULL);
    glCompileShader(shader);

    glGetShaderInfoLog(shader,
                       sizeof(log),
                       &log_size,
                       log);
    g_assert(log_size >= 0);

    if (log_size > 0) {
        g_print("Shader compile log: %s\n", log);
    }

    /* Create a complete program object. */
    glAttachShader(handle, shader);
    glLinkProgram(handle);

    glGetProgramInfoLog(handle,
                        sizeof(log),
                        &log_size,
                        log);

    if (log_size > 0) {
        g_print("Program link log: %s\n", log);
    }


    return handle;
}

static void
set_fullscreen(Display *dpy,
               Window   window)
{
    static Atom wm_state = None;
    static Atom fullscreen = None;

    if (wm_state == None) {
        wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
        fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    }

    XChangeProperty(dpy, window,
                    wm_state,
                    XA_ATOM,
                    32,
                    PropModeReplace,
                    (unsigned char *)&fullscreen,
                    1);
}

#define OPACITY_INCREMENT 0.01

int
main (int argc, char **argv)
{
    Display *dpy;
    int screen;
    Window window;
    XWindowAttributes attr = {0, };
    int n_visual_infos;
    XVisualInfo visual_template;
    XVisualInfo *visual_infos;
    GLXContext ctx;
    GTimer *timer;
    int frame_count;
    gboolean mapped;
    const char *glx_extensions;
    GLuint handle;
    GLenum err;
    GTimer *frame_timer;
    double direction = OPACITY_INCREMENT;

    g_type_init();

    grand = g_rand_new();

    frame_timer = g_timer_new();
    g_timer_start(frame_timer);

    load_data();
    canvas_pixbuf = gdk_pixbuf_new_from_file(CANVAS_FILENAME, NULL);
    g_assert(canvas_pixbuf);

    dpy = XOpenDisplay(NULL);
    g_assert(dpy);

    screen = DefaultScreen(dpy);

    window = XCreateSimpleWindow(dpy,
                                 RootWindow(dpy, screen),
                                 0, 0,
                                 WIDTH, HEIGHT,
                                 0, 0,
                                 0xFFFFFFFF);

    set_fullscreen(dpy, window);

    XSelectInput(dpy, window, ExposureMask | StructureNotifyMask);

    XGetWindowAttributes(dpy, window, &attr);

    n_visual_infos = 0;
    visual_template.visualid = attr.visual->visualid;

    visual_infos = XGetVisualInfo(dpy,
                                  VisualIDMask,
                                  &visual_template,
                                  &n_visual_infos);
    g_assert(n_visual_infos == 1);

    glx_extensions = glXQueryExtensionsString(dpy, 0);
    g_assert(strstr(glx_extensions, "GLX_SGI_video_sync") != NULL);

    ctx = glXCreateContext(dpy, &visual_infos[0], NULL, TRUE);

    glXMakeCurrent(dpy, window, ctx);

    err = glewInit();
    g_assert(err == GLEW_OK);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, (float) WIDTH, (float) HEIGHT, 0.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDepthRange(0.0f, 1.0f);
    glViewport(0, 0, WIDTH, HEIGHT);
    glRasterPos2f(0.0f, 0.0f);

    glEnable(GL_TEXTURE_2D);

    handle = compile_program();
    glUseProgram(handle);
    setup_textures(handle);

    mapped = FALSE;
    XMapWindow(dpy, window);

    frame_count = 1;
    timer = g_timer_new();

    while (TRUE) {

        if (XPending(dpy)) {
            XEvent event;

            XNextEvent(dpy, &event);

            switch (event.type) {
                case Expose:
                    break;

                case MapNotify:
                    mapped = TRUE;
                    break;

                case UnmapNotify:
                    mapped = FALSE;
                    break;

                default:
                    break;
            }
        }

        if (mapped) {
            double elapsed;
            GLint location;

            opacity += direction;
            if (opacity >= 1.0) {
                direction = -OPACITY_INCREMENT;
            } else if (opacity <= 0.0) {
                direction = OPACITY_INCREMENT;
            }

            location = glGetUniformLocation(handle, "opacity");
            glUniform1f(location, opacity);

            upload_data();
            draw_scene();

            glXSwapBuffers(dpy, window);

            frame_count ++;

            elapsed = g_timer_elapsed(frame_timer, NULL) * 1000;
            if (elapsed < FRAME_TIME_MS) {
                g_usleep(1000 * (FRAME_TIME_MS - elapsed));
            }

            g_timer_start(frame_timer);
        }

        if (frame_count % 1000 == 0) {
            double fps;

            fps = 1000 / g_timer_elapsed(timer, NULL);
            g_timer_start(timer);
            g_print("Framerate: %gfps\n", fps);
        }
    }
}

