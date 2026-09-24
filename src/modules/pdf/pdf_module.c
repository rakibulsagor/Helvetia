#include "pdf_module.h"
#include "pdf_tools.h"
#include <stddef.h>

static const HelvetiaTool tools_organizing[] = {
    { .id = "merge_pdfs", .name = "PDF Merger", .description = "Merge multiple PDFs into one", .icon_name = "list-add-symbolic", .keywords = (const char*[]){ "pdfs", "merge", "pdf", NULL }, .cli_command = "merge-pdfs", .create_view = pdf_merge_create_view, .commands = pdf_merge_cmds },
    { .id = "split_pdf", .name = "PDF Splitter", .description = "Split PDF into multiple files", .icon_name = "list-remove-symbolic", .keywords = (const char*[]){ "split", "pdf", NULL }, .cli_command = "split-pdf", .create_view = pdf_split_create_view, .commands = pdf_split_cmds },
    { 0 }
};

static const HelvetiaTool tools_optimization[] = {
    { .id = "pdf_compress", .name = "PDF Compressor", .description = "Reduce PDF file size", .icon_name = "application-pdf-symbolic", .keywords = (const char*[]){ "compress", "pdf", NULL }, .cli_command = "pdf-compress", .create_view = pdf_compress_create_view, .commands = pdf_compress_cmds },
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
