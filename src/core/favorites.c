#include "favorites.h"

static GHashTable *favorite_ids;
static char *favorites_path;

static gint compare_ids(gconstpointer a, gconstpointer b)
{
    const char *const *left = a;
    const char *const *right = b;
    return g_strcmp0(*left, *right);
}

static void save_favorites(void)
{
    GKeyFile *key_file = g_key_file_new();
    GPtrArray *ids = g_ptr_array_new_with_free_func(g_free);
    GHashTableIter iter;
    gpointer key;

    g_hash_table_iter_init(&iter, favorite_ids);
    while (g_hash_table_iter_next(&iter, &key, NULL))
        g_ptr_array_add(ids, g_strdup(key));
    g_ptr_array_sort(ids, compare_ids);
    g_ptr_array_add(ids, NULL);

    g_key_file_set_string_list(key_file, "Favorites", "tools",
                               (const char * const *)ids->pdata,
                               ids->len - 1);
    GError *error = NULL;
    if (!g_key_file_save_to_file(key_file, favorites_path, &error)) {
        g_warning("Could not save favorites: %s", error->message);
        g_clear_error(&error);
    }
    g_ptr_array_unref(ids);
    g_key_file_unref(key_file);
}

void helvetia_favorites_init(void)
{
    if (favorite_ids)
        return;

    favorite_ids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    favorites_path = g_build_filename(g_get_user_state_dir(), "helvetia",
                                      "favorites.ini", NULL);
    char *directory = g_path_get_dirname(favorites_path);
    if (g_mkdir_with_parents(directory, 0700) != 0)
        g_warning("Could not create favorites directory: %s", directory);
    g_free(directory);

    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;
    if (g_key_file_load_from_file(key_file, favorites_path,
                                  G_KEY_FILE_NONE, &error)) {
        gsize count = 0;
        char **ids = g_key_file_get_string_list(key_file, "Favorites", "tools",
                                                &count, NULL);
        for (gsize i = 0; ids && i < count; i++) {
            if (*ids[i])
                g_hash_table_add(favorite_ids, g_strdup(ids[i]));
        }
        g_strfreev(ids);
    } else if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
        g_warning("Could not load favorites: %s", error->message);
    }
    g_clear_error(&error);
    g_key_file_unref(key_file);
}

void helvetia_favorites_shutdown(void)
{
    if (!favorite_ids)
        return;
    g_hash_table_unref(favorite_ids);
    favorite_ids = NULL;
    g_clear_pointer(&favorites_path, g_free);
}

gboolean helvetia_favorites_contains(const char *tool_id)
{
    helvetia_favorites_init();
    return tool_id && g_hash_table_contains(favorite_ids, tool_id);
}

gboolean helvetia_favorites_toggle(const char *tool_id)
{
    if (!tool_id || !*tool_id)
        return FALSE;
    helvetia_favorites_init();
    gboolean is_favorite = g_hash_table_contains(favorite_ids, tool_id);
    if (is_favorite)
        g_hash_table_remove(favorite_ids, tool_id);
    else
        g_hash_table_add(favorite_ids, g_strdup(tool_id));
    save_favorites();
    return !is_favorite;
}

guint helvetia_favorites_count(void)
{
    helvetia_favorites_init();
    return g_hash_table_size(favorite_ids);
}
