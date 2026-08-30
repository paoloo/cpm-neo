/*
 * GUI.COM — minimal cooperative desktop for CP/M Neo.
 *
 * The kernel has one fixed-address TPA, so separate .COM images cannot live
 * there concurrently.  This desktop instead runs small event-driven tasks in
 * one process.  Each task retains state, gets a short scheduler step when due,
 * and can be paused independently.  ANSI cursor positioning works in VEMU and
 * ordinary 80x24 serial terminals used by the Pico 2.
 */

#include <cpm.h>

#define MAX_COLS 80
#define MAX_ROWS 24
#define TASK_COUNT 4
#define FILE_ROWS 7

#define ATTR_NORMAL 0
#define ATTR_REVERSE 1

typedef struct
{
    char ch;
    uint8_t attr;
} Cell;

typedef struct Task Task;
typedef void (*TaskStep)(Task *task, uint32_t now);
typedef void (*TaskDraw)(const Task *task);

struct Task
{
    const char *name;
    uint8_t running;
    uint8_t spin;
    uint32_t runs;
    uint32_t period;
    uint32_t due;
    TaskStep step;
    TaskDraw draw;
};

static Cell screen[MAX_ROWS][MAX_COLS];
static Cell shown[MAX_ROWS][MAX_COLS];
static uint8_t cols;
static uint8_t rows;
static uint8_t selected;
static uint8_t dirty;

static uint32_t uptime_ms;
static SysInfo sysinfo;

static uint16_t file_pos;
static uint16_t file_count;
static uint8_t file_done;
static char file_names[FILE_ROWS][FILENAME_MAX];

static uint8_t about_phase;

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

static void term_begin(void)
{
    out("\033[2J\033[H\033[?25l");
}

static void term_end(void)
{
    out("\033[0m\033[?25h\033[2J\033[H");
}

static void clear_screen_model(void)
{
    for (uint8_t y = 0; y < rows; y++)
    {
        for (uint8_t x = 0; x < cols; x++)
        {
            screen[y][x].ch = ' ';
            screen[y][x].attr = ATTR_NORMAL;
        }
    }
}

static void put_cell(int x, int y, char ch, uint8_t attr)
{
    if (x < 0 || y < 0 || x >= cols || y >= rows)
        return;

    screen[y][x].ch = ch;
    screen[y][x].attr = attr;
}

static void put_text(int x, int y, const char *text, uint8_t attr)
{
    while (*text && x < cols)
        put_cell(x++, y, *text++, attr);
}

static void put_fmt(int x, int y, uint8_t attr, const char *fmt, ...)
{
    char buf[80];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    put_text(x, y, buf, attr);
}

static void fill_row(int y, char ch, uint8_t attr)
{
    for (uint8_t x = 0; x < cols; x++)
        put_cell(x, y, ch, attr);
}

static void box(int x, int y, int w, int h, const char *title)
{
    if (w < 2 || h < 2)
        return;

    put_cell(x, y, '+', ATTR_NORMAL);
    put_cell(x + w - 1, y, '+', ATTR_NORMAL);
    put_cell(x, y + h - 1, '+', ATTR_NORMAL);
    put_cell(x + w - 1, y + h - 1, '+', ATTR_NORMAL);

    for (int i = 1; i < w - 1; i++)
    {
        put_cell(x + i, y, '-', ATTR_NORMAL);
        put_cell(x + i, y + h - 1, '-', ATTR_NORMAL);
    }

    for (int i = 1; i < h - 1; i++)
    {
        put_cell(x, y + i, '|', ATTR_NORMAL);
        put_cell(x + w - 1, y + i, '|', ATTR_NORMAL);
    }

    if (title)
    {
        put_cell(x + 2, y, '[', ATTR_NORMAL);
        put_text(x + 3, y, title, ATTR_NORMAL);
        put_cell(x + 3 + (int)strlen(title), y, ']', ATTR_NORMAL);
    }
}

static void flush_screen(void)
{
    char rowbuf[MAX_COLS + 1];

    for (uint8_t y = 0; y < rows; y++)
    {
        int changed = 0;
        for (uint8_t x = 0; x < cols; x++)
        {
            if (screen[y][x].ch != shown[y][x].ch ||
                screen[y][x].attr != shown[y][x].attr)
            {
                changed = 1;
                break;
            }
        }

        if (!changed)
            continue;

        cursor(0, y);
        uint8_t attr = 0xff;
        uint8_t start = 0;

        while (start < cols)
        {
            uint8_t next_attr = screen[y][start].attr;
            uint8_t end = start;
            while (end < cols && screen[y][end].attr == next_attr)
            {
                rowbuf[end - start] = screen[y][end].ch;
                end++;
            }
            rowbuf[end - start] = '\0';

            if (next_attr != attr)
            {
                out(next_attr == ATTR_REVERSE ? "\033[7m" : "\033[0m");
                attr = next_attr;
            }
            out(rowbuf);
            start = end;
        }

        memcpy(shown[y], screen[y], (size_t)cols * sizeof(Cell));
    }

    out("\033[0m");
    dirty = 0;
}

