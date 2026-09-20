#include "db.h"
#include <glib/gstdio.h>
#include <string.h>

GQuark helvetia_db_error_quark(void) {
    return g_quark_from_static_string("helvetia-db-error");
}

static sqlite3 *g_db = NULL;
static char    *g_db_path = NULL;

static char *compute_db_path(void) {
    const char *base = g_get_user_data_dir();
    char *dir = g_build_filename(base, "helvetia", NULL);
    g_mkdir_with_parents(dir, 0700);   /* 0700 — private */
    char *path = g_build_filename(dir, "data.db", NULL);
    g_free(dir);
    return path;
}

static gboolean apply_pragmas(sqlite3 *db, GError **error) {
    const char *pragmas[] = {
        "PRAGMA journal_mode = WAL;",       /* better concurrency */
        "PRAGMA synchronous = NORMAL;",     /* balance speed/safety */
        "PRAGMA foreign_keys = ON;",        /* enforce FKs */
        "PRAGMA busy_timeout = 5000;",      /* wait on locks */
        NULL
    };
    for (int i = 0; pragmas[i]; ++i) {
        char *err = NULL;
        if (sqlite3_exec(db, pragmas[i], NULL, NULL, &err) != SQLITE_OK) {
            g_set_error(error, HELVETIA_DB_ERROR,
                        HELVETIA_DB_ERROR_OPEN,
                        "pragma failed: %s", err);
            sqlite3_free(err);
            return FALSE;
        }
    }
    return TRUE;
}

static int schema_version(sqlite3 *db) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "PRAGMA user_version;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;
    int version = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        version = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return version;
}

static gboolean set_schema_version(sqlite3 *db, int version, GError **error) {
    char sql[64];
    g_snprintf(sql, sizeof sql, "PRAGMA user_version = %d;", version);
    return helvetia_db_exec(sql, error);
}

static gboolean migrate(sqlite3 *db, GError **error) {
    int current = schema_version(db);
    if (current < 0) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_MIGRATE,
                    "could not read schema version");
        return FALSE;
    }

    if (current == HELVETIA_DB_SCHEMA_VERSION)
        return TRUE;

    if (!helvetia_db_begin(error)) return FALSE;

    /* ---- Migration 0 → 1: initial schema ---- */
    if (current < 1) {
        const char *initial_schema =
            "CREATE TABLE IF NOT EXISTS vault ("
            "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  encrypted     BLOB    NOT NULL,"
            "  created_at    INTEGER NOT NULL DEFAULT (unixepoch()),"
            "  updated_at    INTEGER NOT NULL DEFAULT (unixepoch())"
            ");"

            "CREATE TABLE IF NOT EXISTS vault_meta ("
            "  key    TEXT PRIMARY KEY,"
            "  value  BLOB NOT NULL"
            ");"

            "CREATE TABLE IF NOT EXISTS notes ("
            "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  title         TEXT    NOT NULL DEFAULT '',"
            "  body          TEXT    NOT NULL DEFAULT '',"
            "  pinned        INTEGER NOT NULL DEFAULT 0,"
            "  created_at    INTEGER NOT NULL DEFAULT (unixepoch()),"
            "  updated_at    INTEGER NOT NULL DEFAULT (unixepoch())"
            ");"

            "CREATE INDEX IF NOT EXISTS notes_updated_at "
            "  ON notes (updated_at DESC);"

            "CREATE TABLE IF NOT EXISTS favorites ("
            "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  kind          TEXT    NOT NULL,"
            "  target        TEXT    NOT NULL,"
            "  label         TEXT    NOT NULL DEFAULT '',"
            "  created_at    INTEGER NOT NULL DEFAULT (unixepoch()),"
            "  UNIQUE (kind, target)"
            ");"

            "CREATE INDEX IF NOT EXISTS favorites_kind "
            "  ON favorites (kind);";

        if (!helvetia_db_exec(initial_schema, error)) {
            helvetia_db_rollback(NULL);
            return FALSE;
        }
    }

    if (!set_schema_version(db, HELVETIA_DB_SCHEMA_VERSION, error)) {
        helvetia_db_rollback(NULL);
        return FALSE;
    }

    if (!helvetia_db_commit(error))
        return FALSE;

    return TRUE;
}

gboolean helvetia_db_init(GError **error) {
    if (g_db) return TRUE;   /* already open */

    g_db_path = compute_db_path();

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    int rc = sqlite3_open_v2(g_db_path, &g_db, flags, NULL);
    if (rc != SQLITE_OK) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_OPEN,
                    "cannot open %s: %s",
                    g_db_path, sqlite3_errmsg(g_db));
        sqlite3_close(g_db);
        g_db = NULL;
        g_clear_pointer(&g_db_path, g_free);
        return FALSE;
    }

    if (!apply_pragmas(g_db, error)) {
        sqlite3_close(g_db);
        g_db = NULL;
        g_clear_pointer(&g_db_path, g_free);
        return FALSE;
    }

    if (!migrate(g_db, error)) {
        sqlite3_close(g_db);
        g_db = NULL;
        g_clear_pointer(&g_db_path, g_free);
        return FALSE;
    }

    return TRUE;
}

void helvetia_db_shutdown(void) {
    if (g_db) {
        /* Force WAL checkpoint on close */
        sqlite3_exec(g_db, "PRAGMA wal_checkpoint(TRUNCATE);",
                     NULL, NULL, NULL);
        sqlite3_close(g_db);
        g_db = NULL;
    }
    g_clear_pointer(&g_db_path, g_free);
}

sqlite3 *helvetia_db_handle(void) {
    return g_db;
}

sqlite3_stmt *helvetia_db_prepare(const char *sql, GError **error) {
    if (!g_db) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_OPEN, "db not initialized");
        return NULL;
    }
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_QUERY,
                    "prepare failed: %s", sqlite3_errmsg(g_db));
        return NULL;
    }
    return stmt;
}

gboolean helvetia_db_exec(const char *sql, GError **error) {
    if (!g_db) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_OPEN, "db not initialized");
        return FALSE;
    }
    char *err = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_QUERY,
                    "exec failed: %s", err ? err : "unknown");
        sqlite3_free(err);
        return FALSE;
    }
    return TRUE;
}

gboolean helvetia_db_begin(GError **error) {
    return helvetia_db_exec("BEGIN IMMEDIATE;", error);
}
gboolean helvetia_db_commit(GError **error) {
    return helvetia_db_exec("COMMIT;", error);
}
gboolean helvetia_db_rollback(GError **error) {
    return helvetia_db_exec("ROLLBACK;", error);
}
