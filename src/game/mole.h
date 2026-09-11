void display_mole_holes()
{
    draw_circle(50, 50, 50, 0xFFFFFF); 
    draw_circle(150, 50, 50, 0xFFFFFF); 
    draw_circle(250, 50, 50, 0xFFFFFF); 
    draw_circle(350, 50, 50, 0xFFFFFF); 
    draw_circle(450, 50, 50, 0xFFFFFF); 
}

#define HAMMER_WIDTH  21
#define HAMMER_HEIGHT 41

uint32_t hammer_backing[HAMMER_WIDTH * HAMMER_HEIGHT];

void save_hammer_backing(int cx, int cy)
{
    int i = 0;

    for (int dy = -10; dy <= 30; dy++)
        for (int dx = -10; dx <= 10; dx++)
            hammer_backing[i++] = read_pixel(cx + dx, cy + dy);
}

void restore_hammer_backing(int cx, int cy)
{
    int i = 0;

    for (int dy = -10; dy <= 30; dy++)
        for (int dx = -10; dx <= 10; dx++)
            draw_pixel(cx + dx, cy + dy, hammer_backing[i++]);
}

void display_hammer()
{
    if (!cursor_first && cursorx == last_cursorx && cursory == last_cursory)
        return;

    restore_hammer_backing(last_cursorx, last_cursory);

    save_hammer_backing(cursorx, cursory);

    draw_rect_filled(cursorx - 10, cursory - 10, cursorx + 10, cursory + 10, 0xb8b6b6); //hammer

    draw_rect_filled(cursorx - 2, cursory + 10, cursorx + 2, cursory + 30, 0x733b00); //handle

    last_cursorx = cursorx;
    last_cursory = cursory;
}

void mole_hammer()
{
    while (true)
    {
        keyboard_read_scancode();

        display_hammer();

        display_mole_holes();

        int target = random(1, 5);
    }
}
