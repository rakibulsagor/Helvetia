#include <glib.h>
#include <stdio.h>
#include <string.h>
#include "db.h"

int main(void) {
    GError *error = NULL;

    if (!helvetia_db_init(&error)) {
        fprintf(stderr, "init failed: %s\n", error->message);
        return 1;
    }

    /* --- Notes --- */
    gint64 note_id = helvetia_note_insert("Test note", "Hello, world!", &error);
    if (note_id < 0) { fprintf(stderr, "%s\n", error->message); return 1; }

    GPtrArray *notes = helvetia_note_list(&error);
    g_print("Notes: %u\n", notes->len);
    g_ptr_array_unref(notes);

    /* --- Favorites --- */
    helvetia_favorite_add("tool", "pdf.merge", "Merge PDFs", &error);
    helvetia_favorite_add("tool", "utility.hash", "Hash", &error);

    GPtrArray *favs = helvetia_favorite_list(NULL, &error);
    g_print("Favorites: %u\n", favs->len);
    g_ptr_array_unref(favs);

    /* --- Vault meta --- */
    const char *salt = "0123456789abcdef";
    helvetia_vault_set_meta("salt", salt, strlen(salt), &error);
    GBytes *retrieved = helvetia_vault_get_meta("salt", &error);
    g_print("Vault salt round-trip: %s\n",
            retrieved ? "OK" : "FAIL");
    if (retrieved) g_bytes_unref(retrieved);

    helvetia_db_shutdown();
    return 0;
}
