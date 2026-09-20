/*
 * qpdf_wrapper.cpp — C++ implementation of the QPDF backend.
 *
 * Design notes:
 *   - Every public function is wrapped in try/catch. C++ exceptions
 *     never escape into C.
 *   - Errors are reported through GError with a stable error domain.
 *   - File paths are passed to qpdf as std::string; qpdf handles
 *     UTF-8 paths on Linux natively.
 *   - We use QPDF's C++ API directly rather than the C API because
 *     the C API lags behind and lacks some features (e.g. JSON
 *     metadata extraction).
 */

#include "qpdf_wrapper.h"

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageLabelDocumentHelper.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QUtil.hh>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <memory>

/* ------------------------------------------------------------------ */
/* Error domain                                                       */
/* ------------------------------------------------------------------ */

GQuark helvetia_qpdf_error_quark(void) {
    return g_quark_from_static_string("helvetia-qpdf-error");
}

/* ------------------------------------------------------------------ */
/* Internal helpers                                                   */
/* ------------------------------------------------------------------ */

namespace {

/* Map a qpdf exception to a HelvetiaQpdfError code. */
HelvetiaQpdfError classify(const QPDFExc &e) {
    const std::string msg = e.what();

    /* qpdf doesn't expose error codes, so we pattern-match the message.
       This is not ideal but it's what the library gives us. */
    if (msg.find("invalid password") != std::string::npos ||
        msg.find("password") != std::string::npos)
        return HELVETIA_QPDF_ERROR_WRONG_PASSWORD;

    if (msg.find("is encrypted") != std::string::npos ||
        msg.find("encrypted") != std::string::npos)
        return HELVETIA_QPDF_ERROR_ENCRYPTED;

    if (msg.find("open") != std::string::npos ||
        msg.find("No such file") != std::string::npos)
        return HELVETIA_QPDF_ERROR_OPEN;

    if (msg.find("write") != std::string::npos ||
        msg.find("permission") != std::string::npos)
        return HELVETIA_QPDF_ERROR_WRITE;

    if (msg.find("range") != std::string::npos ||
        msg.find("page") != std::string::npos)
        return HELVETIA_QPDF_ERROR_INVALID_RANGE;

    if (msg.find("damaged") != std::string::npos ||
        msg.find("corrupt") != std::string::npos)
        return HELVETIA_QPDF_ERROR_CORRUPT;

    return HELVETIA_QPDF_ERROR_INTERNAL;
}

/* Set a GError from a qpdf exception. */
void set_error_from_qpdf(GError **error, const QPDFExc &e) {
    if (!error) return;
    g_set_error(error,
                HELVETIA_QPDF_ERROR,
                classify(e),
                "qpdf: %s", e.what());
}

/* Set a GError from a std::exception. */
void set_error_from_std(GError **error, const std::exception &e) {
    if (!error) return;
    g_set_error(error,
                HELVETIA_QPDF_ERROR,
                HELVETIA_QPDF_ERROR_INTERNAL,
                "internal error: %s", e.what());
}

/* Load a PDF from disk, honoring an optional password. */
bool load_pdf(QPDF &pdf, const char *path, const char *password,
              GError **error) {
    try {
        pdf.processFile(path, password ? password : "");
        return true;
    } catch (const QPDFExc &e) {
        set_error_from_qpdf(error, e);
        return false;
    } catch (const std::exception &e) {
        set_error_from_std(error, e);
        return false;
    }
}

/* Parse a page range string into a sorted, deduplicated vector of
   zero-based page indices. Supports "1-5, 8, 12-15". An empty string
   means "all pages". */
bool parse_ranges(const std::string &ranges, int page_count,
                  std::vector<int> &out, GError **error) {
    out.clear();
    if (ranges.empty()) {
        for (int i = 0; i < page_count; ++i) out.push_back(i);
        return true;
    }

    size_t pos = 0;
    while (pos < ranges.size()) {
        /* Skip whitespace and commas */
        while (pos < ranges.size() &&
               (ranges[pos] == ' ' || ranges[pos] == '\t' || ranges[pos] == ','))
            ++pos;
        if (pos >= ranges.size()) break;

        /* Parse start number */
        char *end = nullptr;
        long start = std::strtol(ranges.c_str() + pos, &end, 10);
        if (end == ranges.c_str() + pos) {
            g_set_error(error, HELVETIA_QPDF_ERROR,
                        HELVETIA_QPDF_ERROR_INVALID_RANGE,
                        "invalid range near position %zu", pos);
            return false;
        }
        pos = end - ranges.c_str();

        long finish = start;

        /* Check for '-' */
        while (pos < ranges.size() && (ranges[pos] == ' ' || ranges[pos] == '\t'))
            ++pos;
        if (pos < ranges.size() && ranges[pos] == '-') {
            ++pos;
            while (pos < ranges.size() && (ranges[pos] == ' ' || ranges[pos] == '\t'))
                ++pos;
            long f = std::strtol(ranges.c_str() + pos, &end, 10);
            if (end == ranges.c_str() + pos) {
                g_set_error(error, HELVETIA_QPDF_ERROR,
                            HELVETIA_QPDF_ERROR_INVALID_RANGE,
                            "invalid range end near position %zu", pos);
                return false;
            }
            finish = f;
            pos = end - ranges.c_str();
        }

        if (start < 1 || finish < 1) {
            g_set_error(error, HELVETIA_QPDF_ERROR,
                        HELVETIA_QPDF_ERROR_INVALID_RANGE,
                        "page numbers must be >= 1");
            return false;
        }
        if (start > page_count || finish > page_count) {
            g_set_error(error, HELVETIA_QPDF_ERROR,
                        HELVETIA_QPDF_ERROR_INVALID_PAGE,
                        "page %ld out of range (document has %d pages)",
                        (start > page_count) ? start : finish, page_count);
            return false;
        }

        int lo = (int)((start < finish) ? start : finish) - 1;
        int hi = (int)((start < finish) ? finish : start) - 1;
        for (int i = lo; i <= hi; ++i) out.push_back(i);
    }

    /* Deduplicate while preserving order */
    std::vector<int> seen;
    seen.reserve(out.size());
    for (int v : out) {
        bool dup = false;
        for (int s : seen) {
            if (s == v) { dup = true; break; }
        }
        if (!dup) seen.push_back(v);
    }
    out.swap(seen);
    return true;
}

/* Build an output filename from the input stem and range descriptor. */
std::string make_split_name(const std::string &input_path,
                            const std::string &range_desc) {
    /* Extract basename without extension */
    size_t slash = input_path.find_last_of('/');
    std::string base = (slash == std::string::npos)
                       ? input_path
                       : input_path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);

    std::string name = base;
    if (!range_desc.empty()) {
        name += "_";
        name += range_desc;
    } else {
        name += "_pages";
    }
    name += ".pdf";
    return name;
}

/* Choose QPDFWriter compression level from a quality string. */
int quality_to_level(const char *quality) {
    if (!quality) return 2;
    std::string q = quality;
    for (auto &c : q) c = (char)std::tolower((unsigned char)c);
    if (q == "screen")   return 1;  /* fastest, smallest */
    if (q == "ebook")    return 2;  /* balanced */
    if (q == "printer")  return 3;  /* higher quality */
    if (q == "prepress") return 4;  /* maximum quality */
    return 2;
}

} /* anonymous namespace */

/* ------------------------------------------------------------------ */
/* Merge                                                              */
/* ------------------------------------------------------------------ */

extern "C" gboolean qpdf_merge_files(const char *const *inputs,
                                     guint              n_inputs,
                                     const char        *output,
                                     GError           **error) {
    if (!inputs || n_inputs == 0 || !output) {
        g_set_error(error, HELVETIA_QPDF_ERROR,
                    HELVETIA_QPDF_ERROR_INTERNAL,
                    "merge: missing arguments");
        return FALSE;
    }

    try {
        QPDF out;
        out.emptyPDF();

        for (guint i = 0; i < n_inputs; ++i) {
            QPDF in;
            if (!load_pdf(in, inputs[i], nullptr, error))
                return FALSE;

            QPDFPageDocumentHelper in_helper(in);
            std::vector<QPDFPageObjectHelper> pages = in_helper.getAllPages();

            for (auto &page : pages) {
                out.addPage(page, false);
            }
        }

        QPDFWriter writer(out, output);
        writer.setQDFMode(false);
        writer.setLinearization(true);
        writer.setCompressStreams(true);
        writer.write();

        return TRUE;
    } catch (const QPDFExc &e) {
        set_error_from_qpdf(error, e);
        return FALSE;
    } catch (const std::exception &e) {
        set_error_from_std(error, e);
        return FALSE;
    }
}

/* ------------------------------------------------------------------ */
/* Split                                                              */
/* ------------------------------------------------------------------ */

extern "C" gboolean qpdf_split_file(const char *input,
                                    const char *output_dir,
                                    const char *ranges,
                                    GError    **error) {
    if (!input || !output_dir) {
        g_set_error(error, HELVETIA_QPDF_ERROR,
                    HELVETIA_QPDF_ERROR_INTERNAL,
                    "split: missing arguments");
        return FALSE;
    }

    try {
        QPDF in;
        if (!load_pdf(in, input, nullptr, error))
            return FALSE;

        QPDFPageDocumentHelper helper(in);
        std::vector<QPDFPageObjectHelper> all_pages = helper.getAllPages();
        int page_count = (int)all_pages.size();

        std::vector<int> pages;
        if (!parse_ranges(ranges ? ranges : "", page_count, pages, error))
            return FALSE;

        if (pages.empty()) {
            g_set_error(error, HELVETIA_QPDF_ERROR,
                        HELVETIA_QPDF_ERROR_INVALID_RANGE,
                        "split: no pages selected");
            return FALSE;
        }

        /* If a single range was given, treat it as one output.
           Otherwise, split each selected page into its own file. */
        bool single_output = (ranges && *ranges && pages.size() > 1);

        if (single_output) {
            /* Single range like "1-5" */
            QPDF out;
            out.emptyPDF();
            for (int idx : pages) {
                out.addPage(all_pages[idx], false);
            }

            std::string range_desc(ranges);
            /* Remove spaces for the filename */
            std::string clean;
            for (char c : range_desc) {
                if (c != ' ' && c != '\t') clean += c;
            }
            std::string name = make_split_name(input, clean);
            std::string path = std::string(output_dir) + "/" + name;

            QPDFWriter writer(out, path.c_str());
            writer.setCompressStreams(true);
            writer.write();
        } else {
            /* One file per page */
            for (int idx : pages) {
                QPDF out;
                out.emptyPDF();
                out.addPage(all_pages[idx], false);

                char buf[32];
                std::snprintf(buf, sizeof buf, "%04d", idx + 1);
                std::string name = make_split_name(input, buf);
                std::string path = std::string(output_dir) + "/" + name;

                QPDFWriter writer(out, path.c_str());
                writer.setCompressStreams(true);
                writer.write();
            }
        }

        return TRUE;
    } catch (const QPDFExc &e) {
        set_error_from_qpdf(error, e);
        return FALSE;
    } catch (const std::exception &e) {
        set_error_from_std(error, e);
        return FALSE;
    }
}

extern "C" gboolean qpdf_split_all_pages(const char *input,
                                         const char *output_dir,
                                         GError    **error) {
    return qpdf_split_file(input, output_dir, "", error);
}

/* ------------------------------------------------------------------ */
/* Rotate                                                             */
/* ------------------------------------------------------------------ */

