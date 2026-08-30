/*
 * GUI.COM — NABU-inspired file desktop for CP/M Neo.
 *
 * The BIOS exposes an 80x24 character terminal, so this uses ANSI cursor and
 * reverse-video controls to model a menu bar and two directory windows.  It
 * blocks for keyboard input and redraws only after an action.
 */

#include <cpm.h>

#define COLS 80
#define ROWS 24
#define PANE_FILES 48
#define LIST_ROWS 12
#define VIEW_ROWS 17
#define PAGE_HISTORY 64

#define NORMAL 0
#define REVERSE 1

typedef struct
{
    char ch;
    uint8_t attr;
} Cell;

typedef struct
{
    char name[FILENAME_MAX];
    uint32_t size;
    uint8_t attrib;
    uint8_t user;
} Entry;

typedef struct
{
    uint8_t volume;
    uint8_t user;
    uint8_t count;
    uint8_t selected;
    uint8_t top;
    uint8_t overflow;
    Entry files[PANE_FILES];
} Pane;

enum Key
{
    KEY_NONE = 0,
    KEY_UP = 0x100,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_PGUP,
    KEY_PGDN
};

static Cell screen[ROWS][COLS];
static Cell shown[ROWS][COLS];
static Pane panes[2];
static uint8_t active;
static char message[77];

static void out(const char *s)
{
    sys_write(FD_STDOUT, s, (uint32_t)strlen(s));
}

static void cursor(uint8_t x, uint8_t y)
{
    char seq[16];
    snprintf(seq, sizeof(seq), "\033[%u;%uH", (uint32_t)y + 1,
             (uint32_t)x + 1);
    out(seq);
}

static void terminal_start(void)
{
    out("\033[2J\033[H\033[?25l");
    memset(shown, 0xff, sizeof(shown));
}

static void terminal_stop(void)
{
    out("\033[0m\033[?25h\033[2J\033[H");
}

static void cell(int x, int y, char ch, uint8_t attr)
{
    if (x < 0 || x >= COLS || y < 0 || y >= ROWS)
        return;
    screen[y][x].ch = ch;
    screen[y][x].attr = attr;
}

static void text(int x, int y, const char *s, uint8_t attr)
{
    while (*s && x < COLS)
        cell(x++, y, *s++, attr);
}

static void fmt(int x, int y, uint8_t attr, const char *format, ...)
{
    char buf[80];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    text(x, y, buf, attr);
}

static void fill(int x, int y, int width, char ch, uint8_t attr)
{
    for (int i = 0; i < width; i++)
        cell(x + i, y, ch, attr);
}

static void desktop(void)
{
    for (int y = 0; y < ROWS; y++)
    {
        for (int x = 0; x < COLS; x++)
        {
            screen[y][x].ch = (y > 0 && ((x + y) & 1) == 0) ? '.' : ' ';
            screen[y][x].attr = NORMAL;
        }
    }

    fill(0, 0, COLS, ' ', REVERSE);
    text(2, 0, "FILE", REVERSE);
    text(12, 0, "EDIT", REVERSE);
    text(23, 0, "TOOLS", REVERSE);
    text(35, 0, "FAV", REVERSE);
    text(76, 0, "?", REVERSE);
}

static void border(int x, int y, int width, int height)
{
    cell(x, y, '+', NORMAL);
    cell(x + width - 1, y, '+', NORMAL);
    cell(x, y + height - 1, '+', NORMAL);
    cell(x + width - 1, y + height - 1, '+', NORMAL);

    for (int i = 1; i < width - 1; i++)
    {
        cell(x + i, y, '-', NORMAL);
        cell(x + i, y + height - 1, '-', NORMAL);
    }
    for (int i = 1; i < height - 1; i++)
    {
        cell(x, y + i, '|', NORMAL);
        cell(x + width - 1, y + i, '|', NORMAL);
    }
}

static void flush(void)
{
    char run[COLS + 1];

    for (uint8_t y = 0; y < ROWS; y++)
    {
        if (memcmp(screen[y], shown[y], sizeof(screen[y])) == 0)
            continue;

        cursor(0, y);
        uint8_t start = 0;
        uint8_t current_attr = 0xff;
        while (start < COLS)
        {
            uint8_t attr = screen[y][start].attr;
            uint8_t end = start;
            while (end < COLS && screen[y][end].attr == attr)
            {
                run[end - start] = screen[y][end].ch;
                end++;
            }
            run[end - start] = '\0';
            if (attr != current_attr)
            {
                out(attr == REVERSE ? "\033[7m" : "\033[0m");
                current_attr = attr;
            }
            out(run);
            start = end;
        }
        memcpy(shown[y], screen[y], sizeof(screen[y]));
    }
    out("\033[0m");
}

