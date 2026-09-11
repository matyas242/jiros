
typedef struct {
    int x;
    int y;
} vec2;

//state 0 = none; 1 = x; 2 = o
struct boxes_types
{
    vec2 one;
    vec2 two;
    int state;
};

struct boxes_types boxes[9] =
{
    {{10, 10}, {310, 310}, 0},
    {{310, 10}, {610, 310}, 0},
    {{610, 10}, {910, 310}, 0},
    {{10, 310}, {310, 610}, 0},
    {{310, 310}, {610, 610}, 0},
    {{610, 310}, {910, 610}, 0},
    {{10, 610}, {310, 910}, 0},
    {{310, 610}, {610, 910}, 0},
    {{610, 610}, {910, 910}, 0}
};

// 0 = x; 1 = o
bool turn = 0;

void draw_x(vec2 xy1, vec2 xy2)
{
    draw_line(xy1.x, xy1.y, xy2.x, xy2.y, 0xFFFFFF);

    draw_line(xy1.x, xy2.y, xy2.x, xy1.y, 0xFFFFFF);
}

int tictactoe()
{
    draw_rect(10, 10, 910, 910, 0xFFFFFF);

    draw_line(10, 310, 910, 310, 0xFFFFFF);

    draw_line(10, 610, 910, 610, 0xFFFFFF);

    draw_line(310, 10, 310, 910, 0xFFFFFF);

    draw_line(610, 10, 610, 910, 0xFFFFFF);
    
    while (true)
    {
        keyboard_read_scancode();
        cursor();

        for (int i = 0; i < 9; i++)
        {
            if (boxes[i].state == 0 || boxes[i].state > 2)
            {
                continue;
            }
            else if (boxes[i].state == 1)
            {
                draw_x(boxes[i].one, boxes[i].two);
            }
            else if (boxes[i].state == 2)
            {
                int xlenght = (boxes[i].two.x - boxes[i].one.x) / 2;
                int ylenght = (boxes[i].two.y - boxes[i].one.y) / 2;
                draw_circle(boxes[i].one.x + xlenght, boxes[i].one.y + ylenght, 150, 0xFFFFFF);
            }
        }

        for (int i = 1; i < 3; i++)
        {
            if ((boxes[0].state == i && boxes[1].state == i && boxes[2].state == i) || (boxes[3].state == i && boxes[4].state == i && boxes[5].state == i) || (boxes[6].state == i && boxes[7].state == i && boxes[8].state == i) || (boxes[0].state == i && boxes[3].state == i && boxes[6].state == i) || (boxes[1].state == i && boxes[4].state == i && boxes[7].state == i) || (boxes[2].state == i && boxes[5].state == i && boxes[8].state == i) || (boxes[1].state == i && boxes[4].state == i && boxes[7].state == i) || (boxes[2].state == i && boxes[5].state == i && boxes[8].state == i) || (boxes[0].state == i && boxes[4].state == i && boxes[8].state == i) || (boxes[2].state == i && boxes[4].state == i && boxes[6].state == i))
            {
                tictactoe_running = false;

                turn = 0;

                for (int i = 0; i < 9; i++)
                    boxes[i].state = 0;

                if (i == 1)
                {
                    return 1;
                }
                else
                {
                    return 2;
                }
            }
        }
    }
}

void clicked_check()
{
    if (cursorx >= boxes[0].one.x && cursorx <= boxes[0].two.x && cursory >= boxes[0].one.y && cursory <= boxes[0].two.y && boxes[0].state == 0)
    {
        boxes[0].state = turn + 1;
        turn = !turn;
    }
    else if (cursorx >= boxes[1].one.x && cursorx <= boxes[1].two.x && cursory >= boxes[1].one.y && cursory <= boxes[1].two.y && boxes[1].state == 0)
    {
        boxes[1].state = turn + 1;
        turn = !turn;
    }
    else if (cursorx >= boxes[2].one.x && cursorx <= boxes[2].two.x && cursory >= boxes[2].one.y && cursory <= boxes[2].two.y && boxes[2].state == 0)
    {
        boxes[2].state = turn + 1;
        turn = !turn;
    }
    else if (cursorx >= boxes[3].one.x && cursorx <= boxes[3].two.x && cursory >= boxes[3].one.y && cursory <= boxes[3].two.y && boxes[3].state == 0)
    {
        boxes[3].state = turn + 1;
        turn = !turn;
    }
    else if (cursorx >= boxes[4].one.x && cursorx <= boxes[4].two.x && cursory >= boxes[4].one.y && cursory <= boxes[4].two.y && boxes[4].state == 0)
    {
        boxes[4].state = turn + 1;
        turn = !turn;
    }
    else if (cursorx >= boxes[5].one.x && cursorx <= boxes[5].two.x && cursory >= boxes[5].one.y && cursory <= boxes[5].two.y && boxes[5].state == 0)
    {
        boxes[5].state = turn + 1;
        turn = !turn;
    }
    else if (cursorx >= boxes[6].one.x && cursorx <= boxes[6].two.x && cursory >= boxes[6].one.y && cursory <= boxes[6].two.y && boxes[6].state == 0)
    {
        boxes[6].state = turn + 1;
        turn = !turn;
    }
    else if (cursorx >= boxes[7].one.x && cursorx <= boxes[7].two.x && cursory >= boxes[7].one.y && cursory <= boxes[7].two.y && boxes[7].state == 0)
    {
        boxes[7].state = turn + 1;
        turn = !turn;
    }
    else if (cursorx >= boxes[8].one.x && cursorx <= boxes[8].two.x && cursory >= boxes[8].one.y && cursory <= boxes[8].two.y && boxes[8].state == 0)
    {
        boxes[8].state = turn + 1;
        turn = !turn;
    }
}