extern "C" gboolean qpdf_rotate_pages(const char *input,
                                      const char *output,
                                      int         angle,
                                      const char *pages,
                                      GError    **error) {
    if (!input || !output) {
        g_set_error(error, HELVETIA_QPDF_ERROR,
                    HELVETIA_QPDF_ERROR_INTERNAL,
                    "rotate: missing arguments");
        return FALSE;
    }
    if (angle % 90 != 0) {
        g_set_error(error, HELVETIA_QPDF_ERROR,
                    HELVETIA_QPDF_ERROR_INVALID_RANGE,
                    "rotate: angle must be a multiple of 90");
        return FALSE;
    }

    try {
        QPDF in;
        if (!load_pdf(in, input, nullptr, error))
            return FALSE;

        QPDFPageDocumentHelper helper(in);
        std::vector<QPDFPageObjectHelper> all_pages = helper.getAllPages();
        int page_count = (int)all_pages.size();

        std::vector<int> selected;
        if (!parse_ranges(pages ? pages : "", page_count, selected, error))
            return FALSE;

        /* Normalize angle to 0/90/180/270 */
        int norm = ((angle % 360) + 360) % 360;

        for (int idx : selected) {
            QPDFObjectHandle page = all_pages[idx].getObjectHandle();
            QPDFObjectHandle rotate = page.getKey("/Rotate");
            int current = rotate.isInteger() ? (int)rotate.getIntValue() : 0;
            int next = ((current + norm) % 360 + 360) % 360;
            page.replaceKey("/Rotate", QPDFObjectHandle::newInteger(next));
        }

        QPDFWriter writer(in, output);
        writer.setCompressStreams(true);
        writer.write();
        return TRUE;
    } catch (const QPDFExc &e) {
        set_error_from_qpdf(error, e);
        return FALSE;
    } catch (const std::exception &e) {
        set_error_from_std(error, e);
        return FALSE;
    }
}

/* ------------------------------------------------------------------ */
/* Compress                                                           */
/* ------------------------------------------------------------------ */

extern "C" gboolean qpdf_compress_file(const char *input,
                                       const char *output,
                                       const char *quality,
                                       gboolean    preserve_metadata,
                                       GError    **error) {
    if (!input || !output) {
        g_set_error(error, HELVETIA_QPDF_ERROR,
                    HELVETIA_QPDF_ERROR_INTERNAL,
                    "compress: missing arguments");
        return FALSE;
    }

    try {
        QPDF in;
        if (!load_pdf(in, input, nullptr, error))
            return FALSE;

        int level = quality_to_level(quality);

        QPDFWriter writer(in, output);
        writer.setCompressStreams(true);
        writer.setLinearization(true);
        writer.setObjectStreamMode(qpdf_o_preserve);

        /* Compression level: 1=fast, 9=best. Map quality → level.
           QPDF 12 removes setCompressionLevel from QPDFWriter.
           The default flate compression level is optimal. */

        /* Metadata: qpdf always preserves unless we remove it. */
        if (!preserve_metadata) {
            QPDFObjectHandle trailer = in.getTrailer();
            trailer.removeKey("/Info");
            QPDFObjectHandle root = in.getRoot();
            if (root.hasKey("/Metadata"))
                root.removeKey("/Metadata");
        }

        writer.write();
        return TRUE;
    } catch (const QPDFExc &e) {
        set_error_from_qpdf(error, e);
        return FALSE;
    } catch (const std::exception &e) {
        set_error_from_std(error, e);
        return FALSE;
    }
}

/* ------------------------------------------------------------------ */
/* Encrypt / Decrypt                                                  */
/* ------------------------------------------------------------------ */

