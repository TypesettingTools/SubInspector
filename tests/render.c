#include "SubInspector.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static const char header[] =
    "[Script Info]\nScriptType: v4.00+\nPlayResX: 640\nPlayResY: 480\n"
    "[V4+ Styles]\n"
    "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, "
    "Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, "
    "Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
    "Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,"
    "0,0,0,0,100,100,0,0,1,0,0,7,0,0,0,1\n"
    "[Events]\n"
    "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";

static SI_Rect render(SI_State *state, const char *script) {
    const int32_t times[] = {0, 500, 2500};
    SI_Rect rects[3] = {{0}};
    CHECK(si_setScript(state, script, 0) == 0);
    CHECK(si_calculateBounds(state, rects, times, 3) == 0);
    CHECK(rects[0].w > 0 && rects[0].h > 0);
    CHECK(rects[0].x == rects[1].x && rects[0].y == rects[1].y);
    CHECK(rects[0].w == rects[1].w && rects[0].h == rects[1].h);
    CHECK(rects[0].hash == rects[1].hash);
    CHECK(rects[2].w == 0 && rects[2].h == 0);
    return rects[0];
}

int main(void) {
    CHECK(si_getVersion() == SI_VERSION);
    SI_State *state = si_init(640, 480, NULL, NULL);
    CHECK(state != NULL);
    CHECK(si_setHeader(state, header, 0) == 0);
    const char *shape =
        "Dialogue: 0,0:00:00.00,0:00:02.00,Default,,0,0,0,,"
        "{\\an7\\pos(100,100)\\p1}m 0 0 l 20 0 20 20 0 20\n";
    SI_Rect rect = render(state, shape);
    CHECK(rect.x == 100 && rect.y == 100 && rect.w == 20 && rect.h == 20);
    CHECK(rect.solid == 1);
    CHECK(render(state, shape).hash == rect.hash);

    /* Change only the first of two rendered images. This catches the old
       bug where only the final image contributed to the pixel hash. */
    const char *two_shapes =
        "Dialogue: 0,0:00:00.00,0:00:02.00,Default,,0,0,0,,"
        "{\\an7\\pos(100,100)\\p1}m 0 0 l 20 0 20 20 0 20\n"
        "Dialogue: 1,0:00:00.00,0:00:02.00,Default,,0,0,0,,"
        "{\\an7\\pos(200,100)\\p1}m 0 0 l 20 0 20 20 0 20\n";
    const char *changed_first =
        "Dialogue: 0,0:00:00.00,0:00:02.00,Default,,0,0,0,,"
        "{\\an7\\pos(100,100)\\p1}m 0 0 l 30 0 30 20 0 20\n"
        "Dialogue: 1,0:00:00.00,0:00:02.00,Default,,0,0,0,,"
        "{\\an7\\pos(200,100)\\p1}m 0 0 l 20 0 20 20 0 20\n";
    SI_Rect original = render(state, two_shapes);
    SI_Rect changed = render(state, changed_first);
    CHECK(original.x == changed.x && original.y == changed.y);
    CHECK(original.w == changed.w && original.h == changed.h);
    CHECK(original.hash != changed.hash);

    /* Exercise the platform font provider and shaping dependencies. */
    render(state, "Dialogue: 0,0:00:00.00,0:00:02.00,Default,,0,0,0,,Hello SubInspector\n");
    si_cleanup(state);
    puts("Version, bounds, repeated frames, multi-image hashes, and font rendering passed.");
    return 0;
}
