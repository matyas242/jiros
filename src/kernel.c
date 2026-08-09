#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include "kernel.h"
#include "font.c"

volatile uint16_t* const VGA = (uint16_t*)0xB8000; //usless now

uint32_t bgr_color = 0x000000;

typedef struct {
    uint32_t framebuffer;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t  bpp;
} __attribute__((packed)) vbe_mode_info_t;

#define VBE_INFO ((vbe_mode_info_t *)0x9200)

volatile uint8_t *FB;
uint16_t SCREEN_PITCH;
uint16_t SCREEN_WIDTH;
uint16_t SCREEN_HEIGHT;
uint8_t  SCREEN_BPP;

void VBE_init(void)
{
    FB = (volatile uint8_t *)VBE_INFO->framebuffer;
    SCREEN_PITCH  = VBE_INFO->pitch;
    SCREEN_WIDTH  = VBE_INFO->width;
    SCREEN_HEIGHT = VBE_INFO->height;
    SCREEN_BPP    = VBE_INFO->bpp;
}

static int row = 0;
static int col = 0;

float fabsf(float x)
{
    return (x < 0) ? -x : x;
}

void draw_pixel(int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return; // bounds check — cheap insurance now that resolution isn't fixed

    int offset = y * SCREEN_PITCH + x * 3;

    FB[offset + 0] = (color >> 0)  & 0xFF; // Blue
    FB[offset + 1] = (color >> 8)  & 0xFF; // Green
    FB[offset + 2] = (color >> 16) & 0xFF; // Red
}

