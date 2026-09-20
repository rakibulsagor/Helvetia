#include "db.h"

gint64 helvetia_favorite_add(const char *kind, const char *target,
                             const char *label, GError **error) {
    sqlite3_stmt *stmt = helvetia_db_prepare(
        "INSERT INTO favorites (kind, target, label) "
        "VALUES (?1, ?2, ?3) "
        "ON CONFLICT(kind, target) DO UPDATE SET label = excluded.label;",
        error);
    if (!stmt) return -1;

    sqlite3_bind_text(stmt, 1, kind, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, target, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, label ? label : "", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_QUERY,
                    "favorite_add failed: %s",
                    sqlite3_errmsg(helvetia_db_handle()));
        return -1;
    }
    return sqlite3_last_insert_rowid(helvetia_db_handle());
}

gboolean helvetia_favorite_remove(const char *kind, const char *target,
                                  GError **error) {
    sqlite3_stmt *stmt = helvetia_db_prepare(
        "DELETE FROM favorites WHERE kind = ?1 AND target = ?2;",
        error);
    if (!stmt) return FALSE;

    sqlite3_bind_text(stmt, 1, kind, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, target, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_QUERY,
                    "favorite_remove failed: %s",
                    sqlite3_errmsg(helvetia_db_handle()));
        return FALSE;
    }
    return TRUE;
}

GPtrArray *helvetia_favorite_list(const char *kind, GError **error) {
    sqlite3_stmt *stmt = NULL;
    if (kind) {
        stmt = helvetia_db_prepare(
            "SELECT id, kind, target, label, created_at "
            "FROM favorites WHERE kind = ?1 ORDER BY created_at DESC;",
            error);
        if (!stmt) return NULL;
        sqlite3_bind_text(stmt, 1, kind, -1, SQLITE_STATIC);
    } else {
        stmt = helvetia_db_prepare(
            "SELECT id, kind, target, label, created_at "
            "FROM favorites ORDER BY created_at DESC;",
            error);
        if (!stmt) return NULL;
    }

    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)helvetia_favorite_free);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HelvetiaFavorite *f = g_new0(HelvetiaFavorite, 1);
        f->id         = sqlite3_column_int64(stmt, 0);
        f->kind       = g_strdup((const char *)sqlite3_column_text(stmt, 1));
        f->target     = g_strdup((const char *)sqlite3_column_text(stmt, 2));
        f->label      = g_strdup((const char *)sqlite3_column_text(stmt, 3));
        f->created_at = sqlite3_column_int64(stmt, 4);
        g_ptr_array_add(result, f);
    }
    sqlite3_finalize(stmt);
    return result;
}

gboolean helvetia_favorite_contains(const char *kind, const char *target,
                                    GError **error) {
    sqlite3_stmt *stmt = helvetia_db_prepare(
        "SELECT 1 FROM favorites WHERE kind = ?1 AND target = ?2 LIMIT 1;",
        error);
    if (!stmt) return FALSE;

    sqlite3_bind_text(stmt, 1, kind, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, target, -1, SQLITE_STATIC);

    gboolean found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

void helvetia_favorite_free(HelvetiaFavorite *f) {
    if (!f) return;
    g_free(f->kind);
    g_free(f->target);
    g_free(f->label);
    g_free(f);
}
