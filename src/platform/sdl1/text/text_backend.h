#ifndef RICH4_PLATFORM_SDL1_TEXT_BACKEND_H
#define RICH4_PLATFORM_SDL1_TEXT_BACKEND_H

#include <stdint.h>

#include "platform/sdl1/text_rules.h"

/* R10.2a: platform-neutral text backend interface.
 *
 * The interface deliberately targets a bare RGB565 framebuffer descriptor
 * instead of SDL_Surface so the same backends can drive a future non-SDL
 * handheld target. Layout/anchor/effect decisions are shared through
 * text_rules.c; a backend is only responsible for turning Big5 text into
 * glyph coverage on the target.
 *
 * Text is Big5 (DBCS) as stored in the original Rich4 resources. */

typedef struct Rich4TextTarget {
    uint16_t *pixels;   /* borrowed RGB565 framebuffer */
    int pitch_bytes;    /* bytes between rows */
    int width;          /* clip width in pixels */
    int height;         /* clip height in pixels */
} Rich4TextTarget;

typedef struct Rich4TextStyle {
    int height;   /* nominal glyph cell height in pixels */
    int style;    /* RICH4_TEXT_STYLE_* bit flags from text_rules.h */
    int spacing;  /* extra inter-character spacing */
    int mode;     /* anchor/orientation mode (see text_rules anchor rules) */
    uint16_t fg;  /* RGB565 foreground */
    uint16_t shadow; /* RGB565 shadow/outline color */
} Rich4TextStyle;

typedef struct Rich4TextBackend Rich4TextBackend;

struct Rich4TextBackend {
    /* Measure the un-anchored bounding box of the rendered text. Returns
     * non-zero on success and writes pixel width/height. */
    int (*measure)(Rich4TextBackend *self,
                   const char *big5_text,
                   const Rich4TextStyle *style,
                   int *out_width,
                   int *out_height);
    /* Render text with (x,y) treated per the style mode anchor. Returns
     * non-zero when the backend drew the text. */
    int (*render)(Rich4TextBackend *self,
                  Rich4TextTarget *target,
                  int x,
                  int y,
                  const char *big5_text,
                  const Rich4TextStyle *style);
    /* Release any backend-owned resources (fonts, caches, atlas). */
    void (*shutdown)(Rich4TextBackend *self);
    void *ctx; /* backend-private mutable state */
    const void *borrowed_ctx; /* backend-private read-only borrowed state */
};

/* Convenience dispatch wrappers (null-safe). */
int rich4_text_backend_measure(Rich4TextBackend *backend,
                               const char *big5_text,
                               const Rich4TextStyle *style,
                               int *out_width,
                               int *out_height);
int rich4_text_backend_render(Rich4TextBackend *backend,
                              Rich4TextTarget *target,
                              int x,
                              int y,
                              const char *big5_text,
                              const Rich4TextStyle *style);
void rich4_text_backend_shutdown(Rich4TextBackend *backend);

#endif
