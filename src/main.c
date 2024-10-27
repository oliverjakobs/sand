#include <ignis/ignis.h>

#include "math/math.h"
#include "camera.h"
#include "model/model.h"

#include "nuklear_glfw_gl3.h"

#define MINIMAL_IMPLEMENTATION
#include "minimal.h"


static void ignisLogCallback(IgnisLogLevel level, const char *desc);
static void printVersionInfo();

static MinimalWindow* window;
struct nk_glfw glfw;

float width, height;
mat4 screen_projection;
int view_mode = 0;
int poly_mode = 0;

float camera_rotation = 0.0f;
float camera_radius = 16.0f;
float camera_speed = 1.0f;
float camera_zoom = 4.0f;

vec3 camera_pos = { 0 };
Camera camera = { 0 };

IgnisShader shader_model;
IgnisShader shader_skinned;

int paused = 0;

Model model = { 0 };
AnimationList animations = { 0 };
size_t animation_count = 0;
size_t animation_index = 0;

static void setViewport(float w, float h)
{
    width = w;
    height = h;
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    screen_projection = mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
}

uint8_t onLoad(const char* title, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    /* minimal initialization */
    if (!minimalPlatformInit())
    {
        MINIMAL_ERROR("[App] Failed to initialize Minimal");
        return MINIMAL_FAIL;
    }

    /* creating the window */
    window = minimalCreateWindow(title, x, y, w, h);
    if (!window)
    {
        MINIMAL_ERROR("[App] Failed to create Minimal window");
        return MINIMAL_FAIL;
    }

    minimalSwapInterval(0);

    /* ingis initialization */
    ignisSetLogCallback(ignisLogCallback);

    int debug = 0;
#ifdef _DEBUG
    debug = 1;
#endif

    if (!ignisInit(minimalGetGLProcAddress, debug))
    {
        MINIMAL_ERROR("[IGNIS] Failed to initialize Ignis");
        return MINIMAL_FAIL;
    }

    printVersionInfo();

    ignisEnableBlend(IGNIS_SRC_ALPHA, IGNIS_ONE_MINUS_SRC_ALPHA);
    ignisSetClearColor(IGNIS_DARK_GREY);

    glEnable(GL_DEPTH_TEST);

    ignisDebugRendererInit();
    setViewport((float)w, (float)h);

    cameraCreateOrtho(&camera, 0.0f, 0.0f, (float)width, (float)height);
    cameraSetCenterOrtho(&camera, (vec2) {0.0f, 0.0f});

    /* nuklear */
    nk_glfw3_init(&glfw, window);

    nk_glfw3_load_font_atlas(&glfw);

    /* shader */
    shader_model = ignisCreateShadervf("res/shaders/model.vert", "res/shaders/model.frag");
    shader_skinned = ignisCreateShadervf("res/shaders/skinned.vert", "res/shaders/model.frag");

    /* gltf model */
    //loadModelGLTF(&model, &animation, "res/models/", "Box.gltf");
    //loadModelGLTF(&model, &animation, "res/models/walking_robot", "scene.gltf");
    //loadModelGLTF(&model, &animation, "res/models/robot", "scene.gltf");
    //loadModelGLTF(&model, &animation, "res/models/", "RiggedSimple.gltf");
    //loadModelGLTF(&model, &animation, "res/models/", "RiggedFigure.gltf");
    //loadModelGLTF(&model, &animation, "res/models/", "BoxAnimated.gltf");
    loadGLTF("res/models/", "CesiumMilkTruck.gltf", &model, &animations);
    //loadGLTF("res/models/", "Fox.glb");

    uploadModel(&model);

    return MINIMAL_OK;
}

void onDestroy()
{
    destroyModel(&model);
    destroyAnimationList(&animations);

    ignisDeleteShader(shader_model);
    ignisDeleteShader(shader_skinned);

    nk_glfw3_shutdown(&glfw);

    ignisFontRendererDestroy();
    ignisDebugRendererDestroy();

    ignisDestroy();
}

int onEvent(void *context, const MinimalEvent *e)
{
    float w, h;
    if (minimalEventWindowSize(e, &w, &h))
    {
        setViewport(w, h);
        glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    }

    if (minimalEventIsType(e, MINIMAL_EVENT_CHAR))
    {
        if (glfw.text_len < NK_GLFW_TEXT_MAX)
            glfw.text[glfw.text_len++] = minimalEventChar(e);
    }

    float x = 0, y = 0;
    if (minimalEventMouseScrolled(e, &x, &y))
    {
        glfw.scroll.x += x;
        glfw.scroll.y += y;
    }

    /*
    // TODO: fix minimal not registering mouse release if moved out of window while pressed
    if (minimalEventMouseButtonReleased(e, MINIMAL_MOUSE_BUTTON_1, NULL, NULL))
        MINIMAL_INFO("Released");
    */

    switch (minimalEventKeyPressed(e))
    {
    case MINIMAL_KEY_ESCAPE:   minimalClose(window); break;
    //case MINIMAL_KEY_F6:       minimalToggleVsync(window); break;
    //case MINIMAL_KEY_F7:       minimalToggleDebug(window); break;
    case MINIMAL_KEY_F9:       view_mode = !view_mode; break;
    case MINIMAL_KEY_F10:      poly_mode = !poly_mode; break;
    case MINIMAL_KEY_SPACE:    paused = !paused; break;

    case MINIMAL_KEY_1: if (animation_count >= 0) animation_index = 0; break;
    case MINIMAL_KEY_2: if (animation_count >= 1) animation_index = 1; break;
    case MINIMAL_KEY_3: if (animation_count >= 2) animation_index = 2; break;
    case MINIMAL_KEY_4: if (animation_count >= 3) animation_index = 3; break;
    }

    return MINIMAL_OK;
}