void draw_line(int x_1, int y_1, int x_2, int y_2, uint32_t color)
{
    float dx = x_2 - x_1;
    float dy = y_2 - y_1;

    if (fabsf(dx) >= fabsf(dy))
    {
        float magic = dy / dx;
        int step = (dx > 0) ? 1 : -1;

        for (int i = 0; i != (int)dx; i += step)
        {
            draw_pixel(x_1 + i, y_1 + (int)(i * magic), color);
        }
    }
    else
    {
        float magic = dx / dy;
        int step = (dy > 0) ? 1 : -1;

        for (int i = 0; i != (int)dy; i += step)
        {
            draw_pixel(x_1 + (int)(i * magic), y_1 + i, color);
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

static inline unsigned char inb(unsigned short port)
{
    unsigned char value;

    __asm__ volatile("inb %1, %0"
        : "=a"(value)
        : "Nd"(port));

    return value;
}

unsigned char keyboard_read_scancode()
{
    while (!(inb(0x64) & 1));

    return inb(0x60);
}

struct key_type {
    uint8_t scancode;
    char character;
};

struct key_type keys[] = {
    {0x1E, 'a'},
    {0x30, 'b'},
    {0x2E, 'c'},
    {0x20, 'd'},
    {0x12, 'e'},
    {0x21, 'f'},
    {0x22, 'g'},
    {0x23, 'h'},
    {0x17, 'i'},
    {0x24, 'j'},
    {0x25, 'k'},
    {0x26, 'l'},
    {0x32, 'm'},
    {0x31, 'n'},
    {0x18, 'o'},
    {0x19, 'p'},
    {0x10, 'q'},
    {0x13, 'r'},
    {0x1F, 's'},
    {0x14, 't'},
    {0x16, 'u'},
    {0x2F, 'v'},
    {0x11, 'w'},
    {0x2D, 'x'},
    {0x15, 'y'},
    {0x2C, 'z'},
    {0x0B, '0'},
    {0x02, '1'},
    {0x03, '2'},
    {0x04, '3'},
    {0x05, '4'},
    {0x06, '5'},
    {0x07, '6'},
    {0x08, '7'},
    {0x09, '8'},
    {0x0A, '9'},
    {0x39, ' '},
};

int string_equals(const char* a, const char* b)
{
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
            return 0;

        i++;
    }

    return a[i] == b[i];
}

static bool new_command = true;
static bool init = true;
static bool caps = false;

static char command[] = "";

struct vec2
{
    int x;
    int y;
};

char to_upper(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';

    return c;
}

void kernel_main() {
    int count = sizeof(keys) / sizeof(keys[0]);

    VBE_init();

    //draw_pixel(10, 10, 0xFF0000);
    //draw_line(0, 0, 10, 10, 4);
    //draw_rect(0, 0, 40, 40, 4);
    //draw_circle(50, 50, 30, 4);
    //draw_char('a', 0, 0, 4);
    //draw_string("rape niggers", 0, 0, 0xFFFFFF);

    while (1)
    {
        if (init == true)
        {
            init = false;

            row = 0;
            col = 0;
            
            clean();

            print("JirOS\n"); 
        }

        if (new_command == true) 
        { 
            new_command = false;

            command[0] = '\0';

            print(">");
        }

        unsigned char code = keyboard_read_scancode();

        if (code)
        {
            if (code == 0x1C)
            {
                new_command = true;

                print("\n");

                if (string_equals(command, "help"))
                {
                    print("help none existent");
                }
                else if (string_equals(command, "clear"))
                {
                    init = true;
                }
                else
                {
                    print("unknown command");
                }

                print("\n");
            }
            else if (code == 0x0E)
            {
                if (col >= 2)
                {
                    col--;

                    clear_at(row, col);

                    command[col - 1] = '\0';
                }
            }
            else if (code == 0x2A || code == 0xAA || code == 0x3A)
            {
                if (caps == true)
                {
                    caps = false;
                }
                else
                {
                    caps = true;
                }
            }
            else
            {
                for (int i = 0; i < count; i++)
                {
                    if (keys[i].scancode == code)
                    {
                        char target = keys[i].character;

                        if (caps == true)
                        {
                            target = to_upper(target);
                        }

                        print("%c", target);

                        int size = 0;

                        while (command[size] != '\0')
                            size++;

                        command[size] = target;
                        command[size + 1] = '\0';

                        break;
                    }
                }
            }
        }
    }
}#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include "kernel.h"

volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static int row = 0;
static int col = 0;

void vga_clean(int count) {
    for (int i = 0; i < count; i++) {
        VGA[i] = ((uint16_t)0x0F << 8) | ' ';
    }
}

void clear_at(int x, int y)
{
    int index = x * 80 + y;

    VGA[index] = ((uint16_t)0x0F << 8) | ' ';
}

void print_char(char c)
{
    if (c == '\n')
    {
        row++;
        col = 0;
        return;
    }

    VGA[row * 80 + col] = 0x0F00 | c;
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

static inline unsigned char inb(unsigned short port)
{
    unsigned char value;

    __asm__ volatile("inb %1, %0"
        : "=a"(value)
        : "Nd"(port));

    return value;
}

unsigned char keyboard_read_scancode()
{
    while (!(inb(0x64) & 1));

    return inb(0x60);
}

struct key_type {
    uint8_t scancode;
    char character;
};

struct key_type keys[] = {
    {0x1E, 'a'},
    {0x30, 'b'},
    {0x2E, 'c'},
    {0x20, 'd'},
    {0x12, 'e'},
    {0x21, 'f'},
    {0x22, 'g'},
    {0x23, 'h'},
    {0x17, 'i'},
    {0x24, 'j'},
    {0x25, 'k'},
    {0x26, 'l'},
    {0x32, 'm'},
    {0x31, 'n'},
    {0x18, 'o'},
    {0x19, 'p'},
    {0x10, 'q'},
    {0x13, 'r'},
    {0x1F, 's'},
    {0x14, 't'},
    {0x16, 'u'},
    {0x2F, 'v'},
    {0x11, 'w'},
    {0x2D, 'x'},
    {0x15, 'y'},
    {0x2C, 'z'},
    {0x0B, '0'},
    {0x02, '1'},
    {0x03, '2'},
    {0x04, '3'},
    {0x05, '4'},
    {0x06, '5'},
    {0x07, '6'},
    {0x08, '7'},
    {0x09, '8'},
    {0x0A, '9'},
    {0x39, ' '},
};

int string_equals(const char* a, const char* b)
{
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
            return 0;

        i++;
    }

    return a[i] == b[i];
}

static bool new_command = true;
static bool init = true;
static bool caps = false;

static char command[] = "";

char to_upper(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';

    return c;
}

void kernel_main() {
    int count = sizeof(keys) / sizeof(keys[0]);

    while (1)
    {
        if (init == true)
        {
            init = false;

            row = 0;
            col = 0;
            
            vga_clean(1000);

            print("JirOS\n");
        }

        if (new_command == true) 
        { 
            new_command = false;

            command[0] = '\0';

            print(">");
        }

        unsigned char code = keyboard_read_scancode();

        if (code)
        {
            if (code == 0x1C)
            {
                new_command = true;

                print("\n");

                if (string_equals(command, "help"))
                {
                    print("help none existent");
                }
                else if (string_equals(command, "clear"))
                {
                    init = true;
                }
                else
                {
                    print("unknown command");
                }

                print("\n");
            }
            else if (code == 0x0E)
            {
                if (col >= 2)
                {
                    col--;

                    clear_at(row, col);

                    command[col - 1] = '\0';
                }
            }
            else if (code == 0x2A || code == 0xAA || code == 0x3A)
            {
                if (caps == true)
                {
                    caps = false;
                }
                else
                {
                    caps = true;
                }
            }
            else
            {
                for (int i = 0; i < count; i++)
                {
                    if (keys[i].scancode == code)
                    {
                        char target = keys[i].character;

                        if (caps == true)
                        {
                            target = to_upper(target);
                        }

                        print("%c", target);

                        int size = 0;

                        while (command[size] != '\0')
                            size++;

                        command[size] = target;
                        command[size + 1] = '\0';

                        break;
                    }
                }
            }
        }
    }
}
