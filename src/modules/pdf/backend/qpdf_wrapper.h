#pragma once

/*
 * qpdf_wrapper.h — C API for the QPDF C++ backend.
 *
 * Every function:
 *   - Returns TRUE on success, FALSE on failure.
 *   - On failure, sets *error (if non-NULL) with a GQuark domain
 *     of HELVETIA_QPDF_ERROR and a human-readable message.
 *   - Never throws a C++ exception across the boundary.
 *
 * All strings are UTF-8. All paths are absolute or relative to CWD.
 */

#include <glib.h>

G_BEGIN_DECLS

/* ------------------------------------------------------------------ */
/* Error domain                                                       */
/* ------------------------------------------------------------------ */

#define HELVETIA_QPDF_ERROR (helvetia_qpdf_error_quark())

typedef enum {
    HELVETIA_QPDF_ERROR_OPEN,           /* cannot open input */
    HELVETIA_QPDF_ERROR_WRITE,          /* cannot write output */
    HELVETIA_QPDF_ERROR_ENCRYPTED,      /* input requires a password */
    HELVETIA_QPDF_ERROR_WRONG_PASSWORD, /* password is incorrect */
    HELVETIA_QPDF_ERROR_INVALID_RANGE,  /* page range malformed */
    HELVETIA_QPDF_ERROR_INVALID_PAGE,   /* page number out of bounds */
    HELVETIA_QPDF_ERROR_CORRUPT,        /* input PDF is damaged */
    HELVETIA_QPDF_ERROR_UNSUPPORTED,    /* feature not supported by qpdf */
    HELVETIA_QPDF_ERROR_IO,             /* generic I/O failure */
    HELVETIA_QPDF_ERROR_INTERNAL,       /* unexpected internal error */
} HelvetiaQpdfError;

GQuark helvetia_qpdf_error_quark(void);

/* ------------------------------------------------------------------ */
/* Organizing                                                         */
/* ------------------------------------------------------------------ */

/*
 * Merge multiple PDFs into one output file.
 *
 *   inputs     — array of NUL-terminated UTF-8 paths
 *   n_inputs   — number of entries in `inputs` (must be >= 1)
 *   output     — path to the merged output
 *   error      — set on failure
 *
 * Pages are appended in the order of `inputs`.
 */
gboolean qpdf_merge_files(const char *const *inputs,
                          guint              n_inputs,
                          const char        *output,
                          GError           **error);

/*
 * Split a PDF into one output per range.
 *
 *   input      — source PDF
 *   output_dir — directory where outputs are written (must exist)
 *   ranges     — e.g. "1-5, 8, 12-15". Empty string = one file per page.
 *   error      — set on failure
 *
 * Output files are named "<stem>_<range>.pdf", e.g. "doc_1-5.pdf".
 * If `ranges` is empty, outputs are "doc_page_0001.pdf", etc.
 */
gboolean qpdf_split_file(const char *input,
                         const char *output_dir,
                         const char *ranges,
                         GError    **error);

/*
 * Split every page of a PDF into its own file.
 * Convenience wrapper around qpdf_split_file() with an empty range.
 */
gboolean qpdf_split_all_pages(const char *input,
                              const char *output_dir,
                              GError    **error);

/*
 * Rotate pages in a PDF and write the result to `output`.
 *
 *   angle  — must be a multiple of 90 (positive = clockwise)
 *   pages  — page range string, e.g. "1-3, 7". Empty = all pages.
 */
gboolean qpdf_rotate_pages(const char *input,
                           const char *output,
                           int         angle,
                           const char *pages,
                           GError    **error);

/* ------------------------------------------------------------------ */
/* Optimization                                                       */
/* ------------------------------------------------------------------ */

/*
 * Compress a PDF using Ghostscript-backed optimization levels.
 *
 *   quality — one of "screen", "ebook", "printer", "prepress"
 *             (falls back to "ebook" if unknown)
 *   preserve_metadata — if TRUE, keeps document info and XMP
 *
 * This calls QPDF's built-in compression plus stream re-compression.
 * For full Ghostscript-based recompression, a later version will
 * shell out to `gs`; for now, QPDF's optimize() handles the common
 * cases (linearization, stream compression, object deduplication).
 */
gboolean qpdf_compress_file(const char *input,
                            const char *output,
                            const char *quality,
                            gboolean    preserve_metadata,
                            GError    **error);

/* ------------------------------------------------------------------ */
/* Security                                                           */
/* ------------------------------------------------------------------ */

/*
 * Encrypt a PDF with AES-256.
 *
 *   user_password  — required to open the file
 *   owner_password — required to change permissions
 *                    (if NULL, user_password is used)
 *
 * Output is always 256-bit AES. qpdf's R6 encryption handler is used.
 */
gboolean qpdf_encrypt_file(const char *input,
                           const char *output,
                           const char *user_password,
                           const char *owner_password,
                           GError    **error);

/*
 * Remove encryption from a PDF. `password` may be NULL if the file
 * uses an empty user password.
 */
gboolean qpdf_decrypt_file(const char *input,
                           const char *output,
                           const char *password,
                           GError    **error);

/* ------------------------------------------------------------------ */
/* Metadata                                                           */
/* ------------------------------------------------------------------ */

/*
 * Read the number of pages in a PDF. On failure, returns -1 and
 * sets *error.
 */
gint64 qpdf_get_page_count(const char *input, GError **error);

/*
 * Extract the document info dictionary as a variant dictionary of
 * strings. Keys are the standard PDF info keys (Title, Author,
 * Subject, Keywords, Creator, Producer, CreationDate, ModDate).
 * Missing keys are simply absent. Caller owns the returned GVariant
 * and must g_variant_unref() it.
 */
GVariant *qpdf_get_metadata(const char *input, GError **error);

G_END_DECLS
