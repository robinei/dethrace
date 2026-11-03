#ifndef HARNESS_GLOW_TABLE_H
#define HARNESS_GLOW_TABLE_H

#include <stddef.h>

// Light sources in the game's textures (see tools/glow), for the OpenGL renderer's glow
enum {
    eGlow_light_other = 0,
    eGlow_light_head = 1,
    eGlow_light_tail = 2 // car tail lights: brighter while braking
};

typedef struct tGlow_entry {
    const char* name;            // pixelmap identifier, upper case
    float intensity;             // 0..1
    unsigned char indices[32];   // bit mask of the emissive palette indices
    int light_type;              // eGlow_light_*
    int rect_count;              // if non-zero, the glow is limited to these rectangles
    const short* rects;          // x, y, width, height for each
} tGlow_entry;

extern const tGlow_entry gGlow_table[];
extern const int gGlow_table_count;

#endif
