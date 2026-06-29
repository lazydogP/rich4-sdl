#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* R10.5b: gate that the removed on-screen DEBUG/placeholder UI does not
 * reappear in the SDL1 platform layer. Scans every .c under src/platform/sdl1
 * for forbidden symbols/strings. Call-syntax tokens ("name(") are used so the
 * removal-note comments (which write the names followed by a space) do not
 * trip the gate. Run from the repository root. */

static const char *g_forbidden[] = {
    "draw_menu_overlay(",        /* placeholder RICH4 SDL/START/OPTIONS/QUIT menu */
    "draw_game_preview_overlay(",/* GAME / START PREVIEW debug banner */
    "draw_debug_cursor(",        /* debug crosshair cursor */
    "debug placeholder menu",    /* placeholder-menu diagnostic string */
    "synthetic route",           /* synthetic map-route scaffolding string */
};

static int g_failures = 0;

static int ends_with(const char *s, const char *suffix)
{
    size_t ls = strlen(s);
    size_t lf = strlen(suffix);
    return ls >= lf && strcmp(s + (ls - lf), suffix) == 0;
}

static void scan_file(const char *path)
{
    FILE *fp;
    long size;
    char *text;
    size_t i;

    if (!ends_with(path, ".c")) {
        return;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 ||
        fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return;
    }
    text = (char *)malloc((size_t)size + 1u);
    if (text == NULL) {
        fclose(fp);
        return;
    }
    if (fread(text, 1, (size_t)size, fp) != (size_t)size) {
        free(text);
        fclose(fp);
        return;
    }
    text[size] = '\0';
    fclose(fp);
    for (i = 0; i < sizeof(g_forbidden) / sizeof(g_forbidden[0]); i++) {
        if (strstr(text, g_forbidden[i]) != NULL) {
            fprintf(stderr, "forbidden DEBUG UI token \"%s\" in %s\n",
                    g_forbidden[i], path);
            g_failures++;
        }
    }
    free(text);
}

static void walk(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *ent;
    if (d == NULL) {
        return;
    }
    while ((ent = readdir(d)) != NULL) {
        char path[1024];
        struct stat st;
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        if (stat(path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            walk(path);
        } else if (S_ISREG(st.st_mode)) {
            scan_file(path);
        }
    }
    closedir(d);
}

int main(void)
{
    walk("src/platform/sdl1");
    if (g_failures != 0) {
        fprintf(stderr, "no-debug-ui gate: %d violation(s)\n", g_failures);
        return 1;
    }
    printf("no-debug-ui gate: ok\n");
    return 0;
}
