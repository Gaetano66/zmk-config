#include <assert.h>
#include "../../src/rgb_fx_engine.h"

int main(void) {
    unsigned h = 0;
    for (unsigned i = 1; i <= 32; i++) {
        unsigned next = fx_palette_step(h, 1);
        assert(next == (i % 32) * 360 / 32);
        assert(fx_palette_step(next, -1) == h);
        h = next;
    }
    assert(h == 0);
}
