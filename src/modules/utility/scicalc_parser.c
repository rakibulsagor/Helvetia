/*
 * scicalc_parser.c - Scientific calculator (expression based)
 */
#include "scicalc_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

/* ------------------------------------------------------------------ */
/* state                                                               */
/* ------------------------------------------------------------------ */

static const char *p;              /* parser cursor            */
jmp_buf scicalc_err_jmp;
char scicalc_err_msg[160];

static int  angle_mode = 0;        /* 0 = deg, 1 = rad, 2 = grad */
static double ans = 0.0;
static double mem = 0.0;

void scicalc_set_angle_mode(int mode) {
    angle_mode = mode;
}

typedef struct { char name[32]; double val; } Var;
static Var vars[128];
static int nvars = 0;

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static void fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(scicalc_err_msg, sizeof scicalc_err_msg, fmt, ap);
    va_end(ap);
    longjmp(scicalc_err_jmp, 1);
}

static double to_rad(double x)
{
    if (angle_mode == 0) return x * M_PI / 180.0;
    if (angle_mode == 2) return x * M_PI / 200.0;
    return x;
}

static double from_rad(double x)
{
    if (angle_mode == 0) return x * 180.0 / M_PI;
    if (angle_mode == 2) return x * 200.0 / M_PI;
    return x;
}

static double fact(double n)
{
    if (n < 0 && n == floor(n)) fail("factorial of negative integer");
    if (n == floor(n) && n <= 170) {
        double r = 1.0;
        for (long i = 2; i <= (long)n; i++) r *= (double)i;
        return r;
    }
    return tgamma(n + 1.0);          /* generalized factorial */
}

static double gcd_d(double a, double b)
{
    long x = (long)fabs(a), y = (long)fabs(b);
    while (y) { long t = x % y; x = y; y = t; }
    return (double)x;
}

static double npr(double n, double r)
{
    if (n < r || r < 0) fail("invalid nPr");
    return fact(n) / fact(n - r);
}

static double ncr(double n, double r)
{
    if (n < r || r < 0) fail("invalid nCr");
    return fact(n) / (fact(r) * fact(n - r));
}

static Var *find_var(const char *name)
{
    for (int i = 0; i < nvars; i++)
        if (strcmp(vars[i].name, name) == 0) return &vars[i];
    return NULL;
}

static void set_var(const char *name, double v)
{
    Var *x = find_var(name);
    if (x) { x->val = v; return; }
    if (nvars >= (int)(sizeof vars / sizeof vars[0])) fail("too many variables");
    snprintf(vars[nvars].name, sizeof vars[0].name, "%s", name);
    vars[nvars].val = v;
    nvars++;
}

/* ------------------------------------------------------------------ */
/* recursive descent parser                                            */
/* ------------------------------------------------------------------ */

static double parse_expr(void);

static void skip_ws(void) { while (isspace((unsigned char)*p)) p++; }

static double parse_number(void)
{
    skip_ws();
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        char *end;
        double v = (double)strtoll(p + 2, &end, 16);
        if (end == p + 2) fail("bad hex literal");
        p = end;
        return v;
    }
    if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) {
        char *end;
        double v = (double)strtoll(p + 2, &end, 2);
        if (end == p + 2) fail("bad binary literal");
        p = end;
        return v;
    }
    if (p[0] == '0' && (p[1] == 'o' || p[1] == 'O')) {
        char *end;
        double v = (double)strtoll(p + 2, &end, 8);
        if (end == p + 2) fail("bad octal literal");
        p = end;
        return v;
    }
    char *end;
    double v = strtod(p, &end);
    if (end == p) fail("expected a number near \"%.12s\"", p);
    p = end;
    return v;
}

