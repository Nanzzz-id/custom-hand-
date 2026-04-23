/**
 * HandPosMod v2 - Pakai GetPreloaderInput (cara resmi LeviLauncher)
 *
 * Meniru cara kerja libAxioClient.so:
 *  1. GlossInit() di .init_array
 *  2. GlossSymbol(libpreloader, "GetPreloaderInput") → dapat PLInput
 *  3. Register callback render (OpenGL) dan input (touch)
 *  4. Render UI panel slider X/Y/Z di dalam callback
 */

#include <jni.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <mutex>

#include "preloader_input.h"

#define TAG "HandPosMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ═══════════════════════════════════════
//  POSISI TANGAN
// ═══════════════════════════════════════
static float g_hand_x = 0.0f;
static float g_hand_y = 0.0f;
static float g_hand_z = 0.0f;
static std::mutex g_mtx;

// ═══════════════════════════════════════
//  UI STATE
// ═══════════════════════════════════════
static int   g_sw = 0, g_sh = 0;
static bool  g_visible   = true;
static bool  g_minimized = false;
static int   g_drag_axis = -1;
static int   g_drag_id   = -1;
static bool  g_gl_ready  = false;

// Panel posisi
static float g_px, g_py;
static const float PW = 230.0f;
static const float PH = 330.0f;

// ═══════════════════════════════════════
//  OPENGL
// ═══════════════════════════════════════
static GLuint g_prog_s = 0;  // solid
static GLuint g_prog_t = 0;  // text
static GLuint g_vao = 0, g_vbo = 0;
static GLuint g_font_tex = 0;

static const uint8_t FONT[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00},{0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00},{0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00},{0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00},{0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},{0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06},{0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00},{0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00},{0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00},{0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00},{0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00},{0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00},{0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00},{0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00},{0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},{0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00},{0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00},{0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00},{0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00},{0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00},{0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00},{0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00},{0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00},
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00},{0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00},{0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00},{0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},{0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00},{0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00},{0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},{0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00},
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},{0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00},
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00},
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00},{0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00},
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00},{0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00},
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00},{0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F},
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00},{0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E},{0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00},
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},{0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00},
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00},{0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00},
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F},{0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78},
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00},{0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00},
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00},{0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00},{0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00},
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00},{0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F},
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00},{0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00},
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},{0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00},
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
};

static const char* VS = R"(#version 300 es
layout(location=0) in vec2 aP; layout(location=1) in vec2 aU;
uniform vec2 uR; out vec2 vU;
void main(){ vec2 n=(aP/uR)*2.-1.; n.y=-n.y; gl_Position=vec4(n,0,1); vU=aU; })";
static const char* FS_S = R"(#version 300 es
precision mediump float; uniform vec4 uC; out vec4 fc; void main(){ fc=uC; })";
static const char* FS_T = R"(#version 300 es
precision mediump float; in vec2 vU; uniform sampler2D uT; uniform vec4 uC; out vec4 fc;
void main(){ float a=texture(uT,vU).r; fc=vec4(uC.rgb,uC.a*a); })";

static GLuint mksh(GLenum t, const char* s){
    GLuint h=glCreateShader(t);
    glShaderSource(h,1,&s,nullptr);
    glCompileShader(h);
    return h;
}
static GLuint mkprog(const char* v, const char* f){
    GLuint p=glCreateProgram();
    glAttachShader(p,mksh(GL_VERTEX_SHADER,v));
    glAttachShader(p,mksh(GL_FRAGMENT_SHADER,f));
    glLinkProgram(p);
    return p;
}

