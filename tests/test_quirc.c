#include <glib.h>

#include "modules/utility/quirc/quirc.h"

static void test_resize_rejects_invalid_dimensions(void)
{
    struct quirc *decoder = quirc_new();
    g_assert_nonnull(decoder);

    g_assert_cmpint(quirc_resize(decoder, 64, 64), ==, 0);
    g_assert_nonnull(quirc_begin(decoder, NULL, NULL));

    g_assert_cmpint(quirc_resize(decoder, 0, 64), ==, -1);
    g_assert_cmpint(quirc_resize(decoder, -1, 64), ==, -1);
    /* Failed resizes leave the valid previous allocation intact. */
    g_assert_nonnull(quirc_begin(decoder, NULL, NULL));

    quirc_destroy(decoder);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/quirc/resize-invalid-dimensions",
                    test_resize_rejects_invalid_dimensions);
    return g_test_run();
}
