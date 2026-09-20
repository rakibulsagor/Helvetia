#include "db.h"
#include <sodium.h>

gboolean helvetia_vault_init_schema(GError **error) {
    (void)error;
    return TRUE;
}

gboolean helvetia_vault_set_meta(const char *key,
                                 const void *value, gsize len,
                                 GError **error) {
    sqlite3_stmt *stmt = helvetia_db_prepare(
        "INSERT INTO vault_meta (key, value) VALUES (?1, ?2) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;",
        error);
    if (!stmt) return FALSE;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, value, (int)len, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_QUERY,
                    "vault_set_meta failed: %s",
                    sqlite3_errmsg(helvetia_db_handle()));
        return FALSE;
    }
    return TRUE;
}

GBytes *helvetia_vault_get_meta(const char *key, GError **error) {
    sqlite3_stmt *stmt = helvetia_db_prepare(
        "SELECT value FROM vault_meta WHERE key = ?1;", error);
    if (!stmt) return NULL;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

    GBytes *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void *blob = sqlite3_column_blob(stmt, 0);
        int len = sqlite3_column_bytes(stmt, 0);
        if (blob && len > 0)
            result = g_bytes_new(blob, (gsize)len);
    }
    sqlite3_finalize(stmt);
    return result;
}

gint64 helvetia_vault_insert(GBytes *encrypted, GError **error) {
    sqlite3_stmt *stmt = helvetia_db_prepare(
        "INSERT INTO vault (encrypted) VALUES (?1);", error);
    if (!stmt) return -1;

    gsize len = 0;
    const void *data = g_bytes_get_data(encrypted, &len);
    sqlite3_bind_blob(stmt, 1, data, (int)len, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_QUERY,
                    "vault_insert failed: %s",
                    sqlite3_errmsg(helvetia_db_handle()));
        return -1;
    }
    return sqlite3_last_insert_rowid(helvetia_db_handle());
}

GPtrArray *helvetia_vault_list(GError **error) {
    sqlite3_stmt *stmt = helvetia_db_prepare(
        "SELECT id, created_at, updated_at FROM vault ORDER BY id;",
        error);
    if (!stmt) return NULL;

    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)helvetia_vault_entry_free);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HelvetiaVaultEntry *e = g_new0(HelvetiaVaultEntry, 1);
        e->id         = sqlite3_column_int64(stmt, 0);
        e->created_at = sqlite3_column_int64(stmt, 1);
        e->updated_at = sqlite3_column_int64(stmt, 2);
        g_ptr_array_add(result, e);
    }
    sqlite3_finalize(stmt);
    return result;
}

void helvetia_vault_entry_free(HelvetiaVaultEntry *e) {
    if (!e) return;
    g_clear_pointer(&e->encrypted_data, g_bytes_unref);
    g_free(e);
}

/* Store KEK-wrapped DEK in vault_meta. Called once on vault creation. */
gboolean helvetia_vault_create(const char *master_password,
                               GError **error) {
    if (sodium_init() < 0) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_INTERNAL,
                    "libsodium init failed");
        return FALSE;
    }

    /* Generate random salt and DEK */
    unsigned char salt[crypto_pwhash_SALTBYTES];
    unsigned char dek[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    randombytes_buf(salt, sizeof salt);
    randombytes_buf(dek, sizeof dek);

    /* Derive KEK from master password */
    unsigned char kek[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    if (crypto_pwhash(kek, sizeof kek,
                      master_password, strlen(master_password),
                      salt,
                      crypto_pwhash_OPSLIMIT_MODERATE,
                      crypto_pwhash_MEMLIMIT_MODERATE,
                      crypto_pwhash_ALG_ARGON2ID13) != 0) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_INTERNAL,
                    "key derivation failed (out of memory?)");
        return FALSE;
    }

    /* Encrypt DEK with KEK */
    unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    randombytes_buf(nonce, sizeof nonce);

    unsigned char wrapped[crypto_aead_xchacha20poly1305_ietf_KEYBYTES
                         + crypto_aead_xchacha20poly1305_ietf_ABYTES];
    unsigned long long wrapped_len = 0;

    crypto_aead_xchacha20poly1305_ietf_encrypt(
        wrapped, &wrapped_len,
        dek, sizeof dek,
        NULL, 0,          /* no AAD */
        NULL,             /* no secret nonce */
        nonce, kek);

    /* Store salt, nonce, and wrapped DEK */
    helvetia_vault_set_meta("salt",  salt,  sizeof salt,  error);
    helvetia_vault_set_meta("nonce", nonce, sizeof nonce, error);
    helvetia_vault_set_meta("dek",   wrapped, wrapped_len, error);

    /* Wipe secrets */
    sodium_memzero(kek, sizeof kek);
    sodium_memzero(dek, sizeof dek);

    return TRUE;
}

/* Returns a newly allocated 32-byte DEK, or NULL on failure.
   Caller must sodium_memzero() and free when done. */
unsigned char *helvetia_vault_unlock(const char *master_password,
                                     GError **error) {
    GBytes *salt_bytes  = helvetia_vault_get_meta("salt",  error);
    GBytes *nonce_bytes = helvetia_vault_get_meta("nonce", error);
    GBytes *dek_bytes   = helvetia_vault_get_meta("dek",   error);

    if (!salt_bytes || !nonce_bytes || !dek_bytes) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_NOT_FOUND,
                    "vault has not been created yet");
        return NULL;
    }

    gsize salt_len, nonce_len, wrapped_len;
    const unsigned char *salt    = g_bytes_get_data(salt_bytes, &salt_len);
    const unsigned char *nonce   = g_bytes_get_data(nonce_bytes, &nonce_len);
    const unsigned char *wrapped = g_bytes_get_data(dek_bytes, &wrapped_len);

    unsigned char kek[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    if (crypto_pwhash(kek, sizeof kek,
                      master_password, strlen(master_password),
                      salt,
                      crypto_pwhash_OPSLIMIT_MODERATE,
                      crypto_pwhash_MEMLIMIT_MODERATE,
                      crypto_pwhash_ALG_ARGON2ID13) != 0) {
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_INTERNAL,
                    "key derivation failed");
        return NULL;
    }

    unsigned char *dek = (unsigned char *)g_malloc0(
        crypto_aead_xchacha20poly1305_ietf_KEYBYTES);
    unsigned long long dek_len = 0;

    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            dek, &dek_len,
            NULL,
            wrapped, wrapped_len,
            NULL, 0,
            nonce, kek) != 0) {
        sodium_memzero(kek, sizeof kek);
        g_free(dek);
        g_set_error(error, HELVETIA_DB_ERROR,
                    HELVETIA_DB_ERROR_CONSTRAINT,
                    "wrong master password");
        return NULL;
    }

    sodium_memzero(kek, sizeof kek);
    return dek;
}

GBytes *helvetia_vault_encrypt_entry(const unsigned char *dek,
                                     const char *plaintext,
                                     GError **error) {
    (void)error;
    size_t pt_len = strlen(plaintext);

    unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    randombytes_buf(nonce, sizeof nonce);

    size_t ct_len = pt_len + crypto_aead_xchacha20poly1305_ietf_ABYTES;
    unsigned char *ct = (unsigned char *)g_malloc(sizeof nonce + ct_len);
    unsigned long long written = 0;

    memcpy(ct, nonce, sizeof nonce);   /* prepend nonce */
    crypto_aead_xchacha20poly1305_ietf_encrypt(
        ct + sizeof nonce, &written,
        (const unsigned char *)plaintext, pt_len,
        NULL, 0, NULL, nonce, dek);

    return g_bytes_new_take(ct, sizeof nonce + written);
}
