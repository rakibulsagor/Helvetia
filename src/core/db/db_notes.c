#include "db.h"

gint64 helvetia_note_insert(const char *title, const char *body,
                            GError **error) {
    sqlite3_stmt *stmt = helvetia_db_prepare(
        "INSERT INTO notes (title, body) VALUES (?1, ?2);", error);
    if (!stmt) return -1;

    sqlite3_bind_text(stmt, 1, title ? title : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, body ? body : "", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_QUERY,
                    "note_insert failed: %s",
                    sqlite3_errmsg(helvetia_db_handle()));
        return -1;
    }
    return sqlite3_last_insert_rowid(helvetia_db_handle());
}

gboolean helvetia_note_update(gint64 id, const char *title,
                              const char *body, GError **error) {
    sqlite3_stmt *stmt = helvetia_db_prepare(
        "UPDATE notes SET title = ?1, body = ?2, "
        "updated_at = unixepoch() WHERE id = ?3;",
        error);
    if (!stmt) return FALSE;

    sqlite3_bind_text(stmt, 1, title ? title : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, body ? body : "", -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_QUERY,
                    "note_update failed: %s",
                    sqlite3_errmsg(helvetia_db_handle()));
        return FALSE;
    }
    return TRUE;
}

gboolean helvetia_note_delete(gint64 id, GError **error) {
    sqlite3_stmt *stmt = helvetia_db_prepare(
        "DELETE FROM notes WHERE id = ?1;", error);
    if (!stmt) return FALSE;

    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_QUERY,
                    "note_delete failed: %s",
                    sqlite3_errmsg(helvetia_db_handle()));
        return FALSE;
    }
    return TRUE;
}

GPtrArray *helvetia_note_list(GError **error) {
    sqlite3_stmt *stmt = helvetia_db_prepare(
        "SELECT id, title, body, created_at, updated_at, pinned "
        "FROM notes ORDER BY pinned DESC, updated_at DESC;",
        error);
    if (!stmt) return NULL;

    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)helvetia_note_free);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HelvetiaNote *n = g_new0(HelvetiaNote, 1);
        n->id         = sqlite3_column_int64(stmt, 0);
        n->title      = g_strdup((const char *)sqlite3_column_text(stmt, 1));
        n->body       = g_strdup((const char *)sqlite3_column_text(stmt, 2));
        n->created_at = sqlite3_column_int64(stmt, 3);
        n->updated_at = sqlite3_column_int64(stmt, 4);
        n->pinned     = sqlite3_column_int(stmt, 5) != 0;
        g_ptr_array_add(result, n);
    }
    sqlite3_finalize(stmt);
    return result;
}

void helvetia_note_free(HelvetiaNote *n) {
    if (!n) return;
    g_free(n->title);
    g_free(n->body);
    g_free(n);
}