static void initGL(int w, int h){
    if(g_gl_ready && g_sw==w && g_sh==h) return;
    g_sw=w; g_sh=h;
    if(!g_gl_ready){
        g_prog_s=mkprog(VS,FS_S);
        g_prog_t=mkprog(VS,FS_T);
        glGenVertexArrays(1,&g_vao);
        glGenBuffers(1,&g_vbo);
        glBindVertexArray(g_vao);
        glBindBuffer(GL_ARRAY_BUFFER,g_vbo);
        glBufferData(GL_ARRAY_BUFFER,sizeof(float)*24,nullptr,GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
        glBindVertexArray(0);
        // Font tex
        const int NC=96,CW=8,CH=8;
        std::vector<uint8_t> tex(NC*CW*CH,0);
        for(int c=0;c<NC;c++)
            for(int r=0;r<CH;r++){
                uint8_t b=FONT[c][r];
                for(int col=0;col<CW;col++)
                    if((b>>(7-col))&1)
                        tex[r*(NC*CW)+c*CW+col]=255;
            }
        glGenTextures(1,&g_font_tex);
        glBindTexture(GL_TEXTURE_2D,g_font_tex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R8,NC*CW,CH,0,GL_RED,GL_UNSIGNED_BYTE,tex.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D,0);
        // UI init posisi
        g_px = w - PW - 12;
        g_py = h * 0.22f;
        g_gl_ready=true;
        LOGI("GL init %dx%d", w, h);
    }
}

static void quad(float x,float y,float w,float h,
                 float r,float g,float b,float a){
    glUseProgram(g_prog_s);
    glUniform2f(glGetUniformLocation(g_prog_s,"uR"),(float)g_sw,(float)g_sh);
    glUniform4f(glGetUniformLocation(g_prog_s,"uC"),r,g,b,a);
    float v[24]={x,y,0,0, x+w,y,1,0, x,y+h,0,1,
                 x+w,y,1,0, x+w,y+h,1,1, x,y+h,0,1};
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER,g_vbo);
    glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(v),v);
    glDrawArrays(GL_TRIANGLES,0,6);
    glBindVertexArray(0);
}

static float text(const char* t,float x,float y,float sc,
                  float r,float g,float b,float a){
    const int NC=96,CW=8,CH=8;
    float cw=CW*sc,ch=CH*sc,tw=(float)(NC*CW);
    glUseProgram(g_prog_t);
    glUniform2f(glGetUniformLocation(g_prog_t,"uR"),(float)g_sw,(float)g_sh);
    glUniform4f(glGetUniformLocation(g_prog_t,"uC"),r,g,b,a);
    glUniform1i(glGetUniformLocation(g_prog_t,"uT"),0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,g_font_tex);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER,g_vbo);
    float px=x;
    for(int i=0;t[i];i++){
        int idx=(int)t[i]-32;
        if(idx<0||idx>=NC){px+=cw;continue;}
        float u0=(float)(idx*CW)/tw,u1=(float)(idx*CW+CW)/tw;
        float v[24]={px,y,u0,0, px+cw,y,u1,0, px,y+ch,u0,1,
                     px+cw,y,u1,0, px+cw,y+ch,u1,1, px,y+ch,u0,1};
        glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(v),v);
        glDrawArrays(GL_TRIANGLES,0,6);
        px+=cw;
    }
    glBindTexture(GL_TEXTURE_2D,0);
    glBindVertexArray(0);
    return px-x;
}

