#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include "kernel.h"
#include "font.c"
#include "game/tictactoe.h"

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

int16_t cursorx = 0;
int16_t cursory = 0;

int16_t last_cursorx = 0;
int16_t last_cursory = 0;
uint32_t old_pixel;

unsigned char keyboard_read_scancode()
{
    uint8_t status = inb(0x64);
    
    if (status & 1)
    {
        uint8_t data = inb(0x60);

        if (status & (1 << 5))
        {            
            static uint8_t packet[3];
            static int index = 0;

            packet[index++] = data;

            if (index == 3)
            {
                index = 0;

                uint8_t flags = packet[0];
                int8_t dx = (int8_t)packet[1];
                int8_t dy = (int8_t)packet[2];

                cursorx += dx;
                cursory -= dy;

                if (cursorx < 0) cursorx = 0;
                if (cursorx >= SCREEN_WIDTH)  cursorx = SCREEN_WIDTH - 1;
                if (cursory < 0) cursory = 0;
                if (cursory >= SCREEN_HEIGHT) cursory = SCREEN_HEIGHT - 1;

                if (flags & (1 << 0))
                {
                    //left
                }
                if (flags & (1 << 1))
                {
                    //right
                }
            }

            return 0;
        }
        else
        {
            return data;
        }
    }
}

static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile("outb %0, %1"
        :
        : "a"(value), "Nd"(port));
}

// Wait until it's safe to write to the controller (input buffer empty)
void mouse_wait_write()
{
    int timeout = 100000;
    while (timeout--)
    {
        if ((inb(0x64) & 2) == 0)
            return;
    }
}

// Wait until there's data to read (output buffer full)
void mouse_wait_read()
{
    int timeout = 100000;
    while (timeout--)
    {
        if (inb(0x64) & 1)
            return;
    }
}

void mouse_write(uint8_t data)
{
    mouse_wait_write();
    outb(0x64, 0xD4);   // tell controller: next byte is for the mouse
    mouse_wait_write();
    outb(0x60, data);
}

uint8_t mouse_read()
{
    mouse_wait_read();
    return inb(0x60);
}

void mouse_init()
{
    // 1. Enable auxiliary (mouse) device
    mouse_wait_write();
    outb(0x64, 0xA8);

    // 2. Read controller config byte
    mouse_wait_write();
    outb(0x64, 0x20);
    uint8_t status = mouse_read();

    // 3. Enable IRQ12 line in config (bit 1) and enable mouse clock (clear bit 5)
    status |= 0b00000010;
    status &= ~0b00100000;

    // 4. Write config back
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    // 5. Tell mouse to use defaults
    mouse_write(0xF6);
    mouse_read(); // ack (0xFA)

    // 6. Enable data reporting — without this, no packets ever come
    mouse_write(0xF4);
    mouse_read(); // ack (0xFA)
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

#define CURSOR_RADIUS 4
#define CURSOR_BOX (CURSOR_RADIUS * 2 + 1)

static uint32_t cursor_backing[CURSOR_BOX * CURSOR_BOX];
static bool cursor_first = true;

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
    if (!cursor_first)
        restore_cursor_backing(last_cursorx, last_cursory);

    save_cursor_backing(cursorx, cursory);
    draw_cursor_shape(cursorx, cursory, 0x00f2ff);

    last_cursorx = cursorx;
    last_cursory = cursory;
    cursor_first = false;
}

void kernel_main() {
    int count = sizeof(keys) / sizeof(keys[0]);

    VBE_init();
    mouse_init();

    //draw_pixel(10, 10, 0xFF0000);
    //draw_line(0, 0, 10, 10, 4);
    //draw_rect(0, 0, 40, 40, 4);
    //draw_circle(50, 50, 30, 4);
    //draw_char('a', 0, 0, 4);
    //draw_string("rape niggers", 0, 0, 0xFFFFFF);

    while (1)
    {
        cursor();
        
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
                else if (string_equals(command, "tictactoe"))
                {
                    tictactoe();
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
