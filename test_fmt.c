#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void format_scientific_unicode(double val, char *buf, size_t sz, int precision) {
    char temp[128];
    snprintf(temp, sizeof temp, "%.*g", precision, val);
    
    char *e = strchr(temp, 'e');
    if (!e) e = strchr(temp, 'E');
    
    if (!e) {
        snprintf(buf, sz, "%s", temp);
        return;
    }
    
    *e = '\0';
    char *base = temp;
    char *exp_str = e + 1;
    
    /* Parse exponent */
    int exp_val = atoi(exp_str);
    
    char exp_uni[64] = {0};
    char exp_chars[16];
    snprintf(exp_chars, sizeof exp_chars, "%d", exp_val);
    
    for (char *c = exp_chars; *c; c++) {
        switch (*c) {
            case '-': strcat(exp_uni, "⁻"); break;
            case '0': strcat(exp_uni, "⁰"); break;
            case '1': strcat(exp_uni, "¹"); break;
            case '2': strcat(exp_uni, "²"); break;
            case '3': strcat(exp_uni, "³"); break;
            case '4': strcat(exp_uni, "⁴"); break;
            case '5': strcat(exp_uni, "⁵"); break;
            case '6': strcat(exp_uni, "⁶"); break;
            case '7': strcat(exp_uni, "⁷"); break;
            case '8': strcat(exp_uni, "⁸"); break;
            case '9': strcat(exp_uni, "⁹"); break;
        }
    }
    
    if (strcmp(base, "1") == 0) {
        snprintf(buf, sz, "10%s", exp_uni);
    } else {
        snprintf(buf, sz, "%s × 10%s", base, exp_uni);
    }
}

int main() {
    char buf[128];
    format_scientific_unicode(0.000001, buf, sizeof buf, 4);
    printf("%s\n", buf);
    format_scientific_unicode(1.5e-6, buf, sizeof buf, 4);
    printf("%s\n", buf);
    format_scientific_unicode(2.0e10, buf, sizeof buf, 4);
    printf("%s\n", buf);
    format_scientific_unicode(100.5, buf, sizeof buf, 4);
    printf("%s\n", buf);
    return 0;
}