static void set_message(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);
}

static const char *extension(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot ? dot + 1 : "";
}

static int program(const Entry *entry)
{
    return strcasecmp(extension(entry->name), "COM") == 0;
}

static void path_for(char *path, const Pane *pane, const char *name)
{
    snprintf(path, 20, "%c%u:%s", 'A' + pane->volume,
             (uint32_t)pane->user, name);
}

static void load_pane(Pane *pane)
{
    char pattern[16];
    FileInfo info;
    int pos = 0;

    snprintf(pattern, sizeof(pattern), "%c%u:*.*", 'A' + pane->volume,
             (uint32_t)pane->user);
    pane->count = 0;
    pane->selected = 0;
    pane->top = 0;
    pane->overflow = 0;

    while (pane->count < PANE_FILES)
    {
        int next = sys_findfile(pattern, &info, (uint16_t)pos);
        if (next <= 0)
            break;

        Entry *entry = &pane->files[pane->count++];
        strncpy(entry->name, info.name, FILENAME_MAX - 1);
        entry->name[FILENAME_MAX - 1] = '\0';
        entry->size = info.size;
        entry->attrib = info.attrib;
        entry->user = info.user_area;
        pos = next;
    }

    if (pane->count == PANE_FILES &&
        sys_findfile(pattern, &info, (uint16_t)pos) > 0)
        pane->overflow = 1;
}

static void draw_pane(uint8_t pane_id, int x, int y)
{
    const int width = 36;
    const int height = 17;
    Pane *pane = &panes[pane_id];
    uint8_t title_attr = pane_id == active ? REVERSE : NORMAL;

    for (int row = 0; row < height; row++)
        fill(x, y + row, width, ' ', NORMAL);
    border(x, y, width, height);

    fill(x + 1, y + 1, width - 2, ' ', title_attr);
    cell(x + 2, y + 1, pane_id == active ? '*' : ' ', title_attr);
    fmt(x + 4, y + 1, title_attr, "%c%u:", 'A' + pane->volume,
        (uint32_t)pane->user);
    cell(x + width - 3, y + 1, 'x', title_attr);

    for (uint8_t row = 0; row < LIST_ROWS; row++)
    {
        uint8_t index = pane->top + row;
        if (index >= pane->count)
            break;

        Entry *entry = &pane->files[index];
        uint8_t attr = pane_id == active && index == pane->selected
                           ? REVERSE : NORMAL;
        fmt(x + 3, y + 3 + row, attr, "%-12s %7u %s", entry->name,
            entry->size, program(entry) ? ">" : " ");
    }

    cell(x + width - 2, y + 3, '^', NORMAL);
    cell(x + width - 2, y + height - 3, 'v', NORMAL);
    for (int row = y + 4; row < y + height - 3; row++)
        cell(x + width - 2, row, '|', NORMAL);

    if (pane->count)
    {
        Entry *entry = &pane->files[pane->selected];
        fmt(x + 2, y + height - 2, NORMAL, "%u/%u %s%s",
            (uint32_t)pane->selected + 1, (uint32_t)pane->count,
            entry->attrib & FILE_ATTR_READ_ONLY ? "RO" : "RW",
            entry->attrib & FILE_ATTR_SYSTEM ? " SYS" : "");
    }
    else
        text(x + 3, y + 5, "NO FILES", NORMAL);
}

static void draw_shell(void)
{
    desktop();
    draw_pane(0, 3, 2);
    draw_pane(1, 41, 3);

    fill(0, 21, COLS, ' ', REVERSE);
    text(1, 21,
         "TAB/LEFT/RIGHT Window  UP/DOWN Select  [ ] Volume  ENTER Open  Q Quit",
         REVERSE);
    fill(0, 22, COLS, ' ', NORMAL);
    text(1, 22, message, NORMAL);
    text(1, 23, "V View   X Run .COM   R Reload   H Help", NORMAL);
    flush();
}

