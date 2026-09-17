/* builtin.c — registers all statically-linked modules.
 * Add new built-in modules here as the project grows. */
#include "../core/module_registry.h"
#include "utility/utility_module.h"
#include "pdf/pdf_module.h"
#include "images/images_module.h"
#include "media/media_module.h"
#include "documents/documents_module.h"
#include "convert/convert_module.h"
#include "security/security_module.h"
#include "devtools/devtools_module.h"

void helvetia_register_builtin_modules(void) {
    helvetia_module_registry_add(helvetia_utility_get_module());
    helvetia_module_registry_add(helvetia_pdf_get_module());
    helvetia_module_registry_add(helvetia_images_get_module());
    helvetia_module_registry_add(helvetia_media_get_module());
    helvetia_module_registry_add(helvetia_documents_get_module());
    helvetia_module_registry_add(helvetia_convert_get_module());
    helvetia_module_registry_add(helvetia_security_get_module());
    helvetia_module_registry_add(helvetia_devtools_get_module());
}
