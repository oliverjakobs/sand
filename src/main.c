#include <ignis/ignis.h>

#include "math/math.h"
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

IgnisShader shader_model;
IgnisShader shader_skinned;

Model model = { 0 };
Animation* animations = NULL;
size_t animation_count = 0;
size_t animation_index = 0;

int paused = 0;

void loadGLTF(const char* dir, const char* filename)
{
    size_t size = 0;
    const char* path = ignisTextFormat("%s/%s", dir, filename);
    char* filedata = ignisReadFile(path, &size);

    if (!filedata) return;

    cgltf_options options = { 0 };
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse(&options, filedata, size, &data);
    if (result != cgltf_result_success)
    {
        IGNIS_ERROR("MODEL: [%s] Failed to load glTF data", path);
        free(filedata);
        return;
    }

    MINIMAL_INFO("    > Meshes count: %i", data->meshes_count);
    MINIMAL_INFO("    > Materials count: %i", data->materials_count);
    MINIMAL_INFO("    > Buffers count: %i", data->buffers_count);
    MINIMAL_INFO("    > Images count: %i", data->images_count);
    MINIMAL_INFO("    > Textures count: %i", data->textures_count);

    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success)
    {
        IGNIS_ERROR("MODEL: [%s] Failed to load mesh/material buffers", path);
        cgltf_free(data);
        free(filedata);
        return;
    }

    loadModelGLTF(&model, data, dir);
    animations = loadAnimationsGLTF(data, &animation_count);

    cgltf_free(data);
    free(filedata);
}

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

    ignisPrimitivesRendererInit();
    setViewport((float)w, (float)h);

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
    //loadModelGLTF(&model, &animation, "res/models/", "CesiumMilkTruck.gltf");
    loadGLTF("res/models/", "Fox.glb");

    for (int i = 0; i < model.mesh_count; i++) uploadMesh(&model.meshes[i]);

    return MINIMAL_OK;
}

void onDestroy()
{
    destroyModel(&model);
    for (size_t i = 0; i < animation_count; ++i)
        destroyAnimation(&animations[i]);

    ignisDeleteShader(shader_model);
    ignisDeleteShader(shader_skinned);

    nk_glfw3_shutdown(&glfw);

    ignisFontRendererDestroy();
    ignisPrimitivesRendererDestroy();

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

    if (minimalKeyDown(MINIMAL_KEY_LEFT))  camera_rotation -= camera_speed * framedata->deltatime;
    if (minimalKeyDown(MINIMAL_KEY_RIGHT)) camera_rotation += camera_speed * framedata->deltatime;
    if (minimalKeyDown(MINIMAL_KEY_DOWN))  camera_radius += camera_zoom * framedata->deltatime;
    if (minimalKeyDown(MINIMAL_KEY_UP))    camera_radius -= camera_zoom * framedata->deltatime;

    if (!paused)
    {
        tickAnimation(&animations[animation_index], framedata->deltatime);
    }

    //mat4 model = mat4_rotation((vec3) { 0.5f, 1.0f, 0.0f }, (float)glfwGetTime());
    //mat4 view = mat4_translation(vec3_negate(camera_pos));
    mat4 proj = mat4_perspective(degToRad(45.0f), (float)width / (float)height, 0.1f, 100.0f);

    vec3 eye = {
        sinf(camera_rotation) * camera_radius,
        0.0f,
        cosf(camera_rotation) * camera_radius,
    };
    vec3 look_at = { 0.0f, 0.0f, 0.0f };
    vec3 up = { 0.0f, 1.0f, 0.0f };
    mat4 view = mat4_look_at(eye, look_at, up);

    glPolygonMode(GL_FRONT_AND_BACK, poly_mode ? GL_LINE : GL_FILL);

    if (model.joint_count)
    {
        ignisSetUniformMat4(shader_skinned, "proj", 1, proj.v[0]);
        ignisSetUniformMat4(shader_skinned, "view", 1, view.v[0]);
        renderModelSkinned(&model, &animations[animation_index], shader_skinned);
    }
    else
    {
        ignisSetUniformMat4(shader_model, "proj", 1, proj.v[0]);
        ignisSetUniformMat4(shader_model, "view", 1, view.v[0]);
        renderModel(&model, &animations[animation_index], shader_model);
    }

    mat4 view_proj = mat4_multiply(proj, view);
    ignisPrimitivesRendererSetViewProjection(view_proj.v[0]);

    /*
    for (int i = 0; i < model.mesh_count; ++i)
    {
        Mesh* mesh = &model.meshes[i];
        ignisPrimitives3DRenderBox(&mesh->min.x, &mesh->max.x, IGNIS_WHITE);
    }
    */

    ignisPrimitivesRendererFlush();

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // render debug info

    nk_glfw3_new_frame(&glfw, framedata->deltatime);

    struct nk_context* ctx = &glfw.ctx;
    if (nk_begin(ctx, "Debug", nk_rect(50, 50, 230, 250),
        NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_SCROLL_AUTO_HIDE |
        NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE))
    {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "Fps: %d", framedata->fps);

        nk_layout_row_dynamic(ctx, 20, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "Current animation:  %d", animation_index);
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "Animation Duration: %4.2f", animations[animation_index].duration);
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "Animation Time:     %4.2f", animations[animation_index].time);
    }
    nk_end(ctx);

    nk_glfw3_render(&glfw);

    minimalSwapBuffers(window);
}

int main()
{
    minimalSetWindowHint(MINIMAL_HINT_CONTEXT_MAJOR_VERSION, 4);
    minimalSetWindowHint(MINIMAL_HINT_CONTEXT_MINOR_VERSION, 4);

    if (onLoad("IgnisApp", 100, 100, 1280, 720))
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