static int read_key(void)
{
    int key = getchar();
    if (key != CH_ESC)
        return key;

    int second = getchar();
    if (second != '[' && second != 'O')
        return KEY_NONE;

    int code = getchar();
    switch (code)
    {
    case 'A': return KEY_UP;
    case 'B': return KEY_DOWN;
    case 'C': return KEY_RIGHT;
    case 'D': return KEY_LEFT;
    case 'H': return KEY_HOME;
    case 'F': return KEY_END;
    case '5': return getchar() == '~' ? KEY_PGUP : KEY_NONE;
    case '6': return getchar() == '~' ? KEY_PGDN : KEY_NONE;
    default: return KEY_NONE;
    }
}

static void move_selection(Pane *pane, int amount)
{
    if (!pane->count)
        return;

    int next = pane->selected + amount;
    if (next < 0)
        next = 0;
    if (next >= pane->count)
        next = pane->count - 1;
    pane->selected = (uint8_t)next;

    if (pane->selected < pane->top)
        pane->top = pane->selected;
    else if (pane->selected >= pane->top + LIST_ROWS)
        pane->top = pane->selected - LIST_ROWS + 1;
}

static uint32_t draw_page(int fd, const char *name, uint32_t offset,
                          uint32_t size, uint8_t page, uint8_t *at_end)
{
    desktop();
    const int x = 4;
    const int y = 2;
    const int width = 72;
    const int height = 20;

    for (int row = 0; row < height; row++)
        fill(x, y + row, width, ' ', NORMAL);
    border(x, y, width, height);
    fill(x + 1, y + 1, width - 2, ' ', REVERSE);
    fmt(x + 3, y + 1, REVERSE, "VIEW  %-12s", name);
    fmt(x + width - 21, y + 1, REVERSE, "PAGE %u", (uint32_t)page + 1);

    lseek(fd, offset, SEEK_SET);
    uint32_t pos = offset;
    uint8_t row = 0;
    uint8_t col = 0;
    uint8_t ch = 0;
    int n = 0;

    while (row < VIEW_ROWS && (n = read(fd, &ch, 1)) == 1)
    {
        pos++;
        if (ch == '\r')
            continue;
        if (ch == '\n')
        {
            row++;
            col = 0;
            continue;
        }
        if (col >= width - 4)
        {
            row++;
            col = 0;
            if (row >= VIEW_ROWS)
            {
                pos--;
                break;
            }
        }
        if (ch == '\t')
            ch = ' ';
        else if (ch < 0x20 || ch >= 0x7f)
            ch = '.';
        cell(x + 2 + col++, y + 2 + row, (char)ch, NORMAL);
    }

    *at_end = pos >= size || n != 1;
    fill(0, 22, COLS, ' ', REVERSE);
    text(1, 22, "DOWN/SPACE Next   UP/P Previous   HOME/B Start   Q Close", REVERSE);
    fmt(1, 23, NORMAL, "Offset %u of %u%s", offset, size,
        *at_end ? "  [END]" : "");
    flush();
    return pos;
}

static void view_entry(Pane *pane, Entry *entry)
{
    char path[20];
    path_for(path, pane, entry->name);
    int fd = open(path, "r");
    if (fd < 0)
    {
        set_message("Cannot read %s: %s", path, strerror(fd));
        return;
    }

    uint32_t offsets[PAGE_HISTORY];
    uint8_t page = 0;
    uint8_t known = 1;
    uint8_t at_end = 0;
    offsets[0] = 0;

    for (;;)
    {
        uint32_t next = draw_page(fd, path, offsets[page], entry->size,
                                  page, &at_end);
        int key = read_key();
        if (key == 'q' || key == 'Q' || key == CH_BREAK)
            break;
        if (key == KEY_HOME || key == 'b' || key == 'B')
            page = 0;
        else if (key == KEY_UP || key == KEY_PGUP || key == 'p' || key == 'P')
        {
            if (page)
                page--;
        }
        else if (key == KEY_DOWN || key == KEY_PGDN || key == ' ' ||
                 key == '\r' || key == '\n')
        {
            if (!at_end && page + 1 < PAGE_HISTORY)
            {
                if (page + 1 >= known)
                    offsets[known++] = next;
                page++;
            }
        }
    }

    close(fd);
    memset(shown, 0xff, sizeof(shown));
    set_message("Closed %s", path);
}

