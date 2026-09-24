#include "pdf_module.h"
#include "pdf_tools.h"
#include <stddef.h>

static const HelvetiaTool tools_organizing[] = {
    { "merge_pdfs", "PDF Merger", "Merge multiple PDFs into one", "list-add-symbolic", (const char*[]){ "pdfs", "merge", "pdf", NULL }, "merge-pdfs", pdf_merge_create_view, NULL, NULL, pdf_merge_cmds },
    { "split_pdf", "PDF Splitter", "Split PDF into multiple files", "list-remove-symbolic", (const char*[]){ "split", "pdf", NULL }, "split-pdf", pdf_split_create_view, NULL, NULL, pdf_split_cmds },
    { 0 }
};

static const HelvetiaTool tools_optimization[] = {
    { "pdf_compress", "PDF Compressor", "Reduce PDF file size", "application-pdf-symbolic", (const char*[]){ "compress", "pdf", NULL }, "pdf-compress", pdf_compress_create_view, NULL, NULL, pdf_compress_cmds },
    { 0 }
};

static const HelvetiaSubcategory pdf_subcategories[] = {
    { "Organize", tools_organizing },
    { "Optimize", tools_optimization },
    { 0 }
};

static const HelvetiaModule pdf_module = {
    .id            = "pdf",
    .name          = "PDF Tools",
    .icon_name     = "application-pdf-symbolic",
    .description   = "Merge, split, compress, and edit PDFs.",
    .subcategories = pdf_subcategories,
    .create_view   = NULL,
    .on_activate   = NULL,
    .on_deactivate   = NULL,
    .on_shutdown   = NULL,
};

const HelvetiaModule *helvetia_pdf_get_module(void) {
    return &pdf_module;
}
