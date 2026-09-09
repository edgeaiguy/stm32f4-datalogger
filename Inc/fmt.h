#ifndef FMT_H
#define FMT_H

#include <stdarg.h>
#include <stddef.h>

/* A small, float-free printf subset, shared by the UART console and CSV
 * formatting. Scoped to exactly what this project prints - not a printf
 * clone. Supported: an optional '0' zero-pad flag, an optional decimal
 * width, an optional 'l' length modifier, and conversions d/u/x/X/s/c/%.
 * Not supported: '-' left-justify, '+'/space flags, precision, ll/h/hh. */

typedef void (*fmt_emit_fn)(char c, void *ctx);

void fmt_vformat(fmt_emit_fn emit, void *ctx, const char *fmt, va_list args);

/* Same contract as snprintf: writes at most cap-1 characters plus a NUL, and
 * returns the number of characters that would have been written had cap been
 * unlimited - so callers can detect truncation with `n >= cap`. */
int fmt_snprintf(char *buf, size_t cap, const char *fmt, ...);

#endif // FMT_H
