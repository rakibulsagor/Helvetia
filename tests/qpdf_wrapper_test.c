/*
 * qpdf_wrapper_test.c — standalone test for the QPDF C wrapper.
 *
 * Usage:
 *   ./qpdf_wrapper_test merge   a.pdf b.pdf out.pdf
 *   ./qpdf_wrapper_test split   in.pdf out_dir "1-5, 8"
 *   ./qpdf_wrapper_test rotate  in.pdf out.pdf 90 "1-3"
 *   ./qpdf_wrapper_test compress in.pdf out.pdf ebook
 *   ./qpdf_wrapper_test encrypt in.pdf out.pdf userpass ownerpass
 *   ./qpdf_wrapper_test decrypt in.pdf out.pdf userpass
 *   ./qpdf_wrapper_test pages   in.pdf
 *   ./qpdf_wrapper_test meta    in.pdf
 *
 * Exit code is 0 on success, 1 on failure.
 */

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include "../src/modules/pdf/backend/qpdf_wrapper.h"

static int usage(const char *argv0) {
    fprintf(stderr,
        "Usage:\n"
        "  %s merge    <in1.pdf> <in2.pdf> [in3.pdf...] <out.pdf>\n"
        "  %s split    <in.pdf> <out_dir> [ranges]\n"
        "  %s rotate   <in.pdf> <out.pdf> <angle> [pages]\n"
        "  %s compress <in.pdf> <out.pdf> <quality>\n"
        "  %s encrypt  <in.pdf> <out.pdf> <user_pass> [owner_pass]\n"
        "  %s decrypt  <in.pdf> <out.pdf> <password>\n"
        "  %s pages    <in.pdf>\n"
        "  %s meta     <in.pdf>\n"
        "\n"
        "  quality is one of: screen, ebook, printer, prepress\n",
        argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0);
    return 2;
}

static void print_error(GError *error) {
    if (!error) return;
    fprintf(stderr, "ERROR [%s]: %s\n",
            g_quark_to_string(error->domain),
            error->message);
}

int main(int argc, char **argv) {
    if (argc < 3) return usage(argv[0]);

    GError *error = NULL;
    const char *cmd = argv[1];

    /* ---- merge ---- */
    if (strcmp(cmd, "merge") == 0) {
        if (argc < 5) return usage(argv[0]);
        const char *out = argv[argc - 1];
        guint n = (guint)(argc - 3);
        const char **inputs = g_new0(const char *, n + 1);
        for (guint i = 0; i < n; ++i) inputs[i] = argv[2 + i];

        gboolean ok = qpdf_merge_files(inputs, n, out, &error);
        g_free(inputs);
        if (!ok) { print_error(error); g_clear_error(&error); return 1; }
        printf("OK: merged %u files into %s\n", n, out);
        return 0;
    }

    /* ---- split ---- */
    if (strcmp(cmd, "split") == 0) {
        if (argc < 4) return usage(argv[0]);
        const char *in  = argv[2];
        const char *dir = argv[3];
        const char *ranges = (argc >= 5) ? argv[4] : "";

        gboolean ok = qpdf_split_file(in, dir, ranges, &error);
        if (!ok) { print_error(error); g_clear_error(&error); return 1; }
        printf("OK: split %s into %s\n", in, dir);
        return 0;
    }

    /* ---- rotate ---- */
    if (strcmp(cmd, "rotate") == 0) {
        if (argc < 5) return usage(argv[0]);
        const char *in    = argv[2];
        const char *out   = argv[3];
        int         angle = atoi(argv[4]);
        const char *pages = (argc >= 6) ? argv[5] : "";

        gboolean ok = qpdf_rotate_pages(in, out, angle, pages, &error);
        if (!ok) { print_error(error); g_clear_error(&error); return 1; }
        printf("OK: rotated %d degrees\n", angle);
        return 0;
    }

    /* ---- compress ---- */
    if (strcmp(cmd, "compress") == 0) {
        if (argc < 5) return usage(argv[0]);
        const char *in      = argv[2];
        const char *out     = argv[3];
        const char *quality = argv[4];

        gboolean ok = qpdf_compress_file(in, out, quality, TRUE, &error);
        if (!ok) { print_error(error); g_clear_error(&error); return 1; }
        printf("OK: compressed with quality=%s\n", quality);
        return 0;
    }

    /* ---- encrypt ---- */
    if (strcmp(cmd, "encrypt") == 0) {
        if (argc < 5) return usage(argv[0]);
        const char *in    = argv[2];
        const char *out   = argv[3];
        const char *user  = argv[4];
        const char *owner = (argc >= 6) ? argv[5] : NULL;

        gboolean ok = qpdf_encrypt_file(in, out, user, owner, &error);
        if (!ok) { print_error(error); g_clear_error(&error); return 1; }
        printf("OK: encrypted to %s\n", out);
        return 0;
    }

    /* ---- decrypt ---- */
    if (strcmp(cmd, "decrypt") == 0) {
        if (argc < 5) return usage(argv[0]);
        const char *in   = argv[2];
        const char *out  = argv[3];
        const char *pass = argv[4];

        gboolean ok = qpdf_decrypt_file(in, out, pass, &error);
        if (!ok) { print_error(error); g_clear_error(&error); return 1; }
        printf("OK: decrypted to %s\n", out);
        return 0;
    }

    /* ---- pages ---- */
    if (strcmp(cmd, "pages") == 0) {
        if (argc < 3) return usage(argv[0]);
        gint64 n = qpdf_get_page_count(argv[2], &error);
        if (n < 0) { print_error(error); g_clear_error(&error); return 1; }
        printf("%" G_GINT64_FORMAT "\n", n);
        return 0;
    }

    /* ---- meta ---- */
    if (strcmp(cmd, "meta") == 0) {
        if (argc < 3) return usage(argv[0]);
        GVariant *meta = qpdf_get_metadata(argv[2], &error);
        if (!meta) { print_error(error); g_clear_error(&error); return 1; }

        GVariantIter iter;
        const char *key, *value;
        g_variant_iter_init(&iter, meta);
        while (g_variant_iter_next(&iter, "{&s&s}", &key, &value)) {
            printf("%s = %s\n", key, value);
        }
        g_variant_unref(meta);
        return 0;
    }

    return usage(argv[0]);
}
