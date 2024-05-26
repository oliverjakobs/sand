#include "nuklear_glfw_gl3.h"
#include "nuklear/nuklear_internal.h"

#define NK_SHADER_VERSION "#version 330 core\n"

static const char* vertex_shader =
NK_SHADER_VERSION
"layout (location = 0) in vec2 Position;\n"
"layout (location = 1) in vec2 TexCoord;\n"
"layout (location = 2) in vec4 Color;\n"
"out vec2 Frag_UV;\n"
"out vec4 Frag_Color;\n"
"uniform mat4 ProjMtx;\n"
"void main() {\n"
"   Frag_UV = TexCoord;\n"
"   Frag_Color = Color;\n"
"   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
"}\n";
static const char* fragment_shader =
NK_SHADER_VERSION
"precision mediump float;\n"
"uniform sampler2D Texture;\n"
"in vec2 Frag_UV;\n"
"in vec4 Frag_Color;\n"
"out vec4 Out_Color;\n"
"void main(){\n"
"   Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
"}\n";

NK_API int
nk_glfw3_init(struct nk_glfw* glfw, MinimalWindow* win)
{
    glfw->win = win;

    nk_init_default(&glfw->ctx, 0);
    nk_buffer_init_default(&glfw->cmds);

    glfw->prog = ignisCreateShaderSrcvf(vertex_shader, fragment_shader);

    glfw->uniform_tex = ignisGetUniformLocation(glfw->prog, "Texture");
    glfw->uniform_proj = ignisGetUniformLocation(glfw->prog, "ProjMtx");

    /* buffer setup */
    IgnisBufferElement layout[] = {
       { IGNIS_FLOAT, 2, GL_FALSE },
       { IGNIS_FLOAT, 2, GL_FALSE },
       { IGNIS_UINT8, 4, GL_TRUE }
    };

    ignisGenerateVertexArray(&glfw->vao, 2);
    ignisLoadArrayBuffer(&glfw->vao, 0, MAX_VERTEX_BUFFER, NULL, IGNIS_STREAM_DRAW);
    ignisSetVertexLayout(&glfw->vao, 0, layout, 3);
    ignisLoadElementBuffer(&glfw->vao, 1, NULL, MAX_ELEMENT_BUFFER, IGNIS_STREAM_DRAW);

    /* fill convert configuration */
    static const struct nk_draw_vertex_layout_element vertex_layout[] = {
        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT,    0},
        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT,    8},
        {NK_VERTEX_COLOR,    NK_FORMAT_R8G8B8A8, 16},
        {NK_VERTEX_LAYOUT_END}
    };

    glfw->config.vertex_layout = vertex_layout;
    glfw->config.vertex_size = 20;
    glfw->config.vertex_alignment = 4;
    glfw->config.tex_null.texture = nk_handle_id(IGNIS_DEFAULT_TEXTURE2D.name);
    glfw->config.tex_null.uv = nk_vec2(0.f, 0.f);
    glfw->config.circle_segment_count = 22;
    glfw->config.curve_segment_count = 22;
    glfw->config.arc_segment_count = 22;
    glfw->config.global_alpha = 1.0f;
    glfw->config.shape_AA = nk_true;
    glfw->config.line_AA = nk_true;

    return IGNIS_SUCCESS;
}

NK_API
void nk_glfw3_shutdown(struct nk_glfw* glfw)
{
    ignisFontAtlasClear(&glfw->atlas);

    ignisDeleteShader(glfw->prog);
    ignisDeleteVertexArray(&glfw->vao);
    nk_buffer_free(&glfw->cmds);

    nk_free(&glfw->ctx);
}

static struct nk_font user_font;

float font_text_width(nk_handle handle, float height, const char* text, int len)
{
    IgnisFont* font = handle.ptr;

    NK_ASSERT(font);
    NK_ASSERT(font->glyphs);
    if (!font || !text || !len)
        return 0;

    float scale = height / font->size;

    IgnisRune unicode;
    int glyph_len = nk_utf_decode(text, &unicode, (int)len);
    if (!glyph_len) return 0;

    float text_width = 0;
    int text_len = glyph_len;
    while (text_len <= len && glyph_len)
    {
        if (unicode == NK_UTF_INVALID) break;

        /* query currently drawn glyph information */
        const IgnisGlyph* g = ignisFontFindGlyph(font, unicode);
        text_width += g->xadvance * scale;

        /* offset next glyph */
        glyph_len = nk_utf_decode(text + text_len, &unicode, (int)len - text_len);
        text_len += glyph_len;
    }
    return text_width;
}

