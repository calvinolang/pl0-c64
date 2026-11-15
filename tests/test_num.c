#include "test.h"
#include "platform/num.h"
#include <string.h>

static void P(num_t *r, const char *s) { CHECK_EQ_INT(num_parse(s, r), 0); }
static void fmt_is(const num_t *a, const char *want) {
    char b[16];
    num_format(a, b);
    test_checks++;
    if (strcmp(b, want) != 0) { test_failures++; printf("FAIL %s:%d: format got \"%s\" expected \"%s\"\n", __FILE__, __LINE__, b, want); }
}

static void copy_and_alias(void) {
    num_t a, b; P(&a, "2.5"); num_copy(&b, &a); fmt_is(&b, "2.5");
    num_add(&a, &a, &b); fmt_is(&a, "5");          /* r aliases a */
    num_mul(&b, &a, &b); fmt_is(&b, "12.5");       /* r aliases b */
}
static void format_integer(void) { num_t a; P(&a, "3.0"); fmt_is(&a, "3"); }
static void format_fraction(void) {
    num_t a, b, r; P(&a, "0.1"); P(&b, "0.2"); num_add(&r, &a, &b); fmt_is(&r, "0.3");
}
static void format_negative(void) { num_t a; P(&a, "-2.5"); fmt_is(&a, "-2.5"); }
static void format_huge(void) { num_t a; P(&a, "1e12"); fmt_is(&a, "1E+12"); P(&a, "1.5E12"); fmt_is(&a, "1.5E+12"); }
static void format_tiny(void) { num_t a; P(&a, "0.00001"); fmt_is(&a, "1E-05"); }
static void format_misc(void) {
    num_t a, z; P(&a, "0"); num_neg(&z, &a); fmt_is(&z, "0");
    P(&a, "999999999"); fmt_is(&a, "999999999");
    P(&a, "1e9"); fmt_is(&a, "1E+09");
    P(&a, "0.0001"); fmt_is(&a, "0.0001");
    P(&a, "-7"); fmt_is(&a, "-7");
    P(&a, "3.14159265358979"); fmt_is(&a, "3.14159265");
}
static void format_bounds(void) {
    num_t a;
    a = -1.23456789e100; fmt_is(&a, "-1.2345679E+100");
    a = -1.23456789e-100; fmt_is(&a, "-1.2345679E-100");
    a = 1e300; fmt_is(&a, "1E+300");
    a = 1e-300; fmt_is(&a, "1E-300");
    a = -1.7976931348623157e308; fmt_is(&a, "-1.7976931E+308");
    a = 999999999.5; fmt_is(&a, "1E+09");
    a = -999999999.5; fmt_is(&a, "-1E+09");
    a = -123456789.5; fmt_is(&a, "-123456790");
    a = -0.000123456789; fmt_is(&a, "-0.000123456789");
}
static void parse_ok(void) { num_t a, b; P(&a, "3.14"); num_from_int(&b, 3); CHECK(num_cmp(&a, &b) > 0); CHECK_EQ_INT(num_to_i16(&a), 3); }
static void parse_bad(void) { num_t a; CHECK_EQ_INT(num_parse("3.1.4", &a), -1); CHECK_EQ_INT(num_parse("", &a), -1); CHECK_EQ_INT(num_parse("abc", &a), -1); }
static void div_zero(void) { num_t a, z, r; num_from_int(&a, 1); num_from_int(&z, 0); CHECK(num_div(&r, &a, &z) != 0);
    num_from_int(&z, 4); CHECK_EQ_INT(num_div(&r, &a, &z), 0); fmt_is(&r, "0.25"); }
static void odd(void) {
    num_t a; num_from_int(&a, 3); CHECK(num_is_odd(&a));
    num_from_int(&a, 4); CHECK(!num_is_odd(&a));
    num_from_int(&a, -3); CHECK(num_is_odd(&a));
    P(&a, "2.5"); CHECK(!num_is_odd(&a));
    P(&a, "3.5"); CHECK(num_is_odd(&a));
}
static void fn_domain(void) {
    num_t a, r; num_from_int(&a, -1); CHECK(num_fn(NUMFN_SQR, &r, &a) != 0);
    num_from_int(&a, 0); CHECK(num_fn(NUMFN_LOG, &r, &a) != 0);
    num_from_int(&a, 9); CHECK_EQ_INT(num_fn(NUMFN_SQR, &r, &a), 0); fmt_is(&r, "3");
    P(&a, "-2.5"); CHECK_EQ_INT(num_fn(NUMFN_INT, &r, &a), 0); fmt_is(&r, "-3");
    CHECK_EQ_INT(num_fn(NUMFN_ABS, &r, &a), 0); fmt_is(&r, "2.5");
    P(&a, "0"); CHECK_EQ_INT(num_fn(NUMFN_SIN, &r, &a), 0); fmt_is(&r, "0");
    CHECK_EQ_INT(num_fn(NUMFN_COS, &r, &a), 0); fmt_is(&r, "1");
    CHECK_EQ_INT(num_fn(NUMFN_ATN, &r, &a), 0); fmt_is(&r, "0");
    CHECK_EQ_INT(num_fn(NUMFN_EXP, &r, &a), 0); fmt_is(&r, "1");
    P(&a, "1"); CHECK_EQ_INT(num_fn(NUMFN_LOG, &r, &a), 0); fmt_is(&r, "0");
    CHECK_EQ_INT(num_fn(NUMFN_ATN, &r, &a), 0); fmt_is(&r, "0.785398163");
    P(&a, "1000"); CHECK(num_fn(NUMFN_EXP, &r, &a) != 0);
}
static void to_i16_saturates(void) {
    num_t n; n = NAN; CHECK_EQ_INT(num_to_i16(&n), 0);
    num_t a; P(&a, "1e9"); CHECK_EQ_INT(num_to_i16(&a), 32767);
    P(&a, "-1e9"); CHECK_EQ_INT(num_to_i16(&a), -32768);
    P(&a, "-2.5"); CHECK_EQ_INT(num_to_i16(&a), -3);
    P(&a, "2.5"); CHECK_EQ_INT(num_to_i16(&a), 2);
}

int main(void) {
    copy_and_alias(); format_integer(); format_fraction(); format_negative(); format_huge(); format_tiny(); format_misc(); format_bounds();
    parse_ok(); parse_bad(); div_zero(); odd(); fn_domain(); to_i16_saturates();
    TEST_MAIN_END();
}
