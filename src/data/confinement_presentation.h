#ifndef RICH4_DATA_CONFINEMENT_PRESENTATION_H
#define RICH4_DATA_CONFINEMENT_PRESENTATION_H

typedef struct Rich4ConfinementPresentation {
    int kind, resource, effect, refresh_frame, screen_y;
} Rich4ConfinementPresentation;

/* Data.mkf (0x48A0E4), 0x43D699/0x43ED45 -> 0x45144F.
 * refresh_frame is one based; 0x4512BF rebuilds only the main map. */
static inline const Rich4ConfinementPresentation *
rich4_confinement_presentation_for_kind(int kind)
{
    static const Rich4ConfinementPresentation entries[] = {
        {1, 538, 94, 18, 40}, /* original confinement slot: prison */
        {2, 524, 92, 30, 210} /* original confinement slot: hospital */
    };
    int i;
    for (i = 0; i < 2; i++) if (entries[i].kind == kind) return &entries[i];
    return 0;
}

static inline const Rich4ConfinementPresentation *
rich4_confinement_presentation_for_resource(int resource)
{
    const Rich4ConfinementPresentation *entry;
    int kind;
    for (kind = 1; kind <= 2; kind++) {
        entry = rich4_confinement_presentation_for_kind(kind);
        if (entry && entry->resource == resource) return entry;
    }
    return 0;
}

#endif
