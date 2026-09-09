#include "fmt.h"

/* int and long are both 32 bits on this Cortex-M4 target, so reading every
 * numeric argument as long/unsigned long - regardless of whether the format
 * string said 'l' - consumes the same bits va_arg(args, int) would have.
 * The 'l' flag below only has to be recognized so "%ld" parses as one
 * conversion instead of a stray 'l' followed by a literal 'd'. */

static void emit_uint(fmt_emit_fn emit, void *ctx, unsigned long v,
                       int base, int upper, int width, int zero_pad) {
    char digits[11];   /* 32-bit value in base 10 is at most 10 digits */
    int n = 0;
    do {
        int d = (int)(v % (unsigned)base);
        digits[n++] = (char)((d < 10) ? ('0' + d) : ((upper ? 'A' : 'a') + d - 10));
        v /= (unsigned)base;
    } while (v);

    for (int pad = width - n; pad > 0; pad--) emit(zero_pad ? '0' : ' ', ctx);
    while (n > 0) emit(digits[--n], ctx);
}

static void emit_int(fmt_emit_fn emit, void *ctx, long v, int width, int zero_pad) {
    int negative = (v < 0);
    unsigned long uv = negative ? (unsigned long)(-v) : (unsigned long)v;

    int ndigits = 1;
    for (unsigned long t = uv; t >= 10; t /= 10) ndigits++;

    int pad = width - ndigits - (negative ? 1 : 0);
    if (pad < 0) pad = 0;

    /* Zero-padding and space-padding put the sign in different places:
     * "%04d" of -5 is "-005" (sign then zeros), "%5d" of -27 is "  -27"
     * (spaces then sign) - so the sign has to move relative to the pad,
     * not just come first unconditionally. */
    if (zero_pad) {
        if (negative) emit('-', ctx);
        for (int i = 0; i < pad; i++) emit('0', ctx);
    } else {
        for (int i = 0; i < pad; i++) emit(' ', ctx);
        if (negative) emit('-', ctx);
    }
    emit_uint(emit, ctx, uv, 10, 0, 0, 0);   /* magnitude only, no further padding */
}

void fmt_vformat(fmt_emit_fn emit, void *ctx, const char *fmt, va_list args) {
    while (*fmt) {
        if (*fmt != '%') {
            emit(*fmt++, ctx);
            continue;
        }
        fmt++;   /* skip '%' */

        int zero_pad = 0;
        if (*fmt == '0') { zero_pad = 1; fmt++; }

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }

        if (*fmt == 'l') fmt++;   /* recognized, not needed to pick a va_arg type */

        switch (*fmt) {
        case 'd':
            emit_int(emit, ctx, va_arg(args, long), width, zero_pad);
            break;
        case 'u':
            emit_uint(emit, ctx, va_arg(args, unsigned long), 10, 0, width, zero_pad);
            break;
        case 'x':
            emit_uint(emit, ctx, va_arg(args, unsigned long), 16, 0, width, zero_pad);
            break;
        case 'X':
            emit_uint(emit, ctx, va_arg(args, unsigned long), 16, 1, width, zero_pad);
            break;
        case 's': {
            const char *s = va_arg(args, const char *);
            while (*s) emit(*s++, ctx);
            break;
        }
        case 'c':
            emit((char)va_arg(args, int), ctx);
            break;
        case '%':
            emit('%', ctx);
            break;
        default:
            break;   /* unsupported conversion: drop it rather than misprint */
        }

        if (*fmt) fmt++;
    }
}

typedef struct {
    char   *buf;
    size_t  cap;
    size_t  len;
} snprintf_ctx_t;

static void emit_to_buf(char c, void *ctx_) {
    snprintf_ctx_t *ctx = (snprintf_ctx_t *)ctx_;
    if (ctx->len + 1 < ctx->cap) ctx->buf[ctx->len] = c;
    ctx->len++;
}

int fmt_snprintf(char *buf, size_t cap, const char *fmt, ...) {
    snprintf_ctx_t ctx = { buf, cap, 0 };

    va_list args;
    va_start(args, fmt);
    fmt_vformat(emit_to_buf, &ctx, fmt, args);
    va_end(args);

    if (cap > 0) buf[(ctx.len < cap) ? ctx.len : cap - 1] = '\0';
    return (int)ctx.len;
}
