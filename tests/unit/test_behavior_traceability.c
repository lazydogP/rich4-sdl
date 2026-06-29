#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return 0;
    fclose(file);
    return 1;
}

static int file_contains(const char *path, const char *needle)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *text;
    int found;
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 ||
        (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL) fclose(file);
        return 0;
    }
    text = (char *)malloc((size_t)size + 1u);
    if (text == NULL) { fclose(file); return 0; }
    if (fread(text, 1, (size_t)size, file) != (size_t)size) {
        free(text); fclose(file); return 0;
    }
    text[size] = '\0';
    found = strstr(text, needle) != NULL;
    free(text);
    fclose(file);
    return found;
}

static int approved_class(const char *value)
{
    static const char *classes[] = {
        "confirmed-original", "characterized-compatible", "architecture",
        "format-mechanical", "mixed-explicit"
    };
    size_t i;
    for (i = 0; i < sizeof(classes) / sizeof(classes[0]); ++i)
        if (strcmp(value, classes[i]) == 0) return 1;
    return 0;
}

int main(void)
{
    FILE *file = fopen("docs/evidence/behavior-traceability.tsv", "rb");
    char line[2048];
    unsigned int rows = 0;
    int failed = 0;
    if (file == NULL || fgets(line, sizeof(line), file) == NULL) return 1;
    while (fgets(line, sizeof(line), file) != NULL) {
        char *fields[6];
        char *cursor = line;
        int i;
        for (i = 0; i < 6; ++i) {
            fields[i] = cursor;
            cursor = i == 5 ? NULL : strchr(cursor, '\t');
            if (i != 5 && cursor == NULL) break;
            if (cursor != NULL) *cursor++ = '\0';
        }
        if (i != 6) { failed = 1; continue; }
        fields[5][strcspn(fields[5], "\r\n")] = '\0';
        if (!file_exists(fields[0]) || !approved_class(fields[2]) ||
            fields[3][0] == '\0' || !file_exists(fields[4]) || !file_exists(fields[5])) {
            fprintf(stderr, "invalid traceability row: %s / %s\n", fields[0], fields[1]);
            failed = 1;
        }
        ++rows;
    }
    fclose(file);
    if (rows < 28) failed = 1;
    if (!file_contains("src/core/route_event.c", "ROUTE_EVENT_UNIMPLEMENTED_CONFIRMED") ||
        !file_contains("src/core/status_countdown.c", "STATUS_EFFECT_APPLIED") ||
        !file_contains("src/data/resource_catalog.c", "rich4_resource_catalog_unknown") ||
        !file_contains("src/resource/flc_animation.c", "RICH4_FLC_ERR_UNSUPPORTED_OPCODE") ||
        !file_contains("docs/evidence/40829d-map-render.md", "characterization") ||
        !file_contains("docs/evidence/40b93b-player-actor.md", "partial")) failed = 1;
    /* R10.5b: the previous DEBUG/placeholder scaffolding must NOT reappear.
     * These strings were requirements before R10.5; they are now prohibited. */
    if (file_contains("src/platform/sdl1/runtime_main.c", "synthetic route") ||
        file_contains("src/platform/sdl1/runtime_main.c", "debug placeholder menu")) {
        fprintf(stderr, "prohibited DEBUG scaffolding string reintroduced in runtime_main.c\n");
        failed = 1;
    }
    if (failed) return 1;
    printf("behavior traceability: %u rows ok\n", rows);
    return 0;
}