static void renderUI(){
    if(!g_gl_ready) return;

    // Tombol toggle pojok kanan
    float bx=g_sw-56, by=g_py-56, bs=48;
    quad(bx,by,bs,bs, 0.15f,0.15f,0.2f,0.9f);
    quad(bx,by,bs,3,  0.3f,0.6f,1.0f,1.0f);
    float lw=text("XYZ",0,0,2.2f,1,1,1,0); // ukur
    lw=strlen("XYZ")*8*2.2f;
    text("XYZ", bx+(bs-lw)/2, by+(bs-8*2.2f)/2, 2.2f, 1,1,1,1);

    if(!g_visible) return;

    float px=g_px, py=g_py, pw=PW, ph=PH;

    // Shadow
    quad(px+4,py+4,pw,ph, 0,0,0,0.4f);
    // BG
    quad(px,py,pw,ph, 0.08f,0.08f,0.12f,0.93f);
    // Header
    quad(px,py,pw,38, 0.18f,0.45f,0.85f,1.0f);
    quad(px,py,pw,3,  0.4f,0.7f,1.0f,1.0f);

    text("Hand Position", px+12,py+12, 2.2f, 1,1,1,1);
    text(g_minimized?"[+]":"[-]", px+pw-36,py+12, 2.2f, 1,1,0.8f,1);

    if(g_minimized) return;

    // Sliders X, Y, Z
    const char* lbl[3]={"X","Y","Z"};
    float cr[3]={0.9f,0.3f,0.3f}, cg[3]={0.3f,0.85f,0.3f}, cb[3]={0.35f,0.5f,1.0f};
    float* vals[3]={&g_hand_x,&g_hand_y,&g_hand_z};

    float sx=px+18, sw2=pw-36, sy=py+50, rh=75;

    for(int i=0;i<3;i++){
        float ry=sy+i*rh;
        float r=cr[i],g=cg[i],b=cb[i];

        // Label
        text(lbl[i], sx,ry, 2.5f, r,g,b,1);

        // Nilai
        char vs[16]; snprintf(vs,16,"%.2f",*vals[i]);
        float vw=strlen(vs)*8*2.0f;
        text(vs, px+pw-vw-14,ry, 2.0f, 1,1,1,0.9f);

        // Track
        float ty=ry+24,th=10;
        quad(sx,ty,sw2,th, 0.2f,0.2f,0.25f,0.9f);

        // Fill
        float pct=(*vals[i]+1.0f)/2.0f;
        if(pct>0) quad(sx,ty,pct*sw2,th, r,g,b,0.85f);

        // Knob
        float kx=sx+pct*sw2-12;
        quad(kx+2,ty-6+2,24,22, 0,0,0,0.3f);
        quad(kx,ty-6,24,22, r*1.1f,g*1.1f,b*1.1f,1.0f);
        quad(kx,ty-6,24,3, 1,1,1,0.5f);

        // - +
        quad(sx-2,ty-8,20,20, 0.2f,0.2f,0.25f,0.9f);
        text("-", sx+2,ty-6, 2.0f, 1,1,1,1);
        quad(sx+sw2-18,ty-8,20,20, 0.2f,0.2f,0.25f,0.9f);
        text("+", sx+sw2-14,ty-6, 2.0f, 1,1,1,1);
    }

    // RESET
    float ry2=sy+3*rh+8;
    float bw=pw-36;
    quad(sx,ry2,bw,32, 0.55f,0.15f,0.15f,0.9f);
    quad(sx,ry2,bw,3, 0.9f,0.3f,0.3f,1.0f);
    const char* rs="RESET";
    float rw=strlen(rs)*8*2.2f;
    text(rs, sx+(bw-rw)/2,ry2+8, 2.2f, 1,1,1,1);

    // Border bawah
    quad(px,py+ph-3,pw,3, 0.18f,0.45f,0.85f,0.7f);
}

// ═══════════════════════════════════════
//  RENDER CALLBACK (dipanggil oleh PreloaderInput)
// ═══════════════════════════════════════
static void onRender(EGLDisplay dpy, EGLSurface surf,
                     int w, int h, void* ud){
    initGL(w, h);

    GLint prevProg;
    glGetIntegerv(GL_CURRENT_PROGRAM,&prevProg);
    GLboolean blend=glIsEnabled(GL_BLEND);
    GLboolean depth=glIsEnabled(GL_DEPTH_TEST);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    renderUI();

    if(!blend) glDisable(GL_BLEND);
    if(depth)  glEnable(GL_DEPTH_TEST);
    glUseProgram(prevProg);
}

// ═══════════════════════════════════════
//  INPUT CALLBACK
// ═══════════════════════════════════════
static bool hitR(float tx,float ty,float rx,float ry,float rw,float rh){
    return tx>=rx&&tx<=rx+rw&&ty>=ry&&ty<=ry+rh;
}

