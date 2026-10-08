#ifdef STARFRONT_DISPLAY_TEST
#include <vitasdk.h>
#include <vitaGL.h>
#include <stdio.h>
#include <stdlib.h>
#include "port.h"

static GLuint test_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char message[512] = {0};
        glGetShaderInfoLog(shader, sizeof(message), NULL, message);
        fatal_error("Display test shader 0x%x: %s", type, message);
    }
    return shader;
}

typedef struct {float position[3], uv[2]; unsigned char color[4];} TestVertex;
static TestVertex vertices[] = {
    {{192,109,0},{0,0},{255,255,255,255}},
    {{768,109,0},{1,0},{255,255,255,255}},
    {{768,435,0},{1,1},{255,255,255,255}},
    {{192,435,0},{0,1},{255,255,255,255}}
};
static const GLushort indices[] = {0,1,2,0,2,3};
static const GLfloat matrix[] = {
    2.0f/SCREEN_W,0,0,0, 0,-2.0f/SCREEN_H,0,0,
    0,0,1,0, -1,1,0,1
};

static GLuint textured_program(void) {
    const char *vs =
        "attribute highp vec4 Vertex; attribute mediump vec2 TexCoord0; "
        "attribute lowp vec4 Color0; uniform highp mat4 WorldViewProjectionMatrix; "
        "varying mediump vec2 vCoord0; varying lowp vec4 vColor0; "
        "void main(){vCoord0=TexCoord0;vColor0=Color0;"
        "gl_Position=WorldViewProjectionMatrix*Vertex;}";
    const char *fs =
        "precision mediump float; uniform lowp sampler2D texture; "
        "varying mediump vec2 vCoord0; varying lowp vec4 vColor0; "
        "void main(){gl_FragColor=texture2D(texture,vCoord0)*vColor0;}";
    GLuint program = glCreateProgram();
    glAttachShader(program, test_shader(GL_VERTEX_SHADER, vs));
    glAttachShader(program, test_shader(GL_FRAGMENT_SHADER, fs));
    glLinkProgram(program);
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char message[512] = {0};
        glGetProgramInfoLog(program, sizeof(message), NULL, message);
        fatal_error("Display test program: %s", message);
    }
    return program;
}

static void textured_setup(GLuint program) {
    GLuint texture;
    static const unsigned char texels[] = {
        255,255,0,255, 0,255,255,255,
        255,0,255,255, 0,0,0,255
    };
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glUseProgram(program);
    GLint position = glGetAttribLocation(program, "Vertex");
    GLint uv = glGetAttribLocation(program, "TexCoord0");
    GLint color = glGetAttribLocation(program, "Color0");
    GLint transform = glGetUniformLocation(program, "WorldViewProjectionMatrix");
    GLint sampler = glGetUniformLocation(program, "texture");
    if (position < 0 || uv < 0 || color < 0 || transform == -1 || sampler == -1)
        fatal_error("Display test shader interface missing");
    glUniformMatrix4fv(transform, 1, GL_FALSE, matrix);
    glUniform1i(sampler, 0);
    glEnableVertexAttribArray(position);
    glEnableVertexAttribArray(uv);
    glEnableVertexAttribArray(color);
    glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, sizeof(TestVertex), vertices[0].position);
    glVertexAttribPointer(uv, 2, GL_FLOAT, GL_FALSE, sizeof(TestVertex), vertices[0].uv);
    glVertexAttribPointer(color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TestVertex), vertices[0].color);
    port_log("DISPLAY TEST GLSL: program=%u stride=%u attributes=%d/%d/%d matrix=0x%x sampler=0x%x setupError=0x%x", program, (unsigned)sizeof(TestVertex), position, uv, color, transform, sampler, glGetError());
    port_flush_emulator_log();
}

static GLuint color_program(void) {
    const char *vs =
        "attribute highp vec4 Vertex; attribute lowp vec4 Color0; "
        "uniform highp mat4 WorldViewProjectionMatrix; varying lowp vec4 vColor0; "
        "void main(){vColor0=Color0;gl_Position=WorldViewProjectionMatrix*Vertex;}";
    const char *fs =
        "precision mediump float; varying lowp vec4 vColor0; "
        "void main(){gl_FragColor=vColor0;}";
    GLuint program = glCreateProgram();
    glAttachShader(program, test_shader(GL_VERTEX_SHADER, vs));
    glAttachShader(program, test_shader(GL_FRAGMENT_SHADER, fs));
    glLinkProgram(program);
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) fatal_error("Display test color program link");
    return program;
}