static void step_clock(Task *task, uint32_t now)
{
    (void)task;
    uptime_ms = now;
    dirty = 1;
}

static void step_system(Task *task, uint32_t now)
{
    (void)task;
    (void)now;
    sys_info(&sysinfo);
    dirty = 1;
}

static void step_files(Task *task, uint32_t now)
{
    (void)task;
    (void)now;

    if (file_done)
    {
        file_done = 0;
        file_pos = 0;
        file_count = 0;
        memset(file_names, 0, sizeof(file_names));
    }

    FileInfo fi;
    int next = sys_findfile("*.*", &fi, file_pos);
    if (next > 0)
    {
        uint16_t slot = file_count % FILE_ROWS;
        strncpy(file_names[slot], fi.name, FILENAME_MAX - 1);
        file_names[slot][FILENAME_MAX - 1] = '\0';
        file_count++;
        file_pos = (uint16_t)next;
    }
    else
    {
        file_done = 1;
        task->due = now + 4000;
    }
    dirty = 1;
}

static void step_about(Task *task, uint32_t now)
{
    (void)task;
    (void)now;
    about_phase = (uint8_t)((about_phase + 1) & 3);
    dirty = 1;
}

static void draw_clock(const Task *task)
{
    (void)task;
    uint32_t seconds = uptime_ms / 1000;
    uint32_t hours = seconds / 3600;
    uint32_t mins = (seconds / 60) % 60;
    uint32_t secs = seconds % 60;

    put_text(29, 5, "COOPERATIVE DESKTOP CLOCK", ATTR_NORMAL);
    put_fmt(34, 8, ATTR_REVERSE, "  %02u:%02u:%02u  ", hours, mins, secs);
    put_text(29, 11, "This task updates while other", ATTR_NORMAL);
    put_text(29, 12, "windows are selected.", ATTR_NORMAL);
}

static void draw_system(const Task *task)
{
    (void)task;
    put_fmt(27, 5, ATTR_NORMAL, "Platform : %s", sysinfo.platform);
    put_fmt(27, 7, ATTR_NORMAL, "TPA      : %u KB", sysinfo.tpa);
    put_fmt(27, 8, ATTR_NORMAL, "Disk     : %u KB", sysinfo.disk_size_kb);
    put_fmt(27, 9, ATTR_NORMAL, "Free     : %u KB", sysinfo.disk_unalloc_kb);
    put_fmt(27, 11, ATTR_NORMAL, "OS/K/CCP : %x / %x / %x",
            (uint32_t)sysinfo.os_version, (uint32_t)sysinfo.kern_version,
            (uint32_t)sysinfo.ccp_version);
    put_fmt(27, 13, ATTR_NORMAL, "Volumes  : A:%c B:%c C:%c D:%c",
            sysinfo.vol_mounted[0] ? 'Y' : '-',
            sysinfo.vol_mounted[1] ? 'Y' : '-',
            sysinfo.vol_mounted[2] ? 'Y' : '-',
            sysinfo.vol_mounted[3] ? 'Y' : '-');
}

static void draw_files(const Task *task)
{
    (void)task;
    put_fmt(27, 4, ATTR_NORMAL, "Incremental scan: %u files %s", file_count,
            file_done ? "(complete)" : "(running)");

    uint16_t visible = file_count < FILE_ROWS ? file_count : FILE_ROWS;
    uint16_t first = file_count > FILE_ROWS ? file_count - FILE_ROWS : 0;
    for (uint16_t i = 0; i < visible; i++)
    {
        uint16_t logical = first + i;
        uint16_t slot = logical % FILE_ROWS;
        put_fmt(29, 6 + i, ATTR_NORMAL, "%2u  %-12s", logical + 1,
                file_names[slot]);
    }

    put_text(27, 15, "One directory entry per scheduler step.", ATTR_NORMAL);
}

static void draw_about(const Task *task)
{
    static const char spin[] = "|/-\\";
    (void)task;

    put_text(27, 5, "CP/M Neo GUI", ATTR_REVERSE);
    put_text(27, 7, "Inspired by NABU Cloud CP/M's", ATTR_NORMAL);
    put_text(27, 8, "simple title/list/detail interface.", ATTR_NORMAL);
    put_text(27, 10, "No graphics driver. No preemption.", ATTR_NORMAL);
    put_text(27, 11, "Just ANSI text and small event tasks.", ATTR_NORMAL);
    put_fmt(27, 14, ATTR_NORMAL, "Desktop event loop is alive: %c",
            spin[about_phase]);
}

