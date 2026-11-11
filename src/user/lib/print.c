#include "user/lib/print.h"
#include "user/os/syscall.h"
#define PF_BUF_SIZE 256

typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
} Out;

typedef u64 uintptr_t;

static void out_ch(Out *o, char c) {
    if (o->len + 1 < o->cap) o->buf[o->len] = c;
    o->len++;
}

static void out_pad(Out *o, char c, int n) {
    while (n-- > 0) out_ch(o, c);
}

static void out_num(Out *o, u64 val, int base, int width, char pad, int upper, int neg)
{
    char tmp[32];
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    if (val == 0) tmp[i++] = '0';
    while (val) { tmp[i++] = dig[val % (u64)base]; val /= (u64)base; }

    int total = i + (neg ? 1 : 0);
    int pad_n = width > total ? width - total : 0;

    if (pad == '0' && !neg) {
        out_pad(o, '0', pad_n);
        while (i--) out_ch(o, tmp[i]);
    } else {
        if (pad != '0') out_pad(o, ' ', pad_n);
        if (neg) out_ch(o, '-');
        if (pad == '0') out_pad(o, '0', pad_n);
        while (i--) out_ch(o, tmp[i]);
    }
}

static int vfmt(char *buf, size_t cap, const char *fmt, va_list ap)
{
    Out o = { buf, cap, 0 };

    for (; *fmt; fmt++) {
        if (*fmt != '%') { out_ch(&o, *fmt); continue; }
        fmt++;

        int left = 0, zero = 0;
        while (*fmt == '-' || *fmt == '0') {
            if (*fmt == '-') left = 1;
            if (*fmt == '0') zero = 1;
            fmt++;
        }

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            while (*fmt >= '0' && *fmt <= '9')
                prec = prec * 10 + (*fmt++ - '0');
        }

        char pad = left ? ' ' : (zero ? '0' : ' ');

        switch (*fmt) {
        case 'c':
            out_ch(&o, (char)va_arg(ap, int));
            break;

        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            int n = 0;
            while (s[n] && (prec < 0 || n < prec)) n++;
            int pad_n = width > n ? width - n : 0;
            if (!left) out_pad(&o, ' ', pad_n);
            for (int k = 0; k < n; k++) out_ch(&o, s[k]);
            if (left) out_pad(&o, ' ', pad_n);
            break;
        }

        case 'd': case 'i': {
            i64 v = va_arg(ap, i64);
            int neg = 0;
            u64 uv;
            if (v < 0) { neg = 1; uv = (u64)(-(v + 1)) + 1; }  // 避免 -INT64_MIN 溢出
            else       { uv = (u64)v; }
            out_num(&o, uv, 10, width, pad, 0, neg);
            break;
        }

        case 'u':
            out_num(&o, va_arg(ap, u64), 10, width, pad, 0, 0);
            break;

        case 'x': case 'X':
            out_num(&o, va_arg(ap, u64), 16, width, pad, *fmt == 'X', 0);
            break;

        case 'p': {
            u64 v = (u64)(uintptr_t)va_arg(ap, void *);
            out_ch(&o, '0');
            out_ch(&o, 'x');
            out_num(&o, v, 16, width > 2 ? width - 2 : 0, '0', 0, 0);
            break;
        }

        case '%':
            out_ch(&o, '%');
            break;

        default:
            out_ch(&o, '%');
            if (*fmt) out_ch(&o, *fmt);
            else      fmt--;
        }
    }

    if (cap) buf[o.len < cap ? o.len : cap - 1] = '\0';
    return (int)o.len;
}

int printf_imple(const char *fmt, ...) {
    char buf[PF_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int n = vfmt(buf, sizeof buf, fmt, ap);
    va_end(ap);

    u64 wr = (n < (int)sizeof buf) ? (u64)n : (u64)(sizeof buf - 1);
    write(1, buf, wr);
    return n;
}

