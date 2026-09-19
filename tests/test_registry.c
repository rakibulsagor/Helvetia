#include <glib.h>
#include <glib/gstdio.h>

#include "core/module_registry.h"
#include "core/tool_registry.h"
#include "core/favorites.h"

static const char *keywords[] = { "identifier", NULL };
static char *test_state_dir;
static const HelvetiaTool tools[] = {
    { "uuid", "UUID Generator", "Creates identifiers", NULL, keywords, "uuid", NULL },
    { NULL }
};
static const HelvetiaSubcategory categories[] = {
    { "Generators", tools },
    { NULL }
};
static const HelvetiaModule module = {
    "test-module", "Test module", NULL, NULL, categories, NULL, NULL, NULL, NULL
};

static void test_module_registration_indexes_tools(void)
{
    helvetia_tool_registry_init();
    helvetia_module_registry_init();
    helvetia_module_registry_add(&module);

    g_assert_nonnull(helvetia_tool_registry_find_by_cli("uuid"));

    GPtrArray *results = helvetia_tool_registry_search("identifier", 0);
    g_assert_cmpuint(results->len, ==, 1);
    g_ptr_array_unref(results);

    /* A duplicate module must not add duplicate search results. */
    helvetia_module_registry_add(&module);
    results = helvetia_tool_registry_search("uuid", 0);
    g_assert_cmpuint(results->len, ==, 1);
    g_ptr_array_unref(results);

    helvetia_module_registry_shutdown();
    helvetia_tool_registry_cleanup();
}

static void test_favorites_persist_locally(void)
{
    helvetia_favorites_init();
    g_assert_false(helvetia_favorites_contains("uuid"));
    g_assert_true(helvetia_favorites_toggle("uuid"));
    g_assert_true(helvetia_favorites_contains("uuid"));
    g_assert_cmpuint(helvetia_favorites_count(), ==, 1);
    helvetia_favorites_shutdown();

    helvetia_favorites_init();
    g_assert_true(helvetia_favorites_contains("uuid"));
    g_assert_false(helvetia_favorites_toggle("uuid"));
    helvetia_favorites_shutdown();
}

int main(int argc, char **argv)
{
    test_state_dir = g_dir_make_tmp("helvetia-tests-XXXXXX", NULL);
    g_assert_nonnull(test_state_dir);
    g_setenv("XDG_STATE_HOME", test_state_dir, TRUE);
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/registry/module-registration-indexes-tools",
                    test_module_registration_indexes_tools);
    g_test_add_func("/favorites/persist-locally", test_favorites_persist_locally);
    int status = g_test_run();
    char *app_dir = g_build_filename(test_state_dir, "helvetia", NULL);
    char *favorites = g_build_filename(app_dir, "favorites.ini", NULL);
    g_remove(favorites);
    g_rmdir(app_dir);
    g_rmdir(test_state_dir);
    g_free(favorites);
    g_free(app_dir);
    g_free(test_state_dir);
    return status;
}