static void run_entry(Pane *pane, Entry *entry)
{
    if (!program(entry))
    {
        set_message("%s is not a .COM program", entry->name);
        return;
    }

    char path[20];
    path_for(path, pane, entry->name);
    char *argv[1] = {path};

    terminal_stop();
    int rc = exec(path, 1, argv);
    terminal_start();
    set_message("Cannot run %s: %s", path, strerror(rc));
}

static void help_window(void)
{
    desktop();
    const int x = 15;
    const int y = 3;
    const int width = 50;
    const int height = 16;
    for (int row = 0; row < height; row++)
        fill(x, y + row, width, ' ', NORMAL);
    border(x, y, width, height);
    fill(x + 1, y + 1, width - 2, ' ', REVERSE);
    text(x + 3, y + 1, "HELP", REVERSE);
    text(x + 3, y + 3, "TAB / LEFT / RIGHT  switch window", NORMAL);
    text(x + 3, y + 5, "UP / DOWN / PGUP     select file", NORMAL);
    text(x + 3, y + 7, "[ / ]                 change volume", NORMAL);
    text(x + 3, y + 9, "ENTER                 read or run", NORMAL);
    text(x + 3, y + 10, "V / X / R             view / run / reload", NORMAL);
    text(x + 3, y + 12, "Q                     close or quit", NORMAL);
    fill(0, 22, COLS, ' ', REVERSE);
    text(1, 22, "Press any key to close help", REVERSE);
    flush();
    read_key();
    memset(shown, 0xff, sizeof(shown));
}

int main(void)
{
    uint8_t actual_cols, actual_rows;
    sys_consize(&actual_cols, &actual_rows);
    if (actual_cols < COLS || actual_rows < ROWS)
    {
        printf("GUI requires an 80x24 terminal (found %ux%u).\n",
               (uint32_t)actual_cols, (uint32_t)actual_rows);
        return 1;
    }

    FsContext context;
    sys_getctx(&context);
    panes[0].volume = (uint8_t)context.vol_id;
    panes[0].user = context.user_area;
    panes[1].volume = (uint8_t)((context.vol_id + 1) % VOL_MAX);
    panes[1].user = context.user_area;
    load_pane(&panes[0]);
    load_pane(&panes[1]);
    set_message("ENTER reads files and runs .COM programs");

    terminal_start();
    int alive = 1;
    while (alive)
    {
        draw_shell();
        Pane *pane = &panes[active];
        int key = read_key();

        if (key == 'q' || key == 'Q' || key == CH_BREAK)
            alive = 0;
        else if (key == '\t' || key == KEY_LEFT || key == KEY_RIGHT)
            active ^= 1;
        else if (key == KEY_UP || key == 'k' || key == 'K')
            move_selection(pane, -1);
        else if (key == KEY_DOWN || key == 'j' || key == 'J')
            move_selection(pane, 1);
        else if (key == KEY_HOME)
            move_selection(pane, -(int)pane->selected);
        else if (key == KEY_END)
            move_selection(pane, pane->count);
        else if (key == KEY_PGUP)
            move_selection(pane, -LIST_ROWS);
        else if (key == KEY_PGDN)
            move_selection(pane, LIST_ROWS);
        else if (key == '[')
        {
            pane->volume = pane->volume ? pane->volume - 1 : VOL_MAX - 1;
            load_pane(pane);
        }
        else if (key == ']')
        {
            pane->volume = (uint8_t)((pane->volume + 1) % VOL_MAX);
            load_pane(pane);
        }
        else if (key == 'r' || key == 'R')
        {
            load_pane(pane);
            set_message("Reloaded %c%u:", 'A' + pane->volume,
                        (uint32_t)pane->user);
        }
        else if ((key == 'v' || key == 'V') && pane->count)
            view_entry(pane, &pane->files[pane->selected]);
        else if ((key == 'x' || key == 'X') && pane->count)
            run_entry(pane, &pane->files[pane->selected]);
        else if ((key == '\r' || key == '\n') && pane->count)
        {
            Entry *entry = &pane->files[pane->selected];
            if (program(entry))
                run_entry(pane, entry);
            else
                view_entry(pane, entry);
        }
        else if (key == 'h' || key == 'H' || key == '?')
            help_window();
    }

    terminal_stop();
    return 0;
}
