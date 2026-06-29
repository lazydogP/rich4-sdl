#include "app/game_save.h"
/* R10.1d: primary resource-store/MKF acquisition moved verbatim out of
 * runtime_main.c. */

#include <stdio.h>

#include "platform/platform_profile.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/runtime_resource_bootstrap.h"
#include "platform/sdl1/runtime_trace.h"

static void profile_save(const char *phase, int begin, size_t bytes)
{
    static unsigned int started;
    if (begin) started = runtime_profile_now();
    else runtime_profile_span("save", phase, started, -1, bytes);
}

int game_open_runtime_resources(
    ResourceStore **resource_store, Rich4Mkf **mkf,
    const SdlOptions *options, char *mkf_path, size_t mkf_path_size)
{
    *resource_store = rich4_resource_store_create();
    if (*resource_store == NULL) {
        fprintf(stderr, "error: cannot create resource store\n");
        return 0;
    }
    runtime_profile_attach_resources(*resource_store);
    rich4_game_save_set_debug_observer(profile_save);
    /* The store owns all decoded bytes for the lifetime of the runtime.  The
     * current handle contract has no safe eviction lease. */
    if (!rich4_resource_store_configure_borrow_policy(
            *resource_store,
            RICH4_RESOURCE_CACHE_MAX_BYTES,
            RICH4_RESOURCE_EVICTION_ENABLED)) {
        fprintf(stderr, "error: resource profile requests unsafe eviction\n");
        rich4_resource_store_destroy(*resource_store);
        *resource_store = NULL;
        return 0;
    }
    *mkf = rich4_sdl_open_mkf_from_options(
        *resource_store, options, mkf_path, mkf_path_size);
    if (*mkf == NULL) {
        fprintf(stderr,
                "error: no known MKF file found in %s (tried data.mkf/Data.mkf/DATA.MKF/panel.mkf/Panel.mkf)\n",
                options->data_dir);
        rich4_resource_store_destroy(*resource_store);
        *resource_store = NULL;
        return 0;
    }
    printf("opened MKF: %s\n", mkf_path);
    return 1;
}
