#include "test.h"
#include "core/errors.h"
#include <string.h>

static void every_code_has_text(void) {
    int c;
    for (c = 0; c < E_COUNT; c++) {
        const char *t = err_text((uint8_t)c);
        CHECK(t != NULL);
        CHECK(t && strlen(t) > 0);
        CHECK(t && strlen(t) <= 36);
    }
}

static void texts_unique(void) {
    int a, b;
    for (a = 0; a < E_COUNT; a++)
        for (b = a + 1; b < E_COUNT; b++)
            CHECK(strcmp(err_text((uint8_t)a), err_text((uint8_t)b)) != 0);
}

static void out_of_range_is_safe(void) {
    CHECK(strcmp(err_text(E_COUNT), "unknown error") == 0);
    CHECK(strcmp(err_text(255), "unknown error") == 0);
    CHECK(strcmp(err_text(E_NONE), "ok") == 0);
}

int main(void) {
    every_code_has_text();
    texts_unique();
    out_of_range_is_safe();
    TEST_MAIN_END();
}
