#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define S  800
#define N  360
#define PI 3.141592653589793

static uint32_t
pcg32(uint64_t *s)
{
    uint64_t m = 0x9b60933458e17d7d;
    uint64_t a = 0xd737232eeccdf7ed;
    *s = *s * m + a;
    int shift = 29 - (*s >> 61);
    return *s >> shift;
}

static float
clamp(float x, float lower, float upper)
{
    if (x < lower)
        return lower;
    if (x > upper)
        return upper;
    return x;
}

static float
smoothstep(float lower, float upper, float x)
{
    x = clamp((x - lower) / (upper - lower), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

static void
rgb_split(unsigned long c, float *r, float *g, float *b)
{
    *r = sqrtf((c >> 16) / 255.0f);
    *g = sqrtf(((c >> 8) & 0xff) / 255.0f);
    *b = sqrtf((c & 0xff) / 255.0f);
}

static unsigned long
rgb_join(float r, float g, float b)
{
    unsigned long ir = roundf(r * r * 255.0);
    unsigned long ig = roundf(g * g * 255.0);
    unsigned long ib = roundf(b * b * 255.0);
    return (ir << 16) | (ig << 8) | ib;
}

static void
ppm_write(const unsigned char *buf, FILE *f)
{
    fprintf(f, "P6\n%d %d\n255\n", S, S);
    fwrite(buf, S * 3, S, f);
    fflush(f);
}

static void
ppm_set(unsigned char *buf, int x, int y, unsigned long color)
{
    buf[y * S * 3 + x * 3 + 0] = color >> 16;
    buf[y * S * 3 + x * 3 + 1] = color >>  8;
    buf[y * S * 3 + x * 3 + 2] = color >>  0;
}

static unsigned long
ppm_get(unsigned char *buf, int x, int y)
{
    unsigned long r = buf[y * S * 3 + x * 3 + 0];
    unsigned long g = buf[y * S * 3 + x * 3 + 1];
    unsigned long b = buf[y * S * 3 + x * 3 + 2];
    return (r << 16) | (g << 8) | b;
}

static void
ppm_circle(unsigned char *buf, float x, float y, unsigned long fgc)
{
    #define R0 2.0f
    #define R1 4.0f
    float fr, fg, fb;
    rgb_split(fgc, &fr, &fg, &fb);
    for (int py = floorf(y - R1 - 1); py <= ceilf(y + R1 + 1); py++) {
        float dy = py - y;
        for (int px = floorf(x - R1 - 1); px <= ceilf(x + R1 + 1); px++) {
            float dx = px - x;
            float d = sqrtf(dy * dy + dx * dx);
            float a = smoothstep(R1, R0, d);

            unsigned long bgc = ppm_get(buf, px, py);
            float br, bg, bb;
            rgb_split(bgc, &br, &bg, &bb);

            float r = a * fr + (1 - a) * br;
            float g = a * fg + (1 - a) * bg;
            float b = a * fb + (1 - a) * bb;
            ppm_set(buf, px, py, rgb_join(r, g, b));
        }
    }
}

static unsigned long
hue(int v)
{
    unsigned long h = v / (N / 6);
    unsigned long f = v % (N / 6);
    unsigned long t = 0xff * f / (N / 6);
    unsigned long q = 0xff - t;
    switch (h) {
        case 0:
            return 0xff0000UL | (t << 8);
        case 1:
            return (q << 16) | 0x00ff00UL;
        case 2:
            return 0x00ff00UL | t;
        case 3:
            return (q << 8) | 0x0000ffUL;
        case 4:
            return (t << 16) | 0x0000ffUL;
        case 5:
            return 0xff0000UL | q;
    }
    abort();
}

static int array[N];

static void
frame(void)
{
    static unsigned char buf[S * S * 3];
    memset(buf, 0, sizeof(buf));
    for (int i = 0; i < N; i++) {
        float delta = abs(i - array[i]) / (N / 2.0);
        float x = -sinf(i * 2.0 * PI / N);
        float y = -cosf(i * 2.0 * PI / N);
        float r = S * 15.0 / 32.0 * (1.0 - delta);
        float px = r * x + S / 2;
        float py = r * y + S / 2;
        ppm_circle(buf, px, py, hue(array[i]));
    }
    ppm_write(buf, stdout);
}

static void
swap(int array[N], int a, int b)
{
    int tmp = array[a];
    array[a] = array[b];
    array[b] = tmp;
}

static void
sort_even_odd(int array[N])
{
    int c;
    do {
        c = 0;
        for(int i = 1; i < N - 1; i += 2) {
            if (array[i] > array[i + 1]) {
                swap(array, i, i + 1);
                c = 1;
            }
        }
        for (int i = 0; i < N - 1; i += 2) {
            if (array[i] > array[i + 1]) {
                swap(array, i, i + 1);
                c = 1;
            }
        }
        frame();
    } while (c);
}

static void
sort_bubble(int array[N])
{
    int c;
    do {
        c = 0;
        for (int i = 1; i < N; i++) {
            if (array[i - 1] > array[i]) {
                swap(array, i - 1, i);
                c = 1;
            }
        }
        frame();
    } while (c);
}

static void
sort_insertion(int array[N])
{
    for (int i = 1; i < N; i++) {
        for (int j = i; j > 0 && array[j - 1] > array[j]; j--)
            swap(array, j, j - 1);
        frame();
    }
}

static void
sort_stoogesort(int array[N], int i, int j)
{
    static int c = 0;
    if (array[i] > array[j]) {
        swap(array, i, j);
        if (c++ % 32 == 0)
            frame();
    }
    if (j - i + 1 > 2) {
        int t = (j - i + 1) / 3;
        sort_stoogesort(array, i, j - t);
        sort_stoogesort(array, i + t, j);
        sort_stoogesort(array, i, j - t);
    }
}

static void
sort_quicksort(int *array, int n)
{
    if (n > 1) {
        int high = n;
        for (int i = 1; i < high;) {
            if (array[0] < array[i]) {
                swap(array, i, --high);
                frame();
            } else {
                i++;
            }
        }
        swap(array, 0, --high);
        frame();
        sort_quicksort(array, high + 1);
        sort_quicksort(array + high + 1, n - high - 1);
    }
}

static int
digit(int v, int b, int d)
{
    for (int i = 0; i < d; i++)
        v /= b;
    return v % b;
}

static void
sort_radix_lsd(int *array, int b)
{
    int c, total = 1;
    for (int d = 0; total; d++) {
        total = -1;
        do {
            total++;
            c = 0;
            for(int i = 1; i < N - 1; i += 2) {
                if (digit(array[i], b, d) > digit(array[i + 1], b, d)) {
                    swap(array, i, i + 1);
                    c = 1;
                }
            }
            for (int i = 0; i < N - 1; i += 2) {
                if (digit(array[i], b, d) > digit(array[i + 1], b, d)) {
                    swap(array, i, i + 1);
                    c = 1;
                }
            }
            frame();
        } while (c);
    }
}

#define SHUFFLE_DRAW  (1u << 0)

static void
shuffle(int array[N], unsigned flags)
{
    uint64_t s[1] = {0};
    for (int i = N - 1; i > 0; i--) {
        uint32_t r = pcg32(s) % (i + 1);
        swap(array, i, r);
        if (flags & SHUFFLE_DRAW)
            frame();
    }
}

int
main(void)
{
    enum {
        SORT_NULL,
        SORT_EVEN_ODD,
        SORT_BUBBLE,
        SORT_INSERTION,
        SORT_STOOGESORT,
        SORT_QSORT,
        SORT_RADIX_8_LSD,
    } type = SORT_STOOGESORT;

    for (int i = 0; i < N; i++)
        array[i] = i;
    frame();

    shuffle(array, SHUFFLE_DRAW);

    switch (type) {
        case SORT_NULL:
            break;
        case SORT_EVEN_ODD:
            sort_even_odd(array);
            break;
        case SORT_BUBBLE:
            sort_bubble(array);
            break;
        case SORT_INSERTION:
            sort_insertion(array);
            break;
        case SORT_STOOGESORT:
            sort_stoogesort(array, 0, N - 1);
            break;
        case SORT_QSORT:
            sort_quicksort(array, N);
            break;
        case SORT_RADIX_8_LSD:
            sort_radix_lsd(array, 8);
            break;
    }
    frame();
}
