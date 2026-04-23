#pragma once
#include <stdint.h>
#include <EGL/egl.h>

#ifdef __cplusplus
extern "C" {
#endif

void  GlossInit();
void* GlossOpen(const char* lib);
void* GlossSymbol(void* handle, const char* sym);
int   GlossHook(void* target, void* hook, void** orig);
void* pl_resolve_signature(const char* lib, const char* pattern);

#ifdef __cplusplus
}
#endif

// ─────────────────────────────────────────────────
//  PreloaderInput API
//  Ini cara resmi LeviLauncher untuk render overlay
//  dan menerima touch input dari mod
// ─────────────────────────────────────────────────
#ifdef __cplusplus

// Callback yang dipanggil setiap frame untuk render
typedef void (*PLRenderCallback)(EGLDisplay dpy, EGLSurface surf,
                                  int width, int height, void* userdata);

// Callback yang dipanggil saat ada touch event
typedef bool (*PLInputCallback)(int action, int pointer_id,
                                 float x, float y, void* userdata);

struct PLInputCallbacks {
    PLRenderCallback on_render;  // dipanggil tiap frame
    PLInputCallback  on_input;   // dipanggil saat touch
    void*            userdata;   // data custom yang dikembalikan ke callback
};

// Fungsi untuk register mod ke PreloaderInput
// Dipanggil dari GlossSymbol(libpreloader, "GetPreloaderInput")
typedef void* (*GetPreloaderInput_t)();

// Struct yang dikembalikan GetPreloaderInput
struct PLInput {
    // Register callback render dan input
    void (*registerCallbacks)(PLInput* self, const PLInputCallbacks* cbs);
    // ... fields lain yang tidak kita butuhkan
    void* reserved[16];
};

#endif