static double call_func(const char *name, double *a, int n)
{
#define F1(fn) do { if (n != 1) fail("%s() takes 1 argument", name); return fn; } while (0)
#define F2(fn) do { if (n != 2) fail("%s() takes 2 arguments", name); return fn; } while (0)

    /* trigonometry */
    if (!strcmp(name, "sin"))   F1(sin(to_rad(a[0])));
    if (!strcmp(name, "cos"))   F1(cos(to_rad(a[0])));
    if (!strcmp(name, "tan"))   F1(tan(to_rad(a[0])));
    if (!strcmp(name, "csc"))   F1(1.0 / sin(to_rad(a[0])));
    if (!strcmp(name, "sec"))   F1(1.0 / cos(to_rad(a[0])));
    if (!strcmp(name, "cot"))   F1(1.0 / tan(to_rad(a[0])));
    if (!strcmp(name, "asin"))  F1(from_rad(asin(a[0])));
    if (!strcmp(name, "acos"))  F1(from_rad(acos(a[0])));
    if (!strcmp(name, "atan"))  F1(from_rad(atan(a[0])));
    if (!strcmp(name, "atan2")) F2(from_rad(atan2(a[0], a[1])));

    /* hyperbolic */
    if (!strcmp(name, "sinh"))  F1(sinh(a[0]));
    if (!strcmp(name, "cosh"))  F1(cosh(a[0]));
    if (!strcmp(name, "tanh"))  F1(tanh(a[0]));
    if (!strcmp(name, "asinh")) F1(asinh(a[0]));
    if (!strcmp(name, "acosh")) F1(acosh(a[0]));
    if (!strcmp(name, "atanh")) F1(atanh(a[0]));

    /* logs and powers */
    if (!strcmp(name, "ln"))    F1(log(a[0]));
    if (!strcmp(name, "log"))   F1(log10(a[0]));
    if (!strcmp(name, "log2"))  F1(log2(a[0]));
    if (!strcmp(name, "logb"))  F2(log(a[1]) / log(a[0]));
    if (!strcmp(name, "exp"))   F1(exp(a[0]));
    if (!strcmp(name, "sqrt"))  F1(sqrt(a[0]));
    if (!strcmp(name, "cbrt"))  F1(cbrt(a[0]));
    if (!strcmp(name, "root"))  F2(pow(a[1], 1.0 / a[0]));
    if (!strcmp(name, "pow"))   F2(pow(a[0], a[1]));
    if (!strcmp(name, "hypot")) F2(hypot(a[0], a[1]));
    if (!strcmp(name, "inv"))   F1(1.0 / a[0]);
    if (!strcmp(name, "sqr"))   F1(a[0] * a[0]);

    /* rounding and sign */
    if (!strcmp(name, "abs"))   F1(fabs(a[0]));
    if (!strcmp(name, "floor")) F1(floor(a[0]));
    if (!strcmp(name, "ceil"))  F1(ceil(a[0]));
    if (!strcmp(name, "round")) F1(round(a[0]));
    if (!strcmp(name, "trunc")) F1(trunc(a[0]));
    if (!strcmp(name, "frac"))  F1(a[0] - trunc(a[0]));
    if (!strcmp(name, "sign"))  F1((a[0] > 0) - (a[0] < 0));

    /* combinatorics and number theory */
    if (!strcmp(name, "fact"))  F1(fact(a[0]));
    if (!strcmp(name, "gamma")) F1(tgamma(a[0]));
    if (!strcmp(name, "ncr"))   F2(ncr(a[0], a[1]));
    if (!strcmp(name, "npr"))   F2(npr(a[0], a[1]));
    if (!strcmp(name, "gcd"))   F2(gcd_d(a[0], a[1]));
    if (!strcmp(name, "lcm"))   F2(fabs(a[0] * a[1]) / gcd_d(a[0], a[1]));
    if (!strcmp(name, "mod"))   F2(fmod(a[0], a[1]));

    /* misc */
    if (!strcmp(name, "min"))   F2(a[0] < a[1] ? a[0] : a[1]);
    if (!strcmp(name, "max"))   F2(a[0] > a[1] ? a[0] : a[1]);
    if (!strcmp(name, "xor"))   F2((double)(((long)a[0]) ^ ((long)a[1])));
    if (!strcmp(name, "rad"))   F1(a[0] * M_PI / 180.0);
    if (!strcmp(name, "deg"))   F1(a[0] * 180.0 / M_PI);
    if (!strcmp(name, "rand"))  { if (n) fail("rand() takes no argument");
                                  return (double)rand() / RAND_MAX; }

    fail("unknown function \"%s\"", name);
    return 0;
#undef F1
#undef F2
}

