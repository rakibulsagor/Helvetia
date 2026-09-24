/* builtin.c — registers all statically-linked modules.
 * Add new built-in modules here as the project grows. */
#include "../core/module_registry.h"
#include "image/image_module.h"
#include "pdf/pdf_module.h"
#include "security/security_module.h"

void helvetia_register_builtin_modules(void) {
  helvetia_module_registry_add(helvetia_pdf_get_module());
  helvetia_module_registry_add(helvetia_image_get_module());
  helvetia_module_registry_add(helvetia_security_get_module());
}