static bool onInput(int action, int pid, float tx, float ty, void* ud){
    // action: 0=down, 1=up, 2=move
    float bx=g_sw-56, by=g_py-56, bs=48;
    float sx=g_px+18, sw2=PW-36, sy=g_py+50, rh=75;
    float* vals[3]={&g_hand_x,&g_hand_y,&g_hand_z};

    if(action==0){ // DOWN
        // Toggle btn
        if(hitR(tx,ty,bx,by,bs,bs)){
            g_visible=!g_visible; return true;
        }
        if(!g_visible) return false;
        // Minimize
        if(hitR(tx,ty,g_px+PW-42,g_py+8,42,30)){
            g_minimized=!g_minimized; return true;
        }
        if(g_minimized) return false;
        for(int i=0;i<3;i++){
            float ry=sy+i*rh, ty2=ry+16;
            // Minus
            if(hitR(tx,ty,sx-2,ty2,20,20)){
                std::lock_guard<std::mutex> lk(g_mtx);
                *vals[i]=fmaxf(-1.0f,*vals[i]-0.05f);
                return true;
            }
            // Plus
            if(hitR(tx,ty,sx+sw2-18,ty2,20,20)){
                std::lock_guard<std::mutex> lk(g_mtx);
                *vals[i]=fminf(1.0f,*vals[i]+0.05f);
                return true;
            }
            // Slider
            if(hitR(tx,ty,sx,ry+16,sw2,22)){
                g_drag_axis=i; g_drag_id=pid;
                float pct=fmaxf(0,fminf(1,(tx-sx)/sw2));
                std::lock_guard<std::mutex> lk(g_mtx);
                *vals[i]=pct*2.0f-1.0f;
                return true;
            }
        }
        // Reset
        float ry2=sy+3*rh+8;
        if(hitR(tx,ty,sx,ry2,PW-36,32)){
            std::lock_guard<std::mutex> lk(g_mtx);
            g_hand_x=g_hand_y=g_hand_z=0;
            return true;
        }
    }
    else if(action==2 && g_drag_id==pid && g_drag_axis>=0){ // MOVE
        float pct=fmaxf(0,fminf(1,(tx-sx)/sw2));
        std::lock_guard<std::mutex> lk(g_mtx);
        *vals[g_drag_axis]=pct*2.0f-1.0f;
        return true;
    }
    else if(action==1 && g_drag_id==pid){ // UP
        g_drag_axis=-1; g_drag_id=-1;
    }
    return false;
}

// ═══════════════════════════════════════
//  INIT VIA .init_array
// ═══════════════════════════════════════
static void __attribute__((constructor)) mod_init(){
    LOGI("HandPosMod v2 init...");

    GlossInit();

    // Cari GetPreloaderInput dari libpreloader.so
    void* libPL = GlossOpen("libpreloader.so");
    if(!libPL){ LOGE("libpreloader.so not found"); return; }

    typedef void* (*GetPI_t)();
    GetPI_t getPI = (GetPI_t)GlossSymbol(libPL, "GetPreloaderInput");
    if(!getPI){ LOGE("GetPreloaderInput not found"); return; }

    void* plInput = getPI();
    if(!plInput){ LOGE("GetPreloaderInput returned null"); return; }

    LOGI("Got PreloaderInput @ %p", plInput);

    // Register callbacks ke PreloaderInput
    // Struktur PLInput: array of function pointers
    // [0] = registerRenderCallback
    // [1] = registerInputCallback
    // (berdasarkan analisis libAxioClient.so)
    typedef void (*RegRender_t)(void* cb, void* ud);
    typedef void (*RegInput_t)(void* cb, void* ud);

    void** vtbl = (void**)plInput;

    // Coba register render callback
    RegRender_t regRender = (RegRender_t)vtbl[0];
    if(regRender){
        regRender((void*)onRender, nullptr);
        LOGI("Render callback registered");
    }

    // Coba register input callback
    RegInput_t regInput = (RegInput_t)vtbl[1];
    if(regInput){
        regInput((void*)onInput, nullptr);
        LOGI("Input callback registered");
    }

    LOGI("HandPosMod v2 ready!");
}