static double parse_primary(void)
{
    skip_ws();

    if (*p == '(') {
        p++;
        double v = parse_expr();
        skip_ws();
        if (*p != ')') fail("missing ')'");
        p++;
        return v;
    }

    if (isdigit((unsigned char)*p) || *p == '.')
        return parse_number();

    if (isalpha((unsigned char)*p) || *p == '_') {
        char name[32];
        int i = 0;
        while ((isalnum((unsigned char)*p) || *p == '_') && i < 31) name[i++] = *p++;
        name[i] = '\0';

        skip_ws();
        if (*p == '(') {                     /* function call */
            p++;
            double a[4];
            int n = 0;
            skip_ws();
            if (*p != ')') {
                do {
                    if (n >= 4) fail("too many arguments");
                    a[n++] = parse_expr();
                    skip_ws();
                } while (*p == ',' ? (p++, 1) : 0);
            }
            skip_ws();
            if (*p != ')') fail("missing ')' after arguments");
            p++;
            return call_func(name, a, n);
        }

        /* constants */
        if (!strcmp(name, "pi"))  return M_PI;
        if (!strcmp(name, "e"))   return M_E;
        if (!strcmp(name, "phi")) return (1.0 + sqrt(5.0)) / 2.0;
        if (!strcmp(name, "tau")) return 2.0 * M_PI;
        if (!strcmp(name, "inf")) return INFINITY;
        if (!strcmp(name, "ans")) return ans;
        if (!strcmp(name, "mem")) return mem;

        Var *v = find_var(name);
        if (v) return v->val;
        fail("unknown name \"%s\"", name);
    }

    fail("unexpected input near \"%.12s\"", p);
    return 0;
}

static double parse_postfix(void)
{
    double v = parse_primary();
    for (;;) {
        skip_ws();
        if (*p == '!' && p[1] != '=') { p++; v = fact(v); }
        else break;
    }
    return v;
}

static double parse_unary(void);

static double parse_power(void)
{
    double base = parse_postfix();
    skip_ws();
    if (*p == '^') {
        p++;
        double e = parse_unary();          /* right associative */
        return pow(base, e);
    }
    if (p[0] == '*' && p[1] == '*') {
        p += 2;
        double e = parse_unary();
        return pow(base, e);
    }
    return base;
}

static double parse_unary(void)
{
    skip_ws();
    if (*p == '-') { p++; return -parse_unary(); }
    if (*p == '+') { p++; return  parse_unary(); }
    if (*p == '~') { p++; return (double)(~(long)parse_unary()); }
    return parse_power();
}

static double parse_mul(void)
{
    double v = parse_unary();
    for (;;) {
        skip_ws();
        if (*p == '*' && p[1] != '*') { p++; v *= parse_unary(); }
        else if (*p == '/') {
            p++;
            double d = parse_unary();
            if (d == 0) fail("division by zero");
            v /= d;
        } else if (*p == '%') {
            p++;
            double d = parse_unary();
            if (d == 0) fail("modulo by zero");
            v = fmod(v, d);
        } else return v;
    }
}

static double parse_add(void)
{
    double v = parse_mul();
    for (;;) {
        skip_ws();
        if (*p == '+') { p++; v += parse_mul(); }
        else if (*p == '-') { p++; v -= parse_mul(); }
        else return v;
    }
}

static double parse_shift(void)
{
    double v = parse_add();
    for (;;) {
        skip_ws();
        if (p[0] == '<' && p[1] == '<') { p += 2; v = (double)((long)v << (long)parse_add()); }
        else if (p[0] == '>' && p[1] == '>') { p += 2; v = (double)((long)v >> (long)parse_add()); }
        else return v;
    }
}

static double parse_bitand(void)
{
    double v = parse_shift();
    for (;;) {
        skip_ws();
        if (*p == '&' && p[1] != '&') { p++; v = (double)((long)v & (long)parse_shift()); }
        else return v;
    }
}

static double parse_expr(void)
{
    double v = parse_bitand();
    for (;;) {
        skip_ws();
        if (*p == '|' && p[1] != '|') { p++; v = (double)((long)v | (long)parse_bitand()); }
        else return v;
    }
}

/* whole-line evaluation, supports "x = expr" assignment */
double scicalc_eval_line(const char *line)
{
    p = line;
    skip_ws();

    const char *save = p;
    if (isalpha((unsigned char)*p) || *p == '_') {
        char name[32];
        int i = 0;
        const char *q = p;
        while ((isalnum((unsigned char)*q) || *q == '_') && i < 31) name[i++] = *q++;
        name[i] = '\0';
        while (isspace((unsigned char)*q)) q++;
        if (*q == '=' && q[1] != '=') {
            p = q + 1;
            double v = parse_expr();
            skip_ws();
            if (*p) fail("trailing characters \"%.12s\"", p);
            set_var(name, v);
            return v;
        }
        p = save;
    }

    double v = parse_expr();
    skip_ws();
    if (*p) fail("trailing characters \"%.12s\"", p);
    ans = v;
    return v;
}