static Task tasks[TASK_COUNT] = {
    {"CLOCK", 1, 0, 0, 250, 0, step_clock, draw_clock},
    {"SYSTEM", 1, 0, 0, 1000, 0, step_system, draw_system},
    {"FILES", 1, 0, 0, 120, 0, step_files, draw_files},
    {"ABOUT", 1, 0, 0, 500, 0, step_about, draw_about},
};

static void compose(uint32_t now)
{
    static const char spinner[] = "|/-\\";
    clear_screen_model();

    fill_row(0, ' ', ATTR_REVERSE);
    put_text(2, 0, "CP/M NEO DESKTOP", ATTR_REVERSE);
    put_fmt(cols - 19, 0, ATTR_REVERSE, "UP %6u SEC", now / 1000);

    box(0, 2, 23, 17, "TASKS");
    box(23, 2, cols - 23, 17, tasks[selected].name);
    box(0, 19, cols, rows - 19, "CONTROLS");

    for (uint8_t i = 0; i < TASK_COUNT; i++)
    {
        uint8_t attr = i == selected ? ATTR_REVERSE : ATTR_NORMAL;
        put_fmt(2, 4 + i * 3, attr, "%c %-7s %c %c",
                i == selected ? '>' : ' ', tasks[i].name,
                tasks[i].running ? spinner[tasks[i].spin] : '-',
                tasks[i].running ? 'R' : 'P');
        put_fmt(4, 5 + i * 3, ATTR_NORMAL, "steps %6u", tasks[i].runs);
    }

    tasks[selected].draw(&tasks[selected]);
    put_text(2, 21, "UP/DOWN or J/K select   ENTER/SPACE pause   R resume all   Q quit",
             ATTR_NORMAL);
    put_text(2, 22, "All RUN tasks continue updating when their window is not selected.",
             ATTR_NORMAL);
}

static int read_key(void)
{
    static uint8_t escape_state;

    if (!peekchar())
        return 0;

    int c = getchar();
    if (escape_state == 0)
    {
        if (c == CH_ESC)
        {
            escape_state = 1;
            return 0;
        }
        return c;
    }

    if (escape_state == 1)
    {
        escape_state = (c == '[' || c == 'O') ? 2 : 0;
        return 0;
    }

    escape_state = 0;
    switch (c)
    {
    case 'A': return 'k';
    case 'B': return 'j';
    default: return 0;
    }
}

static int handle_key(int key, uint32_t now)
{
    if (key == 'q' || key == 'Q' || key == CH_BREAK)
        return 0;

    if (key == 'k' || key == 'K')
    {
        selected = selected ? selected - 1 : TASK_COUNT - 1;
        dirty = 1;
    }
    else if (key == 'j' || key == 'J' || key == '\t')
    {
        selected = (uint8_t)((selected + 1) % TASK_COUNT);
        dirty = 1;
    }
    else if (key == ' ' || key == '\r' || key == '\n')
    {
        tasks[selected].running = !tasks[selected].running;
        tasks[selected].due = now;
        dirty = 1;
    }
    else if (key == 'r' || key == 'R')
    {
        for (uint8_t i = 0; i < TASK_COUNT; i++)
        {
            tasks[i].running = 1;
            tasks[i].due = now;
        }
        dirty = 1;
    }

    return 1;
}

static void schedule(uint32_t now)
{
    for (uint8_t i = 0; i < TASK_COUNT; i++)
    {
        Task *task = &tasks[i];
        if (!task->running || (int32_t)(now - task->due) < 0)
            continue;

        task->step(task, now);
        task->runs++;
        task->spin = (uint8_t)((task->spin + 1) & 3);
        if ((int32_t)(task->due - now) <= 0)
            task->due = now + task->period;
    }
}

int main(void)
{
    sys_consize(&cols, &rows);
    if (cols > MAX_COLS)
        cols = MAX_COLS;
    if (rows > MAX_ROWS)
        rows = MAX_ROWS;

    if (cols < 70 || rows < 24)
    {
        printf("GUI requires a 70x24 or larger terminal (found %ux%u).\n",
               (uint32_t)cols, (uint32_t)rows);
        return 1;
    }

    memset(shown, 0xff, sizeof(shown));
    sys_info(&sysinfo);
    uint32_t now = sys_time();
    for (uint8_t i = 0; i < TASK_COUNT; i++)
        tasks[i].due = now;

    dirty = 1;
    term_begin();

    int alive = 1;
    while (alive)
    {
        now = sys_time();
        schedule(now);

        int key = read_key();
        if (key)
            alive = handle_key(key, now);

        if (dirty)
        {
            compose(now);
            flush_screen();
        }
    }

    term_end();
    return 0;
}
