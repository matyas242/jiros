#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include "font.c"

uint32_t bgr_color = 0x000000;

float fabsf(float x)
{
    return (x < 0) ? -x : x;
}

uint8_t keyboard_read_scancode(void);

volatile uint8_t *FB;
uint16_t SCREEN_PITCH;
uint16_t SCREEN_WIDTH;
uint16_t SCREEN_HEIGHT;
uint8_t  SCREEN_BPP;

static bool tictactoe_running = false;

static int16_t cursorx = 0;
static int16_t cursory = 0;

static int16_t last_cursorx = 0;
static int16_t last_cursory = 0;
static uint32_t old_pixel;

#define CURSOR_RADIUS 4
#define CURSOR_BOX (CURSOR_RADIUS * 2 + 1)

static uint32_t cursor_backing[CURSOR_BOX * CURSOR_BOX];
static bool cursor_first = true;

uint32_t read_pixel(int x, int y)
{
    if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return 0;

    int offset = y * SCREEN_PITCH + x * 3;

    uint32_t color = 0;
    color |= (uint32_t)FB[offset + 0] << 0;   // Blue
    color |= (uint32_t)FB[offset + 1] << 8;   // Green
    color |= (uint32_t)FB[offset + 2] << 16;  // Red

    return color;
}

void draw_pixel(int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return;

    int offset = y * SCREEN_PITCH + x * 3;

    FB[offset + 0] = (color >> 0)  & 0xFF; // Blue
    FB[offset + 1] = (color >> 8)  & 0xFF; // Green
    FB[offset + 2] = (color >> 16) & 0xFF; // Red
}

void draw_line(int x1, int y1, int x2, int y2, uint32_t color)
{
    int dx = x2 - x1;
    int dy = y2 - y1;

    int sx = (dx >= 0) ? 1 : -1;
    int sy = (dy >= 0) ? 1 : -1;

    if (dx < 0)
        dx = -dx;

    if (dy < 0)
        dy = -dy;

    int err = dx - dy;

    while (1)
    {
        draw_pixel(x1, y1, color);

        if (x1 == x2 && y1 == y2)
            break;

        int e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }
}

void draw_rect(int x_1, int y_1, int x_2, int y_2, uint32_t color)
{
    draw_line(x_1, y_1, x_1, y_2, color);
    draw_line(x_1, y_2, x_2, y_2, color);
    draw_line(x_2, y_2, x_2, y_1, color);
    draw_line(x_2, y_1, x_1, y_1, color);
}

void draw_rect_filled(int x_1, int y_1, int x_2, int y_2, uint32_t color)
{
    for (int y = y_1; y <= y_2; y++)
    {
        draw_line(x_1, y, x_2, y, color);
    }
}

void draw_circle(int center_x, int center_y, int diameter, uint32_t color)
{
    int radius = diameter / 2;
    int x = 0;
    int y = radius;
    int d = 1 - radius;

    while (x <= y)
    {
        draw_pixel(center_x + x, center_y + y, color);
        draw_pixel(center_x - x, center_y + y, color);
        draw_pixel(center_x + x, center_y - y, color);
        draw_pixel(center_x - x, center_y - y, color);
        draw_pixel(center_x + y, center_y + x, color);
        draw_pixel(center_x - y, center_y + x, color);
        draw_pixel(center_x + y, center_y - x, color);
        draw_pixel(center_x - y, center_y - x, color);

        x++;

        if (d < 0)
        {
            d += 2 * x + 1;
        }
        else
        {
            y--;
            d += 2 * (x - y) + 1;
        }
    }
}

void draw_char(char c, int x, int y, uint32_t color)
{
    uint8_t *glyph = (uint8_t *)font8x8_basic[(unsigned char)c];

    for (int row = 0; row < 8; row++)
    {
        uint8_t bits = glyph[row];

        for (int col = 0; col < 8; col++)
        {
            if (bits & (1 << col))
            {
                draw_pixel(x + col, y + row, color);
            }
        }
    }
}

void draw_string(const char *str, int x, int y, uint32_t color)
{
    int cursor_x = x;
    int cursor_y = y;

    while (*str)
    {
        if (*str == '\n')
        {
            cursor_y += 8;
            cursor_x = x;
        }
        else
        {
            draw_char(*str, cursor_x, cursor_y, color);
            cursor_x += 8;
        }
        str++;
    }
}

void clean() {
    for (int i = 0; i < SCREEN_WIDTH; i++)
    {
        for (int y = 0; y < SCREEN_HEIGHT; y++)
        {
            draw_pixel(i, y, bgr_color);
        }
    }   
}

void clear_at(int row, int col)
{
    int px = col * 8;
    int py = row * 10;

    for (int y = 0; y < 10; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            draw_pixel(px + x, py + y, bgr_color);
        }
    }
}

static int row = 0;
static int col = 0;

void print_char(char c)
{
    if (c == '\n')
    {
        row++;
        col = 0;
        return;
    }

    draw_char(c, col * 8, row * 10, 0xFFFFFF);
    //VGA[row * 80 + col * 8] = 0x0F00 | c;
    col++;
}

void print_string(const char* s)
{
    while (*s)
        print_char(*s++);
}

void print_int(int value)
{
    char buf[12];
    int i = 0;

    if (value == 0)
    {
        print_char('0');
        return;
    }

    if (value < 0)
    {
        print_char('-');
        value = -value;
    }

    while (value)
    {
        buf[i++] = '0' + value % 10;
        value /= 10;
    }

    while (i--)
        print_char(buf[i]);
}

void print_hex(unsigned int value)
{
    char hex[] = "0123456789ABCDEF";

    print_string("0x");

    for (int i = 28; i >= 0; i -= 4)
        print_char(hex[(value >> i) & 0xF]);
}

void print(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt)
    {
        if (*fmt == '%')
        {
            fmt++;

            switch (*fmt)
            {
                case 'd':
                    print_int(va_arg(args, int));
                    break;

                case 'x':
                    print_hex(va_arg(args, unsigned int));
                    break;

                case 's':
                    print_string(va_arg(args, char*));
                    break;

                case 'c':
                    print_char((char)va_arg(args, int));
                    break;

                case '%':
                    print_char('%');
                    break;

                default:
                    print_char('%');
                    print_char(*fmt);
                    break;
            }
        }
        else
        {
            print_char(*fmt);
        }

        fmt++;
    }

    va_end(args);
}

void draw_cursor_shape(int cx, int cy, uint32_t color)
{
    for (int dy = -CURSOR_RADIUS; dy <= CURSOR_RADIUS; dy++)
    {
        for (int dx = -CURSOR_RADIUS; dx <= CURSOR_RADIUS; dx++)
        {
            if (dx * dx + dy * dy <= CURSOR_RADIUS * CURSOR_RADIUS)
                draw_pixel(cx + dx, cy + dy, color);
        }
    }
}

void save_cursor_backing(int cx, int cy)
{
    int i = 0;
    for (int dy = -CURSOR_RADIUS; dy <= CURSOR_RADIUS; dy++)
        for (int dx = -CURSOR_RADIUS; dx <= CURSOR_RADIUS; dx++)
            cursor_backing[i++] = read_pixel(cx + dx, cy + dy);
}

void restore_cursor_backing(int cx, int cy)
{
    int i = 0;
    for (int dy = -CURSOR_RADIUS; dy <= CURSOR_RADIUS; dy++)
        for (int dx = -CURSOR_RADIUS; dx <= CURSOR_RADIUS; dx++)
            draw_pixel(cx + dx, cy + dy, cursor_backing[i++]);
}

void cursor()
{
    if (!cursor_first && cursorx == last_cursorx && cursory == last_cursory)
        return;

    if (!cursor_first)
        restore_cursor_backing(last_cursorx, last_cursory);

    save_cursor_backing(cursorx, cursory);
    draw_cursor_shape(cursorx, cursory, 0x00f2ff);

    last_cursorx = cursorx;
    last_cursory = cursory;
    cursor_first = false;
}

static uint32_t rng_state = 88172645463325252ULL & 0xFFFFFFFF;

void random_seed(uint32_t seed) {
    if (seed == 0) seed = 0xACE1u; // avoid zero-state lockup
    rng_state = seed;
}

static uint32_t xorshift32(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static int random_int = 0;

int random(int min, int max) {
    random_int++;
    random_seed(random_int);
    if (min > max) {
        int tmp = min;
        min = max;
        max = tmp;
    }
    uint32_t range = (uint32_t)(max - min) + 1;
    return min + (int)(xorshift32() % range);
}