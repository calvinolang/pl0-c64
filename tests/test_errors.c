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

static void message_format(void) {
    char buf[ERR_MESSAGE_SIZE];
    int c;
    err_message(buf, E_UNDECLARED, 1);
    CHECK(strcmp(buf, "ERROR line 1: undeclared name") == 0);
    err_message(buf, E_DIV_ZERO, 40);
    CHECK(strcmp(buf, "ERROR line 40: division by zero") == 0);
    err_message(buf, E_STOPPED, 0);
    CHECK(strcmp(buf, "ERROR line 0: stopped") == 0);
    for (c = 0; c <= E_COUNT; c++) {                 /* longest texts fit */
        memset(buf, 'x', sizeof buf);
        err_message(buf, (uint8_t)c, 65535u);
        CHECK(memchr(buf, 0, sizeof buf) != NULL);
        CHECK(strncmp(buf, "ERROR line 65535: ", 18) == 0);
        CHECK(strcmp(buf + 18, err_text((uint8_t)c)) == 0);
    }
}

int main(void) {
    message_format();
    every_code_has_text();
    texts_unique();
    out_of_range_is_safe();
    TEST_MAIN_END();
}