void font_query_glyph(nk_handle handle, float height, nk_font_glyph* glyph, nk_rune codepoint, nk_rune next)
{
    NK_ASSERT(glyph);
    NK_UNUSED(next);

    IgnisFont* font = handle.ptr;
    NK_ASSERT(font);
    NK_ASSERT(font->glyphs);
    if (!font || !glyph)
        return;

    float scale = height / font->size;
    const IgnisGlyph* g = ignisFontFindGlyph(font, codepoint);
    glyph->width = (g->x1 - g->x0) * scale;
    glyph->height = (g->y1 - g->y0) * scale;
    glyph->offset = nk_vec2(g->x0 * scale, g->y0 * scale);
    glyph->xadvance = (g->xadvance * scale);
    glyph->uv[0] = nk_vec2(g->u0, g->v0);
    glyph->uv[1] = nk_vec2(g->u1, g->v1);
}

void set_font(struct nk_context* ctx, IgnisFont* font)
{
    user_font.userdata.ptr = font;
    user_font.height = font->size;
    user_font.width = font_text_width;
    user_font.query = font_query_glyph;
    user_font.texture = nk_handle_id((int)font->texture->name);

    nk_style_set_font(ctx, &user_font);
}

NK_API void
nk_glfw3_load_font_atlas(struct nk_glfw* glfw)
{
    IgnisFontConfig fonts[3];
    loadDefaultFont(&fonts[0], 13.0f);
    //ignisFontAtlasLoadFromFile(&fonts[0], "res/fonts/ProggyClean.ttf", 13);
    ignisFontAtlasLoadFromFile(&fonts[1], "res/fonts/OpenSans.ttf", 13);
    ignisFontAtlasLoadFromFile(&fonts[2], "res/fonts/ProggyTiny.ttf", 14);

    ignisFontAtlasBake(&glfw->atlas, fonts, 3, IGNIS_FONT_FORMAT_RGBA32);

    ignisFontConfigClear(fonts, 3);

    set_font(&glfw->ctx, &glfw->atlas.fonts[0]);
}

NK_API void
nk_glfw3_new_frame(struct nk_glfw* glfw, float deltatime)
{
    glfw->ctx.delta_time_seconds = deltatime;

    struct nk_input* in = &glfw->ctx.input;

    nk_input_update_text_unicode(in, glfw->text, glfw->text_len);
    glfw->text_len = 0;

    struct nk_vec2 pos = { 0 };
    minimalCursorPos(&pos.x, &pos.y);

    nk_input_update_mouse(in, pos, glfw->scroll);
    glfw->scroll = nk_vec2(0, 0);
}

NK_API void
nk_glfw3_render(struct nk_glfw* glfw, const float* proj)
{
    /* convert from command queue into draw list and draw to screen */
    /* allocate vertex and element buffer */
    ignisBindVertexArray(&glfw->vao);

    /* load draw vertices & elements directly into vertex + element buffer */
    void* vertices = ignisMapBuffer(&glfw->vao.buffers[0], GL_WRITE_ONLY);
    void* elements = ignisMapBuffer(&glfw->vao.buffers[1], GL_WRITE_ONLY);
    {
        /* setup buffers to load vertices and elements */
        struct nk_buffer vbuf, ebuf;
        nk_buffer_init_fixed(&vbuf, vertices, (size_t)MAX_VERTEX_BUFFER);
        nk_buffer_init_fixed(&ebuf, elements, (size_t)MAX_ELEMENT_BUFFER);
        nk_convert(&glfw->ctx, &glfw->cmds, &vbuf, &ebuf, &glfw->config);
    }
    ignisUnmapBuffer(&glfw->vao.buffers[0]);
    ignisUnmapBuffer(&glfw->vao.buffers[1]);

    int width, height;
    minimalGetFramebufferSize(glfw->win, &width, &height);

    struct nk_vec2 fb_scale;
    minimalGetWindowContentScale(glfw->win, &fb_scale.x, &fb_scale.y);

    /* setup global state */
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    glViewport(0, 0, (GLsizei)width, (GLsizei)height);

    /* setup program */
    ignisUseShader(glfw->prog);
    ignisSetUniformil(glfw->prog, glfw->uniform_tex, 0);
    ignisSetUniformMat4l(glfw->prog, glfw->uniform_proj, 1, proj);


    /* iterate over and execute each draw command */
    nk_size offset = 0;
    const struct nk_draw_command* cmd;
    nk_draw_foreach(cmd, &glfw->ctx, &glfw->cmds)
    {
        if (!cmd->elem_count) continue;
        glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
        IgnisRect rect = {
            .x = cmd->clip_rect.x * fb_scale.x,
            .y = (height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)) * fb_scale.y,
            .w = cmd->clip_rect.w * fb_scale.x,
            .h = cmd->clip_rect.h * fb_scale.y
        };
        glScissor((GLint)rect.x, (GLint)rect.y, (GLint)rect.w, (GLint)rect.h);
        glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, (const void*)offset);
        offset += cmd->elem_count * sizeof(nk_draw_index);
    }
    nk_clear(&glfw->ctx);
    nk_buffer_clear(&glfw->cmds);

    /* default OpenGL state */
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
}