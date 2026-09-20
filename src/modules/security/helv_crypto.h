#ifndef HELV_CRYPTO_H
#define HELV_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define HELV_MAGIC "HELV"
#define HELV_MAGIC_LEN 4
#define HELV_SALT_LEN 16
#define HELV_NONCE_LEN 12
#define HELV_TAG_LEN 16
#define HELV_DEFAULT_CHUNK_SIZE 65536

typedef enum {
    DECRYPT_OK = 0,
    DECRYPT_ERR_OPEN,
    DECRYPT_ERR_MAGIC,
    DECRYPT_ERR_VERSION,
    DECRYPT_ERR_CONTAINER,
    DECRYPT_ERR_ALGORITHM,
    DECRYPT_ERR_KDF,
    DECRYPT_ERR_PARAMS,
    DECRYPT_ERR_TRUNCATED,
    DECRYPT_ERR_AUTH,
    DECRYPT_ERR_KEYFILE,
    DECRYPT_ERR_WRITE,
    DECRYPT_ERR_MEMORY,
    DECRYPT_ERR_ARCHIVE,
} helv_crypto_result_t;

// Container type
#define HELV_CONTAINER_FILE 0x01
#define HELV_CONTAINER_FOLDER 0x02

// Compression methods
#define HELV_COMP_NONE 0x00
#define HELV_COMP_GZIP 0x01
#define HELV_COMP_XZ   0x02
#define HELV_COMP_ZSTD 0x03

typedef struct {
    uint8_t  version;         // 1 byte
    uint8_t  container_type;  // 1 byte
    uint8_t  algorithm;       // 1 byte
    uint8_t  kdf;             // 1 byte
    uint32_t memory_cost;     // 4 bytes
    uint32_t time_cost;       // 4 bytes
    uint32_t parallelism;     // 4 bytes
    uint8_t  salt[HELV_SALT_LEN];   // 16 bytes
    uint8_t  nonce[HELV_NONCE_LEN]; // 12 bytes
    uint16_t filename_len;    // 2 bytes
    char    *filename;        // filename_len bytes
    uint64_t total_file_count; // 8 bytes
    uint64_t original_size;   // 8 bytes
    uint32_t flags;           // 4 bytes
    uint8_t  compression;     // 1 byte
    uint32_t chunk_size;      // 4 bytes

    // Internal parsing properties
    size_t   header_len;
    uint8_t *header_bytes;
} helv_header_t;

typedef struct {
    bool include_hidden;
    bool preserve_permissions;
    bool preserve_ownership;
    bool preserve_timestamps;
    bool follow_symlinks;
    bool shred_original;
    uint8_t compression;
    uint32_t chunk_size;
} helv_folder_options_t;

const char *helv_crypto_error_msg(helv_crypto_result_t rc);

void helv_header_free(helv_header_t *h);

helv_crypto_result_t helv_read_header(FILE *in, helv_header_t *h);

// File encryption/decryption (Container 0x01)
helv_crypto_result_t helv_decrypt_file(const char *in_path,
                                       const char *out_path,
                                       const char *password,
                                       const uint8_t *keyfile, size_t keyfile_len,
                                       void (*progress_cb)(double, void*), void *progress_data);

helv_crypto_result_t helv_encrypt_file(const char *in_path,
                                       const char *out_path,
                                       const char *password,
                                       const uint8_t *keyfile, size_t keyfile_len,
                                       uint8_t alg_id,
                                       void (*progress_cb)(double, void*), void *progress_data);

// Folder encryption/decryption (Container 0x02)
helv_crypto_result_t helv_encrypt_folder(const char *in_folder_path,
                                         const char *out_path,
                                         const char *password,
                                         const uint8_t *keyfile, size_t keyfile_len,
                                         uint8_t alg_id,
                                         const helv_folder_options_t *options,
                                         void (*progress_cb)(double, void*), void *progress_data);

helv_crypto_result_t helv_decrypt_folder(const char *in_path,
                                         const char *out_folder_path,
                                         const char *password,
                                         const uint8_t *keyfile, size_t keyfile_len,
                                         const helv_folder_options_t *options,
                                         void (*progress_cb)(double, void*), void *progress_data);

#endif