void onTick(void* context, const MinimalFrameData* framedata)
{
    // clear screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /*
    */
    if (minimalKeyDown(MINIMAL_KEY_W)) camera_radius -= camera_zoom * framedata->deltatime;
    if (minimalKeyDown(MINIMAL_KEY_A)) camera_rotation -= camera_speed * framedata->deltatime;
    if (minimalKeyDown(MINIMAL_KEY_S)) camera_radius += camera_zoom * framedata->deltatime;
    if (minimalKeyDown(MINIMAL_KEY_D)) camera_rotation += camera_speed * framedata->deltatime;

    if (!paused)
    {
        tickAnimation(&animations.data[animation_index], framedata->deltatime);
    }

    mat4 proj = mat4_perspective(degToRad(45.0f), (float)width / (float)height, 0.1f, 100.0f);
    //mat4 proj = mat4_ortho(-6, 6, -4, 4, 0.1f, 100.0f);

    vec3 eye = {
        sinf(camera_rotation) * camera_radius,
        0.0f,
        cosf(camera_rotation) * camera_radius,
    };
    vec3 look_at = { 0.0f, 0.0f, 0.0f };
    vec3 up = { 0.0f, 1.0f, 0.0f };
    mat4 view = mat4_look_at(eye, look_at, up);

    //mat4 proj = camera.proj;
    //mat4 view = camera.view;


    // render grid
    mat4 view_proj = mat4_multiply(proj, view);
    ignisDebugRendererSetViewProjection(view_proj.v[0]);

    ignisRenderDebugGrid(10.0f, 10.0f, 1.0f);

    ignisDebugRendererFlush();


    glPolygonMode(GL_FRONT_AND_BACK, poly_mode ? GL_LINE : GL_FILL);

    if (model.joint_count)
    {
        ignisSetUniformMat4(shader_skinned, "proj", 1, proj.v[0]);
        ignisSetUniformMat4(shader_skinned, "view", 1, view.v[0]);
        renderModelSkinned(&model, &animations.data[animation_index], shader_skinned);
    }
    else
    {
        ignisSetUniformMat4(shader_model, "proj", 1, proj.v[0]);
        ignisSetUniformMat4(shader_model, "view", 1, view.v[0]);
        renderModel(&model, &animations.data[animation_index], shader_model);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // render debug info
    nk_glfw3_new_frame(&glfw, framedata->deltatime);

    struct nk_context* ctx = &glfw.ctx;
    if (nk_begin(ctx, "Debug", nk_rect(0, 0, 180, 110), 0))
    {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "Fps: %d", framedata->fps);

        nk_layout_row_dynamic(ctx, 20, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "Current animation:  %d", animation_index);
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "Animation Duration: %4.2f", animations.data[animation_index].duration);
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "Animation Time:     %4.2f", animations.data[animation_index].time);
    }
    nk_end(ctx);

    nk_glfw3_render(&glfw, screen_projection.v[0]);

    minimalSwapBuffers(window);
}

int main()
{
    minimalSetWindowHint(MINIMAL_HINT_CONTEXT_MAJOR_VERSION, 4);
    minimalSetWindowHint(MINIMAL_HINT_CONTEXT_MINOR_VERSION, 4);

    if (onLoad("IgnisApp", 100, 100, 1200, 800))
    {
        minimalSetCurrentContext(window);
        minimalSetEventHandler(NULL, (MinimalEventCB)onEvent);
        minimalRun(window, (MinimalTickCB)onTick, NULL);
    }

    onDestroy();

    return 0;
}

void ignisLogCallback(IgnisLogLevel level, const char *desc)
{
    switch (level)
    {
    case IGNIS_LOG_TRACE:    MINIMAL_TRACE("%s", desc); break;
    case IGNIS_LOC_INFO:     MINIMAL_INFO("%s", desc); break;
    case IGNIS_LOG_WARN:     MINIMAL_WARN("%s", desc); break;
    case IGNIS_LOG_ERROR:    MINIMAL_ERROR("%s", desc); break;
    case IGNIS_LOG_CRITICAL: MINIMAL_CRITICAL("%s", desc); break;
    }
}

void printVersionInfo()
{
    MINIMAL_INFO("[OpenGL]  Version:      %s", ignisGetGLVersion());
    MINIMAL_INFO("[OpenGL]  Vendor:       %s", ignisGetGLVendor());
    MINIMAL_INFO("[OpenGL]  Renderer:     %s", ignisGetGLRenderer());
    MINIMAL_INFO("[OpenGL]  GLSL Version: %s", ignisGetGLSLVersion());
    MINIMAL_INFO("[Ignis]   Version:      %s", ignisGetVersionString());
    MINIMAL_INFO("[Minimal] Version:      %s", minimalGetVersionString());
}