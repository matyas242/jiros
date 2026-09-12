#include "kernel.h"
#include "draw.h"
#include "game/tictactoe.h"
#include "game/mole.h"
#include "diskmanagment.h"

volatile uint16_t* const VGA = (uint16_t*)0xB8000; //usless now

typedef struct {
    uint32_t framebuffer;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t  bpp;
} __attribute__((packed)) vbe_mode_info_t;

#define VBE_INFO ((vbe_mode_info_t *)0x9200)

void VBE_init(void)
{
    FB = (volatile uint8_t *)VBE_INFO->framebuffer;
    SCREEN_PITCH  = VBE_INFO->pitch;
    SCREEN_WIDTH  = VBE_INFO->width;
    SCREEN_HEIGHT = VBE_INFO->height;
    SCREEN_BPP    = VBE_INFO->bpp;
}

static inline unsigned char inb(unsigned short port)
{
    unsigned char value;

    __asm__ volatile("inb %1, %0"
        : "=a"(value)
        : "Nd"(port));

    return value;
}

uint8_t keyboard_read_scancode()
{
    uint8_t status = inb(0x64);
    
    if (status & 1)
    {
        uint8_t data = inb(0x60);

        if (status & (1 << 5))
        {            
            static uint8_t packet[3];
            static int index = 0;

            if (index == 0)
            {
                if (!(data & 0x08))
                    return 0;
            }

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

                if (flags & 1)
                {
                    if (tictactoe_running)
                    {
                        clicked_check();
                        //left
                    }
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

    return 0;
}

static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile("outb %0, %1"
        :
        : "a"(value), "Nd"(port));
}

// wait until its safe to write to the controller (input buffer empty)
void mouse_wait_write()
{
    int timeout = 100000;
    while (timeout--)
    {
        if ((inb(0x64) & 2) == 0)
            return;
    }
}

// wait until there is data to read (output buffer full)
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
    // enable auxiliary (mouse) device
    mouse_wait_write();
    outb(0x64, 0xA8);

    // read controller config byte
    mouse_wait_write();
    outb(0x64, 0x20);
    uint8_t status = mouse_read();

    // enable IRQ12 line in config (bit 1) and enable mouse clock (clear bit 5)
    status |= 0b00000010;
    status &= ~0b00100000;

    // write config back
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    // tell mouse to use defaults
    mouse_write(0xF6);
    mouse_read(); // ack (0xFA)

    // enable data reporting without this, no packets ever come
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

#define COMMAND_MAX 128
static char command[COMMAND_MAX];

char to_upper(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';

    return c;
}

int result = 0;

static inline unsigned char cmos_read(unsigned char reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static inline int rtc_update_in_progress(void)
{
    return cmos_read(0x0A) & 0x80;
}

static int timezone = 0;

void rtc_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    uint8_t last_second, last_minute, last_hour, reg_b;

    while (rtc_update_in_progress());

    *second = cmos_read(0x00);
    *minute = cmos_read(0x02);
    *hour   = cmos_read(0x04);

    // read twice and compare, in case the clock ticked over mid-read
    do
    {
        last_second = *second;
        last_minute = *minute;
        last_hour   = *hour;

        while (rtc_update_in_progress());

        *second = cmos_read(0x00);
        *minute = cmos_read(0x02);
        *hour   = cmos_read(0x04);
    }
    while (*second != last_second || *minute != last_minute || *hour != last_hour);

    reg_b = cmos_read(0x0B);

    // convert from BCD to normal binary, unless the RTC is already in binary mode
    if (!(reg_b & 0x04))
    {
        *second = (*second & 0x0F) + ((*second / 16) * 10);
        *minute = (*minute & 0x0F) + ((*minute / 16) * 10);
        *hour   = ((*hour & 0x0F) + (((*hour & 0x70) / 16) * 10)) | (*hour & 0x80);
    }

    *hour += timezone;

    // convert 12-hour to 24-hour
    if (!(reg_b & 0x02) && (*hour & 0x80))
    {
        *hour = ((*hour & 0x7F) + 12) % 24;
    }
}

void kernel_main() 
{
    int count = sizeof(keys) / sizeof(keys[0]);

    VBE_init();
    mouse_init();

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

            if (result == 1)
            {
                print("X wins!\n");
                result = 0;
            }
            else if (result == 2)
            {
                print("O wins!\n");
                result = 0;
            }
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
                else if (string_equals(command, "clean"))
                {
                    init = true;
                }
                else if (string_equals(command, "tictactoe"))
                {
                    clean();
                    
                    init = true;

                    tictactoe_running = true;

                    result = tictactoe();
                    
                    clean();
                }
                else if (string_equals(command, "mole"))
                {
                    clean();

                    init = true;

                    mole_hammer();

                    clean();
                }
                else if (string_equals(command, "time"))
                {
                    uint8_t hour, minute, second;
                    rtc_get_time(&hour, &minute, &second);
                    print("Current time: %d:%d:%d\n", hour, minute, second);
                }
                else if (string_equals(command, "changetimezone"))
                {
                    int i = timezone;

                    print("timezone: ");
                    int num_row = row;
                    int num_col = col;

                    bool needs_redraw = true;

                    while (true)
                    {
                        if (needs_redraw)
                        {
                            for (int c = 0; c < 4; c++)
                                clear_at(num_row, num_col + c);

                            set_cursor(num_row, num_col); 

                            if (i > 0)
                                print("+%d", i);
                            else
                                print("%d", i);

                            needs_redraw = false;
                        }

                        uint8_t code = keyboard_read_scancode();

                        if (code == 0x1C) 
                        { 
                            timezone = i; 
                            break; 
                        }
                        else if (code == 0x48) 
                        { 
                            i = (i == 12) ? -12 : i + 1; 
                            needs_redraw = true; 
                        }
                        else if (code == 0x50) 
                        { 
                            i = (i == -12) ? 12 : i - 1; 
                            needs_redraw = true; 
                        }
                    }

                    print("\n");
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

                        if (size < COMMAND_MAX - 1)
                        {
                            command[size] = target;
                            command[size + 1] = '\0';
                        }

                        break;
                    }
                }
            }
        }
    }
}