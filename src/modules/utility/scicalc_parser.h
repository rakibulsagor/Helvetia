#pragma once
#ifndef SCICALC_PARSER_H
#define SCICALC_PARSER_H

#include <setjmp.h>

extern jmp_buf scicalc_err_jmp;
extern char scicalc_err_msg[160];

/* whole-line evaluation, supports "x = expr" assignment */
double scicalc_eval_line(const char *line);

void scicalc_set_angle_mode(int mode);

#endif