extern "C" gboolean qpdf_encrypt_file(const char *input,
                                      const char *output,
                                      const char *user_password,
                                      const char *owner_password,
                                      GError    **error) {
    if (!input || !output || !user_password) {
        g_set_error(error, HELVETIA_QPDF_ERROR,
                    HELVETIA_QPDF_ERROR_INTERNAL,
                    "encrypt: missing arguments");
        return FALSE;
    }

    try {
        QPDF in;
        if (!load_pdf(in, input, nullptr, error))
            return FALSE;

        QPDFWriter writer(in, output);
        writer.setCompressStreams(true);
        writer.setR6EncryptionParameters(
            user_password,
            owner_password ? owner_password : user_password,
            /* allow accessibility */      true,
            /* allow extracting */         false,
            /* allow assembling */         false,
            /* allow annotating */         false,
            /* allow form filling */       false,
            /* allow modifying */          false,
            /* print */                    qpdf_r3p_full,
            /* encrypt metadata */         true);
        writer.write();
        return TRUE;
    } catch (const QPDFExc &e) {
        set_error_from_qpdf(error, e);
        return FALSE;
    } catch (const std::exception &e) {
        set_error_from_std(error, e);
        return FALSE;
    }
}

extern "C" gboolean qpdf_decrypt_file(const char *input,
                                      const char *output,
                                      const char *password,
                                      GError    **error) {
    if (!input || !output) {
        g_set_error(error, HELVETIA_QPDF_ERROR,
                    HELVETIA_QPDF_ERROR_INTERNAL,
                    "decrypt: missing arguments");
        return FALSE;
    }

    try {
        QPDF in;
        if (!load_pdf(in, input, password ? password : "", error))
            return FALSE;

        if (!in.isEncrypted()) {
            g_set_error(error, HELVETIA_QPDF_ERROR,
                        HELVETIA_QPDF_ERROR_INTERNAL,
                        "decrypt: input is not encrypted");
            return FALSE;
        }

        QPDFWriter writer(in, output);
        writer.setCompressStreams(true);
        writer.setPreserveEncryption(false);
        writer.write();
        return TRUE;
    } catch (const QPDFExc &e) {
        set_error_from_qpdf(error, e);
        return FALSE;
    } catch (const std::exception &e) {
        set_error_from_std(error, e);
        return FALSE;
    }
}

/* ------------------------------------------------------------------ */
/* Metadata                                                           */
/* ------------------------------------------------------------------ */

extern "C" gint64 qpdf_get_page_count(const char *input, GError **error) {
    if (!input) {
        g_set_error(error, HELVETIA_QPDF_ERROR,
                    HELVETIA_QPDF_ERROR_INTERNAL,
                    "page count: missing input");
        return -1;
    }
    try {
        QPDF in;
        if (!load_pdf(in, input, nullptr, error))
            return -1;
        QPDFPageDocumentHelper helper(in);
        return (gint64)helper.getAllPages().size();
    } catch (const QPDFExc &e) {
        set_error_from_qpdf(error, e);
        return -1;
    } catch (const std::exception &e) {
        set_error_from_std(error, e);
        return -1;
    }
}

extern "C" GVariant *qpdf_get_metadata(const char *input, GError **error) {
    if (!input) {
        g_set_error(error, HELVETIA_QPDF_ERROR,
                    HELVETIA_QPDF_ERROR_INTERNAL,
                    "metadata: missing input");
        return nullptr;
    }

    try {
        QPDF in;
        if (!load_pdf(in, input, nullptr, error))
            return nullptr;

        QPDFObjectHandle trailer = in.getTrailer();
        QPDFObjectHandle info = trailer.getKey("/Info");

        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{ss}"));

        if (info.isDictionary()) {
            for (auto const &key : info.getKeys()) {
                QPDFObjectHandle value = info.getKey(key);
                if (value.isString()) {
                    std::string k = key;
                    /* Strip leading slash */
                    if (!k.empty() && k[0] == '/') k = k.substr(1);
                    g_variant_builder_add(&builder, "{ss}",
                                          k.c_str(),
                                          value.getUTF8Value().c_str());
                }
            }
        }

        return g_variant_builder_end(&builder);
    } catch (const QPDFExc &e) {
        set_error_from_qpdf(error, e);
        return nullptr;
    } catch (const std::exception &e) {
        set_error_from_std(error, e);
        return nullptr;
    }
}
