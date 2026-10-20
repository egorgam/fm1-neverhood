/* Independent reference: output the firmware decoder's PCM for comparison to Python audioop. */
#include "effects_kit.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    effects_voice_t voice;
    uint32_t index, i, n;
    FILE *file;
    assert(argc == 3);
    assert(effects_index(EFFECTS_BASE_NOTE-1u) == -1);
    assert(effects_index(EFFECTS_BASE_NOTE+EFFECTS_COUNT) == -1);
    assert(effects_resource(EFFECTS_COUNT) == 0 && effects_frames(EFFECTS_COUNT) == 0);
    index = (uint32_t)atoi(argv[1]);
    n = effects_frames(index);
    assert(n && index < EFFECTS_COUNT);
    effects_start(&voice, index);
    file = fopen(argv[2], "wb"); assert(file);
    for (i = 0; i < n; i++) {
        int16_t value = (int16_t)(effects_read(&voice, 1.0f, n)*32768.0f);
        fwrite(&value, sizeof value, 1, file);
    }
    assert(effects_read(&voice, 1.0f, n) == 0.0f && voice.finished);
    fclose(file);
    return 0;
}