static void color_setup(GLuint program) {
    glUseProgram(program);
    GLint position = glGetAttribLocation(program, "Vertex");
    GLint color = glGetAttribLocation(program, "Color0");
    GLint transform = glGetUniformLocation(program, "WorldViewProjectionMatrix");
    if (position < 0 || color < 0 || transform == -1)
        fatal_error("Display test color interface missing");
    glUniformMatrix4fv(transform, 1, GL_FALSE, matrix);
    glEnableVertexAttribArray(position);
    glEnableVertexAttribArray(color);
    glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, sizeof(TestVertex), vertices[0].position);
    glVertexAttribPointer(color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TestVertex), vertices[0].color);
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned c = 0; c < 3; ++c) vertices[i].color[c] = 0;
    port_log("DISPLAY TEST COLOR: program=%u black U8N colors error=0x%x", program, glGetError());
    port_flush_emulator_log();
}

/* This separate emulator diagnostic never loads or invokes the game. It
 * distinguishes display presentation from native resource/shader problems. */
int sf_display_test(void) {
    SceDisplayFrameBuf fb = {.size = sizeof(fb)};
    int r = sceDisplayGetFrameBuf(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME);
    if (r < 0 || !fb.base || fb.width != SCREEN_W || fb.height != SCREEN_H)
        fatal_error("Display diagnostic: framebuffer unavailable 0x%x", r);
    static const unsigned colors[] = {0xff00ffffU, 0xffffff00U, 0xffff00ffU};
    for (unsigned y = 0; y < fb.height; ++y)
        for (unsigned x = 0; x < fb.width; ++x)
            ((unsigned *)fb.base)[y * fb.pitch + x] = colors[x * 3 / fb.width];
    port_screen(0);
    port_log("DISPLAY TEST CPU: yellow/cyan/magenta thirds, base=%p pitch=%u", fb.base, fb.pitch);
    port_flush_emulator_log();
    for (unsigned i = 0; i < 120; ++i) {
        sceDisplaySetFrameBuf(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME);
        sceDisplayWaitVblankStart();
    }
    r = vglInitExtended(0, SCREEN_W, SCREEN_H, 64*1024*1024, SCE_GXM_MULTISAMPLE_NONE);
    if (r) fatal_error("Display diagnostic: resolution fallback");
    port_log("DISPLAY TEST GPU: red/green/blue/black quadrants via vitaGL clear");
    port_flush_emulator_log();
    GLuint program = 0, black = 0;
    unsigned previous_stage = ~0U;
    for (unsigned frame = 0; ; ++frame) {
        unsigned stage = frame / 900;
        if (stage > 7) stage = 7;
        if (stage != previous_stage) {
            previous_stage = stage;
            port_log("DISPLAY TEST STAGE %u at frame %u", stage, frame);
            if (stage == 1) {
                int compiler = shark_init(NULL);
                port_log("DISPLAY TEST explicit shark_init result=0x%x (VGL may already initialize it)", compiler);
                if (compiler < 0) fatal_error("Display test compiler init: 0x%x", compiler);
            } else if (stage == 2) {
                program = textured_program();
                port_log("DISPLAY TEST linked texture GLSL, not bound/setup/drawn");
            } else if (stage == 3) {
                textured_setup(program);
            } else if (stage == 6) {
                black = color_program();
                color_setup(black);
            }
            port_flush_emulator_log();
        }
        if (stage == 4 || stage == 5 || stage == 7) glUseProgram(0);
        glEnable(GL_SCISSOR_TEST);
        for (unsigned q = 0; q < 4; ++q) {
            glScissor((q & 1) * (SCREEN_W / 2), (q >> 1) * (SCREEN_H / 2), SCREEN_W / 2, SCREEN_H / 2);
            glClearColor(q == 0, q == 1, q == 2, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        if (stage >= 5) {
            glUseProgram(stage >= 6 ? black : program);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
        }
        vglSwapBuffers(GL_FALSE);
        if (frame % 900 == 0) {
            port_log("DISPLAY TEST stage=%u frame=%u GL error=0x%x", stage, frame, glGetError());
            port_flush_emulator_log();
        }
    }
}
#endif
