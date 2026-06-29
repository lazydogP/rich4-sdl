#include "platform/sdl1/text/text_backend.h"

#include <stddef.h>

/* R10.2a: null-safe dispatch helpers for the text backend interface. Concrete
 * backends (FreeType desktop / prebaked atlas handheld) plug into the vtable. */

int rich4_text_backend_measure(Rich4TextBackend *backend,
                               const char *big5_text,
                               const Rich4TextStyle *style,
                               int *out_width,
                               int *out_height)
{
    if (out_width != NULL) *out_width = 0;
    if (out_height != NULL) *out_height = 0;
    if (backend == NULL || backend->measure == NULL ||
        big5_text == NULL || style == NULL) {
        return 0;
    }
    return backend->measure(backend, big5_text, style, out_width, out_height);
}

int rich4_text_backend_render(Rich4TextBackend *backend,
                              Rich4TextTarget *target,
                              int x,
                              int y,
                              const char *big5_text,
                              const Rich4TextStyle *style)
{
    if (backend == NULL || backend->render == NULL ||
        target == NULL || target->pixels == NULL ||
        big5_text == NULL || style == NULL) {
        return 0;
    }
    return backend->render(backend, target, x, y, big5_text, style);
}

void rich4_text_backend_shutdown(Rich4TextBackend *backend)
{
    if (backend != NULL && backend->shutdown != NULL) {
        backend->shutdown(backend);
    }
}
