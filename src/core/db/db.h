#pragma once
#include <glib.h>
#include <sqlite3.h>

G_BEGIN_DECLS

#define HELVETIA_DB_ERROR (helvetia_db_error_quark())

typedef enum {
    HELVETIA_DB_ERROR_OPEN,
    HELVETIA_DB_ERROR_MIGRATE,
    HELVETIA_DB_ERROR_QUERY,
    HELVETIA_DB_ERROR_CONSTRAINT,
    HELVETIA_DB_ERROR_NOT_FOUND,
    HELVETIA_DB_ERROR_INTERNAL,
} HelvetiaDbError;

GQuark helvetia_db_error_quark(void);

/* Open (or create) the database at the default location.
   Applies migrations. Returns NULL on failure. */
gboolean helvetia_db_init(GError **error);
void     helvetia_db_shutdown(void);

/* Get the raw handle for advanced use. Do not close. */
sqlite3 *helvetia_db_handle(void);

#define HELVETIA_DB_SCHEMA_VERSION 1

/* Prepare a statement. On failure, sets *error. */
sqlite3_stmt *helvetia_db_prepare(const char *sql, GError **error);

/* Execute a statement with no result rows. */
gboolean helvetia_db_exec(const char *sql, GError **error);

gboolean helvetia_db_begin(GError **error);
gboolean helvetia_db_commit(GError **error);
gboolean helvetia_db_rollback(GError **error);

/* ---- Vault ---- */
typedef struct {
    gint64  id;
    gint64  created_at;
    gint64  updated_at;
    GBytes *encrypted_data;   /* caller must unref */
} HelvetiaVaultEntry;

gboolean helvetia_vault_init_schema(GError **error);
void helvetia_vault_entry_free(HelvetiaVaultEntry *e);

gboolean helvetia_vault_set_meta(const char *key,
                                 const void *value, gsize len,
                                 GError **error);
GBytes *helvetia_vault_get_meta(const char *key, GError **error);
gint64 helvetia_vault_insert(GBytes *encrypted, GError **error);
GPtrArray *helvetia_vault_list(GError **error);

gboolean helvetia_vault_create(const char *master_password, GError **error);
unsigned char *helvetia_vault_unlock(const char *master_password, GError **error);
GBytes *helvetia_vault_encrypt_entry(const unsigned char *dek, const char *plaintext, GError **error);

/* ---- Notes ---- */
typedef struct {
    gint64  id;
    char   *title;
    char   *body;
    gint64  created_at;
    gint64  updated_at;
    gboolean pinned;
} HelvetiaNote;

gint64 helvetia_note_insert(const char *title, const char *body, GError **error);
gboolean helvetia_note_update(gint64 id, const char *title, const char *body, GError **error);
gboolean helvetia_note_delete(gint64 id, GError **error);
GPtrArray *helvetia_note_list(GError **error);
void helvetia_note_free(HelvetiaNote *n);

/* ---- Favorites ---- */
typedef struct {
    gint64  id;
    char   *kind;     /* "tool", "file", "url" */
    char   *target;   /* tool_id, path, or URL */
    char   *label;
    gint64  created_at;
} HelvetiaFavorite;

gint64 helvetia_favorite_add(const char *kind, const char *target, const char *label, GError **error);
gboolean helvetia_favorite_remove(const char *kind, const char *target, GError **error);
GPtrArray *helvetia_favorite_list(const char *kind, GError **error);
gboolean helvetia_favorite_contains(const char *kind, const char *target, GError **error);
void helvetia_favorite_free(HelvetiaFavorite *f);

G_END_DECLS
