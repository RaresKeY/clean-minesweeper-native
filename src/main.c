#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <conio.h>
#include <io.h>
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif
#else
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifdef CLEAN_MINES_WITH_SDL
#define SDL_MAIN_HANDLED
#include <SDL.h>
#endif

#define APP_NAME "clean-mines"
#define APP_VERSION "0.1.0"

typedef enum {
    GAME_READY = 0,
    GAME_PLAYING,
    GAME_WON,
    GAME_LOST
} GameStatus;

typedef enum {
    THEME_LIGHT = 0,
    THEME_DARK
} ThemeMode;

typedef enum {
    SIZE_SMALL = 0,
    SIZE_MEDIUM,
    SIZE_LARGE,
    SIZE_LEVEL_COUNT
} SizeLevel;

typedef enum {
    MINE_EASY = 0,
    MINE_NORMAL,
    MINE_HARD,
    MINE_DIFFICULTY_COUNT
} MineDifficulty;

typedef struct {
    unsigned char mine;
    unsigned char adjacent;
    unsigned char revealed;
    unsigned char flagged;
    unsigned char hit;
    unsigned char wrong;
} Cell;

typedef struct {
    int width;
    int height;
    int mines;
    int first_reveal;
    int revealed_safe;
    GameStatus status;
    uint32_t rng;
    Cell *cells;
} Game;

typedef struct {
    int width;
    int height;
    int mines;
} Difficulty;

typedef struct {
    SizeLevel level;
    MineDifficulty difficulty;
    int custom;
    Difficulty config;
} BoardSelection;

static const Difficulty PRESET_CONFIGS[SIZE_LEVEL_COUNT][MINE_DIFFICULTY_COUNT] = {
    {
        {9, 9, 8},
        {9, 9, 10},
        {9, 9, 14}
    },
    {
        {16, 16, 30},
        {16, 16, 40},
        {16, 16, 55}
    },
    {
        {30, 16, 75},
        {30, 16, 99},
        {30, 16, 120}
    }
};

static const char *SIZE_LEVEL_NAMES[SIZE_LEVEL_COUNT] = {"small", "medium", "large"};
static const char *MINE_DIFFICULTY_NAMES[MINE_DIFFICULTY_COUNT] = {"easy", "normal", "hard"};

static Difficulty preset_config(SizeLevel level, MineDifficulty difficulty) {
    if (level < 0 || level >= SIZE_LEVEL_COUNT) {
        level = SIZE_SMALL;
    }
    if (difficulty < 0 || difficulty >= MINE_DIFFICULTY_COUNT) {
        difficulty = MINE_NORMAL;
    }
    return PRESET_CONFIGS[level][difficulty];
}

static const char *size_level_name(SizeLevel level) {
    if (level < 0 || level >= SIZE_LEVEL_COUNT) {
        return "custom";
    }
    return SIZE_LEVEL_NAMES[level];
}

static const char *mine_difficulty_name(MineDifficulty difficulty) {
    if (difficulty < 0 || difficulty >= MINE_DIFFICULTY_COUNT) {
        return "custom";
    }
    return MINE_DIFFICULTY_NAMES[difficulty];
}

static void selection_set_preset(BoardSelection *selection, SizeLevel level, MineDifficulty difficulty) {
    selection->level = level;
    selection->difficulty = difficulty;
    selection->custom = 0;
    selection->config = preset_config(level, difficulty);
}

static void selection_set_custom(BoardSelection *selection, int width, int height, int mines) {
    selection->custom = 1;
    selection->config.width = width;
    selection->config.height = height;
    selection->config.mines = mines;
}

static void selection_label(const BoardSelection *selection, char *out, size_t out_size) {
    if (selection->custom) {
        snprintf(out, out_size, "custom %dx%d/%d",
                 selection->config.width, selection->config.height, selection->config.mines);
        return;
    }
    snprintf(out, out_size, "%s %s", size_level_name(selection->level),
             mine_difficulty_name(selection->difficulty));
}

static void lowercase_text(char *text) {
    for (int i = 0; text[i] != '\0'; i++) {
        text[i] = (char)tolower((unsigned char)text[i]);
    }
}

static int parse_size_level_arg(const char *value, SizeLevel *out) {
    if (strcmp(value, "small") == 0 || strcmp(value, "s") == 0) {
        *out = SIZE_SMALL;
        return 1;
    }
    if (strcmp(value, "medium") == 0 || strcmp(value, "m") == 0) {
        *out = SIZE_MEDIUM;
        return 1;
    }
    if (strcmp(value, "large") == 0 || strcmp(value, "l") == 0) {
        *out = SIZE_LARGE;
        return 1;
    }
    return 0;
}

static int parse_mine_difficulty_arg(const char *value, MineDifficulty *out) {
    if (strcmp(value, "easy") == 0 || strcmp(value, "e") == 0) {
        *out = MINE_EASY;
        return 1;
    }
    if (strcmp(value, "normal") == 0 || strcmp(value, "n") == 0) {
        *out = MINE_NORMAL;
        return 1;
    }
    if (strcmp(value, "hard") == 0 || strcmp(value, "h") == 0) {
        *out = MINE_HARD;
        return 1;
    }
    return 0;
}

static void *xcalloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (ptr == NULL) {
        fprintf(stderr, "allocation failed\n");
        exit(2);
    }
    return ptr;
}

static uint32_t rng_next(Game *game) {
    uint32_t x = game->rng;
    if (x == 0u) {
        x = 0x6d2b79f5u;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    game->rng = x;
    return x;
}

static int in_bounds(const Game *game, int x, int y) {
    return x >= 0 && y >= 0 && x < game->width && y < game->height;
}

static int index_of(const Game *game, int x, int y) {
    return y * game->width + x;
}

static Cell *cell_at(Game *game, int x, int y) {
    if (!in_bounds(game, x, y)) {
        return NULL;
    }
    return &game->cells[index_of(game, x, y)];
}

static const Cell *const_cell_at(const Game *game, int x, int y) {
    if (!in_bounds(game, x, y)) {
        return NULL;
    }
    return &game->cells[index_of(game, x, y)];
}

static int count_flags(const Game *game) {
    int count = 0;
    int total = game->width * game->height;
    for (int i = 0; i < total; i++) {
        if (game->cells[i].flagged) {
            count++;
        }
    }
    return count;
}

static int count_adjacent_mines(const Game *game, int x, int y) {
    int count = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            const Cell *cell = const_cell_at(game, x + dx, y + dy);
            if (cell != NULL && cell->mine) {
                count++;
            }
        }
    }
    return count;
}

static int validate_config(int width, int height, int mines) {
    if (width < 2 || height < 2 || width > 60 || height > 40) {
        return 0;
    }
    if (mines < 1 || mines >= width * height) {
        return 0;
    }
    return 1;
}

static void game_free(Game *game) {
    free(game->cells);
    game->cells = NULL;
}

static void game_init(Game *game, int width, int height, int mines, uint32_t seed) {
    if (!validate_config(width, height, mines)) {
        fprintf(stderr, "invalid board: width=%d height=%d mines=%d\n", width, height, mines);
        exit(2);
    }
    memset(game, 0, sizeof(*game));
    game->width = width;
    game->height = height;
    game->mines = mines;
    game->first_reveal = 1;
    game->status = GAME_READY;
    game->rng = seed == 0u ? (uint32_t)time(NULL) : seed;
    game->cells = (Cell *)xcalloc((size_t)width * (size_t)height, sizeof(Cell));
}

static int is_complete(const Game *game) {
    return game->status == GAME_WON || game->status == GAME_LOST;
}

static void mark_lost(Game *game) {
    int total = game->width * game->height;
    game->status = GAME_LOST;
    for (int i = 0; i < total; i++) {
        if (game->cells[i].mine) {
            game->cells[i].revealed = 1;
        }
        if (game->cells[i].flagged && !game->cells[i].mine) {
            game->cells[i].wrong = 1;
        }
    }
}

static void check_win(Game *game) {
    int safe_total = game->width * game->height - game->mines;
    if (game->revealed_safe != safe_total) {
        return;
    }
    game->status = GAME_WON;
    for (int i = 0; i < game->width * game->height; i++) {
        if (game->cells[i].mine) {
            game->cells[i].flagged = 1;
        }
    }
}

static void place_mines(Game *game, int safe_x, int safe_y) {
    int total = game->width * game->height;
    int *safe = (int *)xcalloc((size_t)total, sizeof(int));
    int *candidates = (int *)xcalloc((size_t)total, sizeof(int));
    int safe_count = 0;
    int candidate_count = 0;

    safe[index_of(game, safe_x, safe_y)] = 1;
    safe_count++;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = safe_x + dx;
            int ny = safe_y + dy;
            if (!in_bounds(game, nx, ny)) {
                continue;
            }
            int idx = index_of(game, nx, ny);
            if (!safe[idx]) {
                safe[idx] = 1;
                safe_count++;
            }
        }
    }

    if (total - safe_count < game->mines) {
        memset(safe, 0, (size_t)total * sizeof(int));
        safe[index_of(game, safe_x, safe_y)] = 1;
    }

    for (int i = 0; i < total; i++) {
        if (!safe[i]) {
            candidates[candidate_count++] = i;
        }
    }

    for (int i = candidate_count - 1; i > 0; i--) {
        int swap = (int)(rng_next(game) % (uint32_t)(i + 1));
        int tmp = candidates[i];
        candidates[i] = candidates[swap];
        candidates[swap] = tmp;
    }

    for (int i = 0; i < game->mines; i++) {
        game->cells[candidates[i]].mine = 1;
    }

    for (int y = 0; y < game->height; y++) {
        for (int x = 0; x < game->width; x++) {
            cell_at(game, x, y)->adjacent = (unsigned char)count_adjacent_mines(game, x, y);
        }
    }

    free(candidates);
    free(safe);
}

static void ensure_started(Game *game, int x, int y) {
    if (!game->first_reveal) {
        if (game->status == GAME_READY) {
            game->status = GAME_PLAYING;
        }
        return;
    }
    place_mines(game, x, y);
    game->first_reveal = 0;
    game->status = GAME_PLAYING;
}

static int reveal_safe_from(Game *game, int start_index) {
    int total = game->width * game->height;
    int *stack = (int *)xcalloc((size_t)total, sizeof(int));
    unsigned char *queued = (unsigned char *)xcalloc((size_t)total, sizeof(unsigned char));
    int top = 0;
    int revealed = 0;
    stack[top++] = start_index;
    queued[start_index] = 1;

    while (top > 0) {
        int idx = stack[--top];
        Cell *cell = &game->cells[idx];
        if (cell->revealed || cell->mine || cell->flagged) {
            continue;
        }
        cell->revealed = 1;
        revealed++;
        game->revealed_safe++;

        if (cell->adjacent != 0) {
            continue;
        }

        int x = idx % game->width;
        int y = idx / game->width;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                int nx = x + dx;
                int ny = y + dy;
                if (!in_bounds(game, nx, ny)) {
                    continue;
                }
                int nidx = index_of(game, nx, ny);
                Cell *neighbor = &game->cells[nidx];
                if (!queued[nidx] && !neighbor->revealed && !neighbor->mine && !neighbor->flagged) {
                    queued[nidx] = 1;
                    stack[top++] = nidx;
                }
            }
        }
    }

    free(queued);
    free(stack);
    return revealed;
}

static int game_reveal(Game *game, int x, int y) {
    Cell *cell = cell_at(game, x, y);
    if (cell == NULL || is_complete(game) || cell->revealed || cell->flagged) {
        return 0;
    }

    ensure_started(game, x, y);
    if (cell->mine) {
        cell->revealed = 1;
        cell->hit = 1;
        mark_lost(game);
        return 1;
    }

    int changed = reveal_safe_from(game, index_of(game, x, y));
    check_win(game);
    return changed;
}

static int game_toggle_flag(Game *game, int x, int y) {
    Cell *cell = cell_at(game, x, y);
    if (cell == NULL || is_complete(game) || cell->revealed) {
        return 0;
    }
    cell->flagged = (unsigned char)!cell->flagged;
    return 1;
}

static int game_chord(Game *game, int x, int y) {
    Cell *cell = cell_at(game, x, y);
    if (cell == NULL || is_complete(game) || !cell->revealed || cell->adjacent == 0) {
        return 0;
    }

    int flags = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            Cell *neighbor = cell_at(game, x + dx, y + dy);
            if (neighbor != NULL && neighbor->flagged) {
                flags++;
            }
        }
    }

    if (flags != cell->adjacent) {
        return 0;
    }

    int changed = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            int nx = x + dx;
            int ny = y + dy;
            Cell *neighbor = cell_at(game, nx, ny);
            if (neighbor == NULL || neighbor->revealed || neighbor->flagged) {
                continue;
            }
            if (neighbor->mine) {
                neighbor->revealed = 1;
                neighbor->hit = 1;
                mark_lost(game);
                return 1;
            }
            changed += reveal_safe_from(game, index_of(game, nx, ny));
        }
    }
    check_win(game);
    return changed > 0;
}

static char visible_char(const Game *game, const Cell *cell) {
    if (game->status == GAME_LOST && cell->wrong) {
        return 'x';
    }
    if (!cell->revealed) {
        return cell->flagged ? 'F' : '.';
    }
    if (cell->mine) {
        return cell->hit ? '!' : '*';
    }
    if (cell->adjacent == 0) {
        return ' ';
    }
    return (char)('0' + cell->adjacent);
}

static void render(const Game *game, const BoardSelection *selection) {
    char label[64];
    selection_label(selection, label, sizeof(label));
    printf("\n");
    printf("Board: %s  Mines: %d  Flags: %d  State: ", label, game->mines, count_flags(game));
    if (game->status == GAME_READY) {
        printf("ready\n");
    } else if (game->status == GAME_PLAYING) {
        printf("playing\n");
    } else if (game->status == GAME_WON) {
        printf("won\n");
    } else {
        printf("lost\n");
    }

    printf("    ");
    for (int x = 0; x < game->width; x++) {
        printf(" %2d", x + 1);
    }
    printf("\n");

    for (int y = 0; y < game->height; y++) {
        printf("%3d ", y + 1);
        for (int x = 0; x < game->width; x++) {
            printf("  %c", visible_char(game, const_cell_at(game, x, y)));
        }
        printf("\n");
    }
}

static void print_help(void) {
    printf("Commands:\n");
    printf("  r x y    reveal cell\n");
    printf("  f x y    flag/unflag cell\n");
    printf("  c x y    chord revealed number\n");
    printf("  n        restart current board\n");
    printf("  n s e    new board: size s/m/l, difficulty e/n/h\n");
    printf("  s s      select size level: small, medium, large\n");
    printf("  d h      select difficulty: easy, normal, hard\n");
    printf("  h        help\n");
    printf("  q        quit\n");
}

static int parse_xy(const char *line, int *x, int *y) {
    int px = 0;
    int py = 0;
    if (sscanf(line, " %*c %d %d", &px, &py) != 2) {
        return 0;
    }
    *x = px - 1;
    *y = py - 1;
    return 1;
}

static void reset_to_selection(Game *game, const BoardSelection *selection, uint32_t seed) {
    game_free(game);
    game_init(game, selection->config.width, selection->config.height, selection->config.mines, seed);
}

static int parse_new_selection(const char *line, BoardSelection *selection) {
    char first[32] = "";
    char second[32] = "";
    int count = sscanf(line, " %*s %31s %31s", first, second);
    if (count <= 0) {
        return 1;
    }

    lowercase_text(first);
    lowercase_text(second);

    if (count == 1) {
        SizeLevel level = selection->level;
        MineDifficulty difficulty = selection->difficulty;
        if (strcmp(first, "b") == 0 || strcmp(first, "beginner") == 0) {
            selection_set_preset(selection, SIZE_SMALL, MINE_NORMAL);
            return 1;
        }
        if (strcmp(first, "i") == 0 || strcmp(first, "intermediate") == 0) {
            selection_set_preset(selection, SIZE_MEDIUM, MINE_NORMAL);
            return 1;
        }
        if (strcmp(first, "e") == 0 || strcmp(first, "x") == 0 || strcmp(first, "expert") == 0) {
            selection_set_preset(selection, SIZE_LARGE, MINE_NORMAL);
            return 1;
        }
        if (parse_size_level_arg(first, &level)) {
            selection_set_preset(selection, level, difficulty);
            return 1;
        }
        if (parse_mine_difficulty_arg(first, &difficulty)) {
            selection_set_preset(selection, level, difficulty);
            return 1;
        }
        return 0;
    }

    SizeLevel level = selection->level;
    MineDifficulty difficulty = selection->difficulty;
    if (!parse_size_level_arg(first, &level) || !parse_mine_difficulty_arg(second, &difficulty)) {
        return 0;
    }
    selection_set_preset(selection, level, difficulty);
    return 1;
}

typedef enum {
    TERM_KEY_NONE = 0,
    TERM_KEY_QUIT,
    TERM_KEY_UP,
    TERM_KEY_DOWN,
    TERM_KEY_LEFT,
    TERM_KEY_RIGHT,
    TERM_KEY_REVEAL,
    TERM_KEY_FLAG,
    TERM_KEY_CHORD,
    TERM_KEY_RESTART,
    TERM_KEY_HELP,
    TERM_KEY_SMALL,
    TERM_KEY_MEDIUM,
    TERM_KEY_LARGE,
    TERM_KEY_EASY,
    TERM_KEY_NORMAL,
    TERM_KEY_HARD
} TerminalKey;

typedef struct {
    int active;
#if defined(_WIN32)
    HANDLE input;
    HANDLE output;
    DWORD input_mode;
    DWORD output_mode;
    int restore_input;
    int restore_output;
#else
    struct termios original;
    int restore_terminal;
#endif
} TerminalSession;

static const char *game_status_text(GameStatus status) {
    if (status == GAME_READY) {
        return "ready";
    }
    if (status == GAME_PLAYING) {
        return "playing";
    }
    if (status == GAME_WON) {
        return "won";
    }
    return "lost";
}

static int terminal_can_control(void) {
#if defined(_WIN32)
    return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
#else
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
}

static void terminal_sleep_ms(int milliseconds) {
#if defined(_WIN32)
    Sleep((DWORD)milliseconds);
#else
    struct timeval delay;
    delay.tv_sec = milliseconds / 1000;
    delay.tv_usec = (milliseconds % 1000) * 1000;
    select(0, NULL, NULL, NULL, &delay);
#endif
}

static void terminal_get_size(int *cols, int *rows) {
    *cols = 80;
    *rows = 24;
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(output, &info)) {
        *cols = info.srWindow.Right - info.srWindow.Left + 1;
        *rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
#else
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0 && size.ws_row > 0) {
        *cols = size.ws_col;
        *rows = size.ws_row;
    }
#endif
}

static int terminal_enter(TerminalSession *session) {
    memset(session, 0, sizeof(*session));
#if defined(_WIN32)
    session->input = GetStdHandle(STD_INPUT_HANDLE);
    session->output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (session->input == INVALID_HANDLE_VALUE || session->output == INVALID_HANDLE_VALUE) {
        return 0;
    }
    if (GetConsoleMode(session->input, &session->input_mode)) {
        session->restore_input = 1;
    }
    if (GetConsoleMode(session->output, &session->output_mode)) {
        DWORD mode = session->output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
        if (SetConsoleMode(session->output, mode)) {
            session->restore_output = 1;
        }
    }
#else
    if (tcgetattr(STDIN_FILENO, &session->original) != 0) {
        return 0;
    }
    struct termios raw = session->original;
    raw.c_iflag &= (tcflag_t)~(ICRNL | IXON);
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return 0;
    }
    session->restore_terminal = 1;
#endif
    session->active = 1;
    printf("\033[?1049h\033[?25l\033[2J\033[H");
    fflush(stdout);
    return 1;
}

static void terminal_leave(TerminalSession *session) {
    if (!session->active) {
        return;
    }
    printf("\033[0m\033[?25h\033[?1049l");
    fflush(stdout);
#if defined(_WIN32)
    if (session->restore_input) {
        SetConsoleMode(session->input, session->input_mode);
    }
    if (session->restore_output) {
        SetConsoleMode(session->output, session->output_mode);
    }
#else
    if (session->restore_terminal) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &session->original);
    }
#endif
    session->active = 0;
}

static TerminalKey terminal_read_key(void) {
#if defined(_WIN32)
    if (!_kbhit()) {
        return TERM_KEY_NONE;
    }
    int ch = _getch();
    if (ch == 0 || ch == 224) {
        int ext = _getch();
        if (ext == 72) {
            return TERM_KEY_UP;
        }
        if (ext == 80) {
            return TERM_KEY_DOWN;
        }
        if (ext == 75) {
            return TERM_KEY_LEFT;
        }
        if (ext == 77) {
            return TERM_KEY_RIGHT;
        }
        return TERM_KEY_NONE;
    }
#else
    unsigned char ch = 0;
    ssize_t read_count = read(STDIN_FILENO, &ch, 1);
    if (read_count <= 0) {
        return TERM_KEY_NONE;
    }
    if (ch == 27) {
        unsigned char seq[2] = {0, 0};
        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            return TERM_KEY_QUIT;
        }
        if (seq[0] != '[' && seq[0] != 'O') {
            return TERM_KEY_NONE;
        }
        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            return TERM_KEY_NONE;
        }
        if (seq[1] == 'A') {
            return TERM_KEY_UP;
        }
        if (seq[1] == 'B') {
            return TERM_KEY_DOWN;
        }
        if (seq[1] == 'C') {
            return TERM_KEY_RIGHT;
        }
        if (seq[1] == 'D') {
            return TERM_KEY_LEFT;
        }
        return TERM_KEY_NONE;
    }
#endif

    if (ch == 3 || ch == 'q' || ch == 'Q') {
        return TERM_KEY_QUIT;
    }
    if (ch == '\r' || ch == '\n' || ch == ' ') {
        return TERM_KEY_REVEAL;
    }
    ch = (unsigned char)tolower(ch);
    if (ch == 'f') {
        return TERM_KEY_FLAG;
    }
    if (ch == 'c') {
        return TERM_KEY_CHORD;
    }
    if (ch == 'r') {
        return TERM_KEY_RESTART;
    }
    if (ch == '?') {
        return TERM_KEY_HELP;
    }
    if (ch == '1' || ch == 's' || ch == 'b') {
        return TERM_KEY_SMALL;
    }
    if (ch == '2' || ch == 'm' || ch == 'i') {
        return TERM_KEY_MEDIUM;
    }
    if (ch == '3' || ch == 'l' || ch == 'x') {
        return TERM_KEY_LARGE;
    }
    if (ch == 'e') {
        return TERM_KEY_EASY;
    }
    if (ch == 'n') {
        return TERM_KEY_NORMAL;
    }
    if (ch == 'h') {
        return TERM_KEY_HARD;
    }
    return TERM_KEY_NONE;
}

static const char *terminal_cell_color(char value) {
    if (value == '1') {
        return "\033[34m";
    }
    if (value == '2') {
        return "\033[32m";
    }
    if (value == '3') {
        return "\033[31m";
    }
    if (value == '4') {
        return "\033[35m";
    }
    if (value == '5') {
        return "\033[91m";
    }
    if (value == '6') {
        return "\033[36m";
    }
    if (value == 'F') {
        return "\033[35m";
    }
    if (value == '!' || value == '*' || value == 'x') {
        return "\033[31m";
    }
    return "";
}

static void terminal_print_cell(char value, int width, int focused, int blink_on) {
    const char *color = terminal_cell_color(value);
    if (focused && blink_on) {
        printf("\033[7m");
    } else if (focused) {
        printf("\033[1m");
    }
    if (color[0] != '\0') {
        printf("%s", color);
    }

    if (width == 3) {
        printf(" %c ", value);
    } else {
        printf(" %c", value);
    }

    if (focused || color[0] != '\0') {
        printf("\033[0m");
    }
}

static void terminal_render_controller(const Game *game, const BoardSelection *selection,
                                       int focus_x, int focus_y, int blink_on,
                                       int show_help, const char *message) {
    int cols = 0;
    int rows = 0;
    terminal_get_size(&cols, &rows);
    int cell_width = (4 + game->width * 3 <= cols) ? 3 : 2;
    int show_columns = cell_width == 3;
    char label[64];
    selection_label(selection, label, sizeof(label));

    printf("\033[H\033[2J");
    printf("clean-mines %s  %s  %s  mines %d  flags %d  focus %d,%d\n",
           APP_VERSION, label, game_status_text(game->status), game->mines,
           count_flags(game), focus_x + 1, focus_y + 1);
    printf("Arrows move  Space/Enter reveal  f flag  c chord  r restart  ? help  q quit\n");
    printf("Size: 1/s small  2/m medium  3/l large    Difficulty: e easy  n normal  h hard\n");
    if (message != NULL && message[0] != '\0') {
        printf("%s\n", message);
    } else {
        printf("\n");
    }

    if (show_columns) {
        printf("    ");
        for (int x = 0; x < game->width; x++) {
            printf("%3d", x + 1);
        }
        printf("\n");
    }

    for (int y = 0; y < game->height; y++) {
        printf("%3d ", y + 1);
        for (int x = 0; x < game->width; x++) {
            char value = visible_char(game, const_cell_at(game, x, y));
            terminal_print_cell(value, cell_width, x == focus_x && y == focus_y, blink_on);
        }
        printf("\n");
    }

    if (show_help) {
        printf("\n");
        printf("Controls are immediate; no command prompt is used in this screen.\n");
        printf("The flashing reverse-video cell is the current target.\n");
        printf("Use --width/--height/--mines for a custom board; preset keys return to preset boards.\n");
    } else if (rows < game->height + 6) {
        printf("\nTerminal is short for this board; resize taller if the bottom is clipped.\n");
    }
    fflush(stdout);
}

static void terminal_set_message(char *message, size_t message_size, const char *text) {
    snprintf(message, message_size, "%s", text);
}

static int interactive(BoardSelection selection, uint32_t seed);

static int terminal_controller(BoardSelection selection, uint32_t seed) {
    if (!terminal_can_control()) {
        return interactive(selection, seed);
    }

    TerminalSession terminal;
    if (!terminal_enter(&terminal)) {
        return interactive(selection, seed);
    }

    Game game;
    game_init(&game, selection.config.width, selection.config.height, selection.config.mines, seed);
    int focus_x = 0;
    int focus_y = 0;
    int running = 1;
    int blink_tick = 0;
    int show_help = 0;
    char message[96];
    terminal_set_message(message, sizeof(message), "Move with arrows, reveal with Space/Enter.");

    while (running) {
        terminal_render_controller(&game, &selection, focus_x, focus_y,
                                   (blink_tick / 4) % 2 == 0, show_help, message);

        TerminalKey key = terminal_read_key();
        switch (key) {
        case TERM_KEY_QUIT:
            running = 0;
            break;
        case TERM_KEY_UP:
            if (focus_y > 0) {
                focus_y--;
            }
            break;
        case TERM_KEY_DOWN:
            if (focus_y < game.height - 1) {
                focus_y++;
            }
            break;
        case TERM_KEY_LEFT:
            if (focus_x > 0) {
                focus_x--;
            }
            break;
        case TERM_KEY_RIGHT:
            if (focus_x < game.width - 1) {
                focus_x++;
            }
            break;
        case TERM_KEY_REVEAL:
            if (const_cell_at(&game, focus_x, focus_y)->revealed) {
                game_chord(&game, focus_x, focus_y);
                terminal_set_message(message, sizeof(message), "Chorded revealed number.");
            } else {
                game_reveal(&game, focus_x, focus_y);
                terminal_set_message(message, sizeof(message), "Revealed cell.");
            }
            break;
        case TERM_KEY_FLAG:
            game_toggle_flag(&game, focus_x, focus_y);
            terminal_set_message(message, sizeof(message), "Toggled flag.");
            break;
        case TERM_KEY_CHORD:
            game_chord(&game, focus_x, focus_y);
            terminal_set_message(message, sizeof(message), "Chorded cell.");
            break;
        case TERM_KEY_RESTART:
            reset_to_selection(&game, &selection, seed);
            focus_x = 0;
            focus_y = 0;
            terminal_set_message(message, sizeof(message), "Restarted board.");
            break;
        case TERM_KEY_HELP:
            show_help = !show_help;
            terminal_set_message(message, sizeof(message), show_help ? "Help shown." : "Help hidden.");
            break;
        case TERM_KEY_SMALL:
            selection_set_preset(&selection, SIZE_SMALL, selection.difficulty);
            reset_to_selection(&game, &selection, seed);
            focus_x = 0;
            focus_y = 0;
            terminal_set_message(message, sizeof(message), "Selected small size.");
            break;
        case TERM_KEY_MEDIUM:
            selection_set_preset(&selection, SIZE_MEDIUM, selection.difficulty);
            reset_to_selection(&game, &selection, seed);
            focus_x = 0;
            focus_y = 0;
            terminal_set_message(message, sizeof(message), "Selected medium size.");
            break;
        case TERM_KEY_LARGE:
            selection_set_preset(&selection, SIZE_LARGE, selection.difficulty);
            reset_to_selection(&game, &selection, seed);
            focus_x = 0;
            focus_y = 0;
            terminal_set_message(message, sizeof(message), "Selected large size.");
            break;
        case TERM_KEY_EASY:
            selection_set_preset(&selection, selection.level, MINE_EASY);
            reset_to_selection(&game, &selection, seed);
            focus_x = 0;
            focus_y = 0;
            terminal_set_message(message, sizeof(message), "Selected easy difficulty.");
            break;
        case TERM_KEY_NORMAL:
            selection_set_preset(&selection, selection.level, MINE_NORMAL);
            reset_to_selection(&game, &selection, seed);
            focus_x = 0;
            focus_y = 0;
            terminal_set_message(message, sizeof(message), "Selected normal difficulty.");
            break;
        case TERM_KEY_HARD:
            selection_set_preset(&selection, selection.level, MINE_HARD);
            reset_to_selection(&game, &selection, seed);
            focus_x = 0;
            focus_y = 0;
            terminal_set_message(message, sizeof(message), "Selected hard difficulty.");
            break;
        case TERM_KEY_NONE:
            break;
        }

        if (game.status == GAME_WON) {
            terminal_set_message(message, sizeof(message), "Cleared. Press r for a new board.");
        } else if (game.status == GAME_LOST) {
            terminal_set_message(message, sizeof(message), "Mine hit. Press r for a new board.");
        }

        blink_tick++;
        terminal_sleep_ms(35);
    }

    game_free(&game);
    terminal_leave(&terminal);
    return 0;
}

static int interactive(BoardSelection selection, uint32_t seed) {
    Game game;
    char line[128];
    game_init(&game, selection.config.width, selection.config.height, selection.config.mines, seed);

    printf(APP_NAME " " APP_VERSION "\n");
    print_help();

    while (1) {
        render(&game, &selection);
        if (game.status == GAME_WON) {
            printf("Cleared.\n");
        } else if (game.status == GAME_LOST) {
            printf("Mine hit. Start a new board with n, n s e, s m, or d h.\n");
        }
        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        char command = (char)tolower((unsigned char)line[0]);
        if (command == 'q') {
            break;
        }
        if (command == 'h') {
            print_help();
            continue;
        }
        if (command == 'n') {
            if (parse_new_selection(line, &selection)) {
                reset_to_selection(&game, &selection, seed);
            } else {
                printf("unknown level or difficulty\n");
            }
            continue;
        }
        if (command == 's') {
            char value[32] = "";
            SizeLevel level = selection.level;
            if (sscanf(line, " %*s %31s", value) == 1) {
                lowercase_text(value);
                if (parse_size_level_arg(value, &level)) {
                    selection_set_preset(&selection, level, selection.difficulty);
                    reset_to_selection(&game, &selection, seed);
                } else {
                    printf("unknown size level\n");
                }
            } else {
                printf("missing size level\n");
            }
            continue;
        }
        if (command == 'd') {
            char value[32] = "";
            MineDifficulty difficulty = selection.difficulty;
            if (sscanf(line, " %*s %31s", value) == 1) {
                lowercase_text(value);
                if (parse_mine_difficulty_arg(value, &difficulty)) {
                    selection_set_preset(&selection, selection.level, difficulty);
                    reset_to_selection(&game, &selection, seed);
                } else {
                    printf("unknown difficulty\n");
                }
            } else {
                printf("missing difficulty\n");
            }
            continue;
        }

        int x = 0;
        int y = 0;
        if ((command == 'r' || command == 'f' || command == 'c') && parse_xy(line, &x, &y)) {
            if (!in_bounds(&game, x, y)) {
                printf("out of bounds\n");
                continue;
            }
            if (command == 'r') {
                game_reveal(&game, x, y);
            } else if (command == 'f') {
                game_toggle_flag(&game, x, y);
            } else {
                game_chord(&game, x, y);
            }
        } else {
            printf("unknown command\n");
        }
    }

    game_free(&game);
    return 0;
}

#ifdef CLEAN_MINES_WITH_SDL
typedef struct {
    SDL_Texture *tile_closed;
    SDL_Texture *tile_open;
    SDL_Texture *tile_pressed;
    SDL_Texture *closed_flag[2];
    SDL_Texture *closed_wrong_flag[2];
    SDL_Texture *open_mine[2];
    SDL_Texture *open_mine_hit[2];
} Textures;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} RectI;

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} Color;

typedef struct {
    Color background;
    Color header;
    Color counter;
    Color focus;
    Color tile_closed;
    Color tile_open;
    Color tile_pressed;
    Color numbers[9];
} GraphicsTheme;

typedef enum {
    MENU_CLOSED = 0,
    MENU_ROOT,
    MENU_SIZE,
    MENU_DIFFICULTY,
    MENU_THEME
} MenuState;

typedef struct {
    RectI board;
    int tile_size;
    int ui_scale;
    int board_pad;
    int header_height;
} GraphicsLayout;

static const int TILE_SIZE = 32;
static const int UI_SCALE = 2;
static const int BASE_BOARD_PAD = 6;
static const int BASE_HEADER_HEIGHT = 29;
static const int BASE_COUNTER_X = 7;
static const int BASE_COUNTER_Y = 7;
static const int BASE_COUNTER_WIDTH = 48;
static const int BASE_COUNTER_HEIGHT = 15;
static const int BASE_COUNTER_GAP = 7;
static const int BASE_MENU_BUTTON_WIDTH = 38;
static const int BASE_MENU_BUTTON_HEIGHT = 15;
static const int BASE_MENU_PANEL_WIDTH = 92;
static const int BASE_MENU_PANEL_PAD = 3;
static const int BASE_MENU_ROW_HEIGHT = 15;
static const int BASE_MENU_TEXT_X = 5;

static const GraphicsTheme GRAPHICS_THEMES[2] = {
    {
        {238, 241, 238},
        {255, 255, 255},
        {36, 43, 43},
        {208, 138, 36},
        {255, 255, 255},
        {255, 255, 255},
        {255, 255, 255},
        {
            {30, 36, 36},
            {34, 93, 185},
            {40, 115, 61},
            {192, 44, 44},
            {81, 55, 150},
            {143, 44, 44},
            {20, 112, 109},
            {34, 34, 34},
            {107, 112, 112}
        }
    },
    {
        {22, 24, 23},
        {30, 34, 33},
        {13, 18, 18},
        {224, 156, 56},
        {108, 132, 126},
        {112, 104, 88},
        {100, 92, 78},
        {
            {219, 226, 223},
            {91, 158, 255},
            {91, 201, 122},
            {255, 102, 102},
            {174, 145, 255},
            {236, 118, 96},
            {79, 214, 205},
            {228, 231, 224},
            {156, 166, 164}
        }
    }
};

static const GraphicsTheme *graphics_theme(ThemeMode theme) {
    return &GRAPHICS_THEMES[theme == THEME_DARK ? THEME_DARK : THEME_LIGHT];
}

static const char *theme_name(ThemeMode theme) {
    return theme == THEME_DARK ? "dark" : "light";
}

static int build_path(char *out, size_t out_size, const char *base, const char *name) {
    int written = snprintf(out, out_size, "%s/%s", base, name);
    return written > 0 && (size_t)written < out_size;
}

static SDL_Texture *try_load_bmp_texture(SDL_Renderer *renderer, const char *dir,
                                         const char *name) {
    char path[512];
    if (dir == NULL || dir[0] == '\0' || !build_path(path, sizeof(path), dir, name)) {
        return NULL;
    }

    SDL_Surface *surface = SDL_LoadBMP(path);
    if (surface == NULL) {
        return NULL;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

static SDL_Texture *load_bmp_texture(SDL_Renderer *renderer, const char *name) {
    const char *env_dir = getenv("CLEAN_MINES_ASSET_DIR");
    SDL_Texture *texture = try_load_bmp_texture(renderer, env_dir, name);
    if (texture != NULL) {
        return texture;
    }

#ifdef CLEAN_MINES_WITH_SDL
    char *base_path = SDL_GetBasePath();
    if (base_path != NULL) {
        char exe_relative[512];
        int written = snprintf(exe_relative, sizeof(exe_relative), "%s../share/clean-mines/assets", base_path);
        if (written > 0 && (size_t)written < sizeof(exe_relative)) {
            texture = try_load_bmp_texture(renderer, exe_relative, name);
        }
        SDL_free(base_path);
        if (texture != NULL) {
            return texture;
        }
    }
#endif

    const char *dirs[] = {
        "assets/native",
        "assets",
        "share/clean-mines/assets",
        "../share/clean-mines/assets",
        NULL
    };

    for (int i = 0; dirs[i] != NULL; i++) {
        texture = try_load_bmp_texture(renderer, dirs[i], name);
        if (texture != NULL) {
            return texture;
        }
    }

    fprintf(stderr, "could not load asset %s: %s\n", name, SDL_GetError());
    return NULL;
}

static void destroy_texture_pair(SDL_Texture *textures[2]) {
    SDL_DestroyTexture(textures[THEME_LIGHT]);
    SDL_DestroyTexture(textures[THEME_DARK]);
}

static void destroy_textures(Textures *textures) {
    SDL_DestroyTexture(textures->tile_closed);
    SDL_DestroyTexture(textures->tile_open);
    SDL_DestroyTexture(textures->tile_pressed);
    destroy_texture_pair(textures->closed_flag);
    destroy_texture_pair(textures->closed_wrong_flag);
    destroy_texture_pair(textures->open_mine);
    destroy_texture_pair(textures->open_mine_hit);
    memset(textures, 0, sizeof(*textures));
}

static int load_texture_pair(SDL_Renderer *renderer, SDL_Texture *textures[2],
                             const char *light_name, const char *dark_name) {
    textures[THEME_LIGHT] = load_bmp_texture(renderer, light_name);
    textures[THEME_DARK] = load_bmp_texture(renderer, dark_name);
    return textures[THEME_LIGHT] != NULL && textures[THEME_DARK] != NULL;
}

static int load_textures(SDL_Renderer *renderer, Textures *textures) {
    memset(textures, 0, sizeof(*textures));
    textures->tile_closed = load_bmp_texture(renderer, "tile-closed.bmp");
    textures->tile_open = load_bmp_texture(renderer, "tile-open.bmp");
    textures->tile_pressed = load_bmp_texture(renderer, "tile-pressed.bmp");

    int ok = textures->tile_closed != NULL &&
             textures->tile_open != NULL &&
             textures->tile_pressed != NULL &&
             load_texture_pair(renderer, textures->closed_flag,
                               "tile-closed-flag-light.bmp",
                               "tile-closed-flag-dark.bmp") &&
             load_texture_pair(renderer, textures->closed_wrong_flag,
                               "tile-closed-wrong-flag-light.bmp",
                               "tile-closed-wrong-flag-dark.bmp") &&
             load_texture_pair(renderer, textures->open_mine,
                               "tile-open-mine-light.bmp",
                               "tile-open-mine-dark.bmp") &&
             load_texture_pair(renderer, textures->open_mine_hit,
                               "tile-open-mine-hit-light.bmp",
                               "tile-open-mine-hit-dark.bmp");

    if (!ok) {
        destroy_textures(textures);
        return 0;
    }
    return 1;
}

static void set_texture_color(SDL_Texture *texture, Color color) {
    if (texture != NULL) {
        SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    }
}

static void set_texture_pair_color(SDL_Texture *textures[2], Color color) {
    set_texture_color(textures[THEME_LIGHT], color);
    set_texture_color(textures[THEME_DARK], color);
}

static void apply_texture_theme(Textures *textures, ThemeMode theme_mode) {
    const GraphicsTheme *theme = graphics_theme(theme_mode);
    Color white = {255, 255, 255};
    set_texture_color(textures->tile_closed, theme->tile_closed);
    set_texture_color(textures->tile_open, theme->tile_open);
    set_texture_color(textures->tile_pressed, theme->tile_pressed);
    set_texture_pair_color(textures->closed_flag, white);
    set_texture_pair_color(textures->closed_wrong_flag, white);
    set_texture_pair_color(textures->open_mine, white);
    set_texture_pair_color(textures->open_mine_hit, white);
}

static int scaled_ui(int base_value, int ui_scale) {
    return base_value * ui_scale;
}

static int minimum_header_width(int ui_scale) {
    return scaled_ui(BASE_COUNTER_X + BASE_COUNTER_WIDTH + BASE_COUNTER_GAP +
                         BASE_COUNTER_WIDTH + BASE_COUNTER_GAP +
                         BASE_MENU_BUTTON_WIDTH + BASE_BOARD_PAD,
                     ui_scale);
}

static void window_size_for_game(const Game *game, SDL_Window *window, int *width, int *height) {
    (void)window;
    int tile_size = TILE_SIZE;
    int ui_scale = UI_SCALE;
    int board_pad = scaled_ui(BASE_BOARD_PAD, ui_scale);
    int header_height = scaled_ui(BASE_HEADER_HEIGHT, ui_scale);

    *width = game->width * tile_size + board_pad * 2;
    if (*width < minimum_header_width(ui_scale)) {
        *width = minimum_header_width(ui_scale);
    }
    *height = game->height * tile_size + header_height + board_pad;
}

static const char *status_name(GameStatus status) {
    if (status == GAME_READY) {
        return "ready";
    }
    if (status == GAME_PLAYING) {
        return "playing";
    }
    if (status == GAME_WON) {
        return "won";
    }
    return "lost";
}

static void update_window_title(SDL_Window *window, const Game *game,
                                const BoardSelection *selection, ThemeMode theme) {
    char title[160];
    char label[64];
    selection_label(selection, label, sizeof(label));
    snprintf(title, sizeof(title), "%s - %s - %s - mines %d flags %d - %s",
             APP_NAME, status_name(game->status), label, game->mines, count_flags(game),
             theme_name(theme));
    SDL_SetWindowTitle(window, title);
}

static void fill_rect(SDL_Renderer *renderer, int x, int y, int w, int h,
                      unsigned char r, unsigned char g, unsigned char b) {
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderFillRect(renderer, &rect);
}

static void outline_rect(SDL_Renderer *renderer, int x, int y, int w, int h,
                         unsigned char r, unsigned char g, unsigned char b) {
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderDrawRect(renderer, &rect);
}

static void set_draw_color(SDL_Renderer *renderer, Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
}

static Color make_color(unsigned char r, unsigned char g, unsigned char b) {
    Color color;
    color.r = r;
    color.g = g;
    color.b = b;
    return color;
}

static void fill_rect_color(SDL_Renderer *renderer, RectI rect, Color color) {
    fill_rect(renderer, rect.x, rect.y, rect.width, rect.height, color.r, color.g, color.b);
}

static void outline_rect_color(SDL_Renderer *renderer, RectI rect, Color color) {
    outline_rect(renderer, rect.x, rect.y, rect.width, rect.height, color.r, color.g, color.b);
}

static int point_in_rect(RectI rect, int x, int y) {
    return x >= rect.x && y >= rect.y &&
           x < rect.x + rect.width && y < rect.y + rect.height;
}

static const unsigned char DIGITS[11][5] = {
    {7, 5, 5, 5, 7},
    {2, 6, 2, 2, 7},
    {7, 1, 7, 4, 7},
    {7, 1, 7, 1, 7},
    {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7},
    {7, 4, 7, 5, 7},
    {7, 1, 1, 1, 1},
    {7, 5, 7, 5, 7},
    {7, 5, 7, 1, 7},
    {0, 0, 7, 0, 0}
};

typedef struct {
    char c;
    unsigned char rows[7];
} TextGlyph;

static const TextGlyph TEXT_GLYPHS[] = {
    {'0', {14, 17, 19, 21, 25, 17, 14}},
    {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},
    {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}},
    {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'A', {14, 17, 17, 31, 17, 17, 17}},
    {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}},
    {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}},
    {'J', {1, 1, 1, 1, 17, 17, 14}},
    {'K', {17, 18, 20, 24, 20, 18, 17}},
    {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}},
    {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}},
    {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},
    {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}},
    {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},
    {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},
    {'<', {1, 2, 4, 8, 4, 2, 1}},
    {'>', {16, 8, 4, 2, 4, 8, 16}},
    {'.', {0, 0, 0, 0, 0, 12, 12}},
    {':', {0, 12, 12, 0, 12, 12, 0}}
};

static const unsigned char *text_glyph_rows(char c) {
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    for (size_t i = 0; i < sizeof(TEXT_GLYPHS) / sizeof(TEXT_GLYPHS[0]); i++) {
        if (TEXT_GLYPHS[i].c == c) {
            return TEXT_GLYPHS[i].rows;
        }
    }
    return NULL;
}

static int text_width(const char *text, int scale) {
    int count = 0;
    while (text[count] != '\0') {
        count++;
    }
    if (count == 0) {
        return 0;
    }
    return (count * 6 - 1) * scale;
}

static void draw_text(SDL_Renderer *renderer, const char *text, int x, int y,
                      int scale, Color color) {
    set_draw_color(renderer, color);
    for (int i = 0; text[i] != '\0'; i++) {
        const unsigned char *rows = text_glyph_rows(text[i]);
        if (rows != NULL) {
            for (int row = 0; row < 7; row++) {
                for (int col = 0; col < 5; col++) {
                    if ((rows[row] & (1u << (4 - col))) == 0) {
                        continue;
                    }
                    SDL_Rect rect;
                    rect.x = x + i * 6 * scale + col * scale;
                    rect.y = y + row * scale;
                    rect.w = scale;
                    rect.h = scale;
                    SDL_RenderFillRect(renderer, &rect);
                }
            }
        }
    }
}

static void draw_text_centered(SDL_Renderer *renderer, const char *text, RectI rect,
                               int scale, Color color) {
    int width = text_width(text, scale);
    int height = 7 * scale;
    int x = rect.x + (rect.width - width) / 2;
    int y = rect.y + (rect.height - height) / 2;
    draw_text(renderer, text, x, y, scale, color);
}

static void draw_text_left_centered(SDL_Renderer *renderer, const char *text, RectI rect,
                                    int x, int scale, Color color) {
    int height = 7 * scale;
    int y = rect.y + (rect.height - height) / 2;
    draw_text(renderer, text, x, y, scale, color);
}

static void set_number_color(SDL_Renderer *renderer, int value, const GraphicsTheme *theme) {
    int index = value < 0 ? 0 : value;
    if (index > 8) {
        index = 8;
    }
    SDL_SetRenderDrawColor(renderer, theme->numbers[index].r, theme->numbers[index].g,
                           theme->numbers[index].b, 255);
}

static void draw_glyph(SDL_Renderer *renderer, int glyph, int x, int y, int scale) {
    if (glyph < 0 || glyph > 10) {
        return;
    }
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            if ((DIGITS[glyph][row] & (1u << (2 - col))) == 0) {
                continue;
            }
            SDL_Rect rect;
            rect.x = x + col * scale;
            rect.y = y + row * scale;
            rect.w = scale;
            rect.h = scale;
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}

static void draw_int(SDL_Renderer *renderer, int value, int x, int y, int scale) {
    char text[16];
    snprintf(text, sizeof(text), "%d", value);
    for (int i = 0; text[i] != '\0'; i++) {
        int glyph = text[i] == '-' ? 10 : text[i] - '0';
        draw_glyph(renderer, glyph, x + i * scale * 4, y, scale);
    }
}

static int int_width(int value, int scale) {
    char text[16];
    int count = snprintf(text, sizeof(text), "%d", value);
    if (count <= 0) {
        return 0;
    }
    return (count * 4 - 1) * scale;
}

static void draw_int_centered(SDL_Renderer *renderer, int value, RectI rect, int scale) {
    int width = int_width(value, scale);
    int height = 5 * scale;
    int x = rect.x + (rect.width - width) / 2;
    int y = rect.y + (rect.height - height) / 2;
    draw_int(renderer, value, x, y, scale);
}

static void draw_cell_number(SDL_Renderer *renderer, int value, SDL_Rect dst,
                             const GraphicsTheme *theme) {
    int scale = dst.w / 8;
    if (scale < 1) {
        scale = 1;
    }
    int glyph_w = 3 * scale;
    int glyph_h = 5 * scale;
    set_number_color(renderer, value, theme);
    draw_glyph(renderer, value, dst.x + (dst.w - glyph_w) / 2,
               dst.y + (dst.h - glyph_h) / 2, scale);
}

static RectI counter_rect(const GraphicsLayout *layout, int index) {
    RectI rect;
    rect.x = scaled_ui(BASE_COUNTER_X, layout->ui_scale);
    if (index > 0) {
        rect.x += scaled_ui(BASE_COUNTER_WIDTH + BASE_COUNTER_GAP, layout->ui_scale);
    }
    rect.y = scaled_ui(BASE_COUNTER_Y, layout->ui_scale);
    rect.width = scaled_ui(BASE_COUNTER_WIDTH, layout->ui_scale);
    rect.height = scaled_ui(BASE_COUNTER_HEIGHT, layout->ui_scale);
    return rect;
}

static RectI menu_button_rect(const GraphicsLayout *layout, int window_width) {
    RectI rect;
    rect.x = window_width - layout->board_pad -
             scaled_ui(BASE_MENU_BUTTON_WIDTH, layout->ui_scale);
    rect.y = scaled_ui(BASE_COUNTER_Y, layout->ui_scale);
    rect.width = scaled_ui(BASE_MENU_BUTTON_WIDTH, layout->ui_scale);
    rect.height = scaled_ui(BASE_MENU_BUTTON_HEIGHT, layout->ui_scale);
    if (rect.x < layout->board_pad) {
        rect.x = layout->board_pad;
    }
    return rect;
}

static int menu_row_count(MenuState state) {
    if (state == MENU_ROOT) {
        return 5;
    }
    if (state == MENU_SIZE || state == MENU_DIFFICULTY) {
        return 4;
    }
    if (state == MENU_THEME) {
        return 3;
    }
    return 0;
}

static RectI menu_panel_rect(const GraphicsLayout *layout, int window_width, MenuState state) {
    RectI rect;
    int rows = menu_row_count(state);
    int panel_pad = scaled_ui(BASE_MENU_PANEL_PAD, layout->ui_scale);
    int row_height = scaled_ui(BASE_MENU_ROW_HEIGHT, layout->ui_scale);
    rect.width = scaled_ui(BASE_MENU_PANEL_WIDTH, layout->ui_scale);
    rect.height = rows * row_height + panel_pad * 2;
    rect.x = window_width - layout->board_pad - rect.width;
    rect.y = layout->header_height - layout->ui_scale;
    if (rect.x < layout->board_pad) {
        rect.x = layout->board_pad;
    }
    return rect;
}

static int menu_row_at(MenuState state, const GraphicsLayout *layout,
                       int window_width, int px, int py) {
    RectI panel = menu_panel_rect(layout, window_width, state);
    int rows = menu_row_count(state);
    int panel_pad = scaled_ui(BASE_MENU_PANEL_PAD, layout->ui_scale);
    int row_height = scaled_ui(BASE_MENU_ROW_HEIGHT, layout->ui_scale);
    if (!point_in_rect(panel, px, py)) {
        return -1;
    }
    int local_y = py - panel.y - panel_pad;
    if (local_y < 0) {
        return -1;
    }
    int row = local_y / row_height;
    if (row < 0 || row >= rows) {
        return -1;
    }
    return row;
}

static void menu_row_label(MenuState state, int row, int flag_mode,
                           char *out, size_t out_size) {
    const char *label = "";
    if (state == MENU_ROOT) {
        const char *labels[] = {"SIZE >", "DIFFICULTY >", "THEME >", "FLAG MODE", "RESET"};
        label = labels[row];
        if (row == 3) {
            snprintf(out, out_size, "%s", flag_mode ? "FLAG ON" : "FLAG OFF");
            return;
        }
    } else if (state == MENU_SIZE) {
        const char *labels[] = {"< BACK", "SMALL", "MEDIUM", "LARGE"};
        label = labels[row];
    } else if (state == MENU_DIFFICULTY) {
        const char *labels[] = {"< BACK", "EASY", "NORMAL", "HARD"};
        label = labels[row];
    } else if (state == MENU_THEME) {
        const char *labels[] = {"< BACK", "DARK", "LIGHT"};
        label = labels[row];
    }
    snprintf(out, out_size, "%s", label);
}

static int menu_row_active(MenuState state, int row, const BoardSelection *selection,
                           ThemeMode theme, int flag_mode) {
    if (state == MENU_ROOT) {
        return row == 3 && flag_mode;
    }
    if (state == MENU_SIZE) {
        return !selection->custom && row == (int)selection->level + 1;
    }
    if (state == MENU_DIFFICULTY) {
        return !selection->custom && row == (int)selection->difficulty + 1;
    }
    if (state == MENU_THEME) {
        return row == (theme == THEME_DARK ? 1 : 2);
    }
    return 0;
}

static void clamp_menu_focus(MenuState state, int *menu_focus) {
    int rows = menu_row_count(state);
    if (rows <= 0) {
        *menu_focus = 0;
    } else if (*menu_focus < 0) {
        *menu_focus = rows - 1;
    } else if (*menu_focus >= rows) {
        *menu_focus = 0;
    }
}

static void set_menu_state(MenuState *menu_state, int *menu_focus, MenuState next) {
    *menu_state = next;
    *menu_focus = 0;
}

static void render_menu(SDL_Renderer *renderer, const BoardSelection *selection,
                        ThemeMode theme_mode, MenuState menu_state, int menu_focus,
                        int flag_mode, int window_width, const GraphicsLayout *layout) {
    const GraphicsTheme *theme = graphics_theme(theme_mode);
    Color button_text = {255, 255, 255};
    Color panel = make_color(250, 252, 249);
    Color border = make_color(174, 184, 180);
    Color text = make_color(29, 36, 35);
    Color selected_text = {24, 27, 26};
    Color focus_soft = make_color(228, 234, 230);
    if (theme_mode == THEME_DARK) {
        panel = make_color(34, 40, 38);
        border = make_color(116, 132, 127);
        text = make_color(232, 238, 234);
        focus_soft = make_color(58, 68, 65);
    }

    RectI button = menu_button_rect(layout, window_width);
    fill_rect_color(renderer, button, theme->counter);
    outline_rect_color(renderer, button, menu_state == MENU_CLOSED ? border : theme->focus);
    draw_text_centered(renderer, "MENU", button, layout->ui_scale, button_text);

    if (menu_state == MENU_CLOSED) {
        return;
    }

    RectI menu = menu_panel_rect(layout, window_width, menu_state);
    fill_rect_color(renderer, menu, panel);
    outline_rect_color(renderer, menu, border);

    int rows = menu_row_count(menu_state);
    int panel_pad = scaled_ui(BASE_MENU_PANEL_PAD, layout->ui_scale);
    int row_height = scaled_ui(BASE_MENU_ROW_HEIGHT, layout->ui_scale);
    for (int row = 0; row < rows; row++) {
        RectI row_rect;
        row_rect.x = menu.x + panel_pad;
        row_rect.y = menu.y + panel_pad + row * row_height;
        row_rect.width = menu.width - panel_pad * 2;
        row_rect.height = row_height;

        int active = menu_row_active(menu_state, row, selection, theme_mode, flag_mode);
        if (active) {
            fill_rect_color(renderer, row_rect, theme->focus);
        } else if (row == menu_focus) {
            fill_rect_color(renderer, row_rect, focus_soft);
        }
        if (row == menu_focus) {
            outline_rect_color(renderer, row_rect, theme->focus);
        }

        char label[32];
        menu_row_label(menu_state, row, flag_mode, label, sizeof(label));
        draw_text_left_centered(renderer, label, row_rect,
                                row_rect.x + scaled_ui(BASE_MENU_TEXT_X, layout->ui_scale),
                                layout->ui_scale, active ? selected_text : text);
    }
}

static void graphics_layout(const Game *game, int window_width, int window_height,
                            GraphicsLayout *layout) {
    int tile_size = TILE_SIZE;
    int ui_scale = UI_SCALE;
    int board_pad = scaled_ui(BASE_BOARD_PAD, ui_scale);
    int header_height = scaled_ui(BASE_HEADER_HEIGHT, ui_scale);
    int available_height = window_height - header_height - board_pad;

    layout->tile_size = tile_size;
    layout->ui_scale = ui_scale;
    layout->board_pad = board_pad;
    layout->header_height = header_height;
    layout->board.width = game->width * tile_size;
    layout->board.height = game->height * tile_size;
    layout->board.x = (window_width - layout->board.width) / 2;
    layout->board.y = header_height + (available_height - layout->board.height) / 2;
    if (layout->board.x < board_pad) {
        layout->board.x = board_pad;
    }
    if (layout->board.y < header_height) {
        layout->board.y = header_height;
    }
}

static int cell_from_point(const Game *game, int window_width, int window_height,
                           int px, int py, int *x, int *y) {
    GraphicsLayout layout;
    graphics_layout(game, window_width, window_height, &layout);
    int local_x = px - layout.board.x;
    int local_y = py - layout.board.y;
    if (local_x < 0 || local_y < 0) {
        return 0;
    }
    *x = local_x / layout.tile_size;
    *y = local_y / layout.tile_size;
    return in_bounds(game, *x, *y);
}

static SDL_Texture *themed_texture(SDL_Texture *const textures[2], ThemeMode theme_mode) {
    return textures[theme_mode == THEME_DARK ? THEME_DARK : THEME_LIGHT];
}

static void render_graphics(SDL_Window *window, SDL_Renderer *renderer, const Textures *textures,
                            const Game *game, const BoardSelection *selection,
                            int focus_x, int focus_y, ThemeMode theme_mode,
                            MenuState menu_state, int menu_focus, int flag_mode) {
    const GraphicsTheme *theme = graphics_theme(theme_mode);
    SDL_SetRenderDrawColor(renderer, theme->background.r, theme->background.g,
                           theme->background.b, 255);
    SDL_RenderClear(renderer);

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    GraphicsLayout layout;
    graphics_layout(game, width, height, &layout);

    fill_rect(renderer, 0, 0, width, layout.header_height,
              theme->header.r, theme->header.g, theme->header.b);

    RectI mines_counter = counter_rect(&layout, 0);
    RectI state_counter = counter_rect(&layout, 1);
    fill_rect_color(renderer, mines_counter, theme->counter);
    fill_rect_color(renderer, state_counter, theme->counter);
    set_number_color(renderer, 1, theme);
    draw_int_centered(renderer, game->mines - count_flags(game),
                      mines_counter, layout.ui_scale * 2);
    draw_int_centered(renderer, game->status == GAME_WON ? 1 : game->status == GAME_LOST ? -1 : 0,
                      state_counter, layout.ui_scale * 2);

    for (int y = 0; y < game->height; y++) {
        for (int x = 0; x < game->width; x++) {
            const Cell *cell = const_cell_at(game, x, y);
            SDL_Rect dst;
            dst.x = layout.board.x + x * layout.tile_size;
            dst.y = layout.board.y + y * layout.tile_size;
            dst.w = layout.tile_size;
            dst.h = layout.tile_size;

            SDL_Texture *cell_texture = NULL;
            if (game->status == GAME_LOST && cell->wrong) {
                cell_texture = themed_texture(textures->closed_wrong_flag, theme_mode);
            } else if (!cell->revealed && cell->flagged) {
                cell_texture = themed_texture(textures->closed_flag, theme_mode);
            } else if (cell->revealed && cell->mine) {
                cell_texture = themed_texture(cell->hit ? textures->open_mine_hit : textures->open_mine,
                                              theme_mode);
            }

            if (cell_texture != NULL) {
                SDL_RenderCopy(renderer, cell_texture, NULL, &dst);
            } else {
                SDL_Texture *base = cell->revealed ? textures->tile_open : textures->tile_closed;
                SDL_RenderCopy(renderer, base, NULL, &dst);
            }

            if (!cell->mine && cell->revealed && cell->adjacent > 0) {
                draw_cell_number(renderer, cell->adjacent, dst, theme);
            }
        }
    }

    if (in_bounds(game, focus_x, focus_y)) {
        outline_rect(renderer, layout.board.x + focus_x * layout.tile_size + 1,
                     layout.board.y + focus_y * layout.tile_size + 1,
                     layout.tile_size - 2, layout.tile_size - 2,
                     theme->focus.r, theme->focus.g, theme->focus.b);
    }

    render_menu(renderer, selection, theme_mode, menu_state, menu_focus, flag_mode,
                width, &layout);
    SDL_RenderPresent(renderer);
}

static void reset_graphics_game(Game *game, SDL_Window *window, const BoardSelection *selection,
                                uint32_t seed, int *focus_x, int *focus_y, ThemeMode theme) {
    game_free(game);
    game_init(game, selection->config.width, selection->config.height, selection->config.mines, seed);
    *focus_x = 0;
    *focus_y = 0;
    int width = 0;
    int height = 0;
    window_size_for_game(game, window, &width, &height);
    SDL_SetWindowSize(window, width, height);
    update_window_title(window, game, selection, theme);
}

static void graphics_reveal_or_chord(Game *game, int x, int y) {
    Cell *cell = cell_at(game, x, y);
    if (cell == NULL) {
        return;
    }
    if (cell->revealed) {
        game_chord(game, x, y);
    } else {
        game_reveal(game, x, y);
    }
}

static void invoke_graphics_menu_row(Game *game, SDL_Window *window, Textures *textures,
                                     BoardSelection *selection, uint32_t seed,
                                     ThemeMode *theme, MenuState *menu_state,
                                     int *menu_focus, int *flag_mode,
                                     int *focus_x, int *focus_y, int row) {
    if (row < 0 || row >= menu_row_count(*menu_state)) {
        return;
    }

    if (*menu_state == MENU_ROOT) {
        if (row == 0) {
            set_menu_state(menu_state, menu_focus, MENU_SIZE);
        } else if (row == 1) {
            set_menu_state(menu_state, menu_focus, MENU_DIFFICULTY);
        } else if (row == 2) {
            set_menu_state(menu_state, menu_focus, MENU_THEME);
        } else if (row == 3) {
            *flag_mode = !*flag_mode;
            set_menu_state(menu_state, menu_focus, MENU_CLOSED);
        } else if (row == 4) {
            reset_graphics_game(game, window, selection, seed, focus_x, focus_y, *theme);
            set_menu_state(menu_state, menu_focus, MENU_CLOSED);
        }
        return;
    }

    if (row == 0) {
        set_menu_state(menu_state, menu_focus, MENU_ROOT);
        return;
    }

    if (*menu_state == MENU_SIZE) {
        selection_set_preset(selection, (SizeLevel)(row - 1), selection->difficulty);
        reset_graphics_game(game, window, selection, seed, focus_x, focus_y, *theme);
        set_menu_state(menu_state, menu_focus, MENU_CLOSED);
    } else if (*menu_state == MENU_DIFFICULTY) {
        selection_set_preset(selection, selection->level, (MineDifficulty)(row - 1));
        reset_graphics_game(game, window, selection, seed, focus_x, focus_y, *theme);
        set_menu_state(menu_state, menu_focus, MENU_CLOSED);
    } else if (*menu_state == MENU_THEME) {
        *theme = row == 1 ? THEME_DARK : THEME_LIGHT;
        apply_texture_theme(textures, *theme);
        update_window_title(window, game, selection, *theme);
        set_menu_state(menu_state, menu_focus, MENU_CLOSED);
    }
}

static void handle_graphics_pointer(Game *game, SDL_Window *window, Textures *textures,
                                    BoardSelection *selection, uint32_t seed,
                                    ThemeMode *theme, MenuState *menu_state,
                                    int *menu_focus, int *flag_mode,
                                    int *focus_x, int *focus_y,
                                    int px, int py, unsigned char button) {
    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(window, &window_width, &window_height);
    GraphicsLayout layout;
    graphics_layout(game, window_width, window_height, &layout);

    if (button == SDL_BUTTON_LEFT && point_in_rect(menu_button_rect(&layout, window_width), px, py)) {
        set_menu_state(menu_state, menu_focus,
                       *menu_state == MENU_CLOSED ? MENU_ROOT : MENU_CLOSED);
        return;
    }

    if (*menu_state != MENU_CLOSED) {
        if (button == SDL_BUTTON_LEFT) {
            int row = menu_row_at(*menu_state, &layout, window_width, px, py);
            if (row >= 0) {
                *menu_focus = row;
                invoke_graphics_menu_row(game, window, textures, selection, seed, theme,
                                         menu_state, menu_focus, flag_mode, focus_x, focus_y, row);
                return;
            }
        }
        set_menu_state(menu_state, menu_focus, MENU_CLOSED);
        return;
    }

    int x = 0;
    int y = 0;
    if (cell_from_point(game, window_width, window_height, px, py, &x, &y)) {
        *focus_x = x;
        *focus_y = y;
        if (button == SDL_BUTTON_RIGHT || *flag_mode) {
            game_toggle_flag(game, x, y);
        } else if (button == SDL_BUTTON_LEFT) {
            graphics_reveal_or_chord(game, x, y);
        }
        update_window_title(window, game, selection, *theme);
    }
}

static int run_graphics(BoardSelection selection, uint32_t seed, ThemeMode theme) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    Game game;
    game_init(&game, selection.config.width, selection.config.height, selection.config.mines, seed);
    int win_w = 0;
    int win_h = 0;
    window_size_for_game(&game, NULL, &win_w, &win_h);

    SDL_Window *window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          win_w, win_h,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        game_free(&game);
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        game_free(&game);
        SDL_Quit();
        return 1;
    }

    Textures textures;
    if (!load_textures(renderer, &textures)) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        game_free(&game);
        SDL_Quit();
        return 1;
    }
    apply_texture_theme(&textures, theme);

    int focus_x = 0;
    int focus_y = 0;
    MenuState menu_state = MENU_CLOSED;
    int menu_focus = 0;
    int flag_mode = 0;
    int running = 1;
    update_window_title(window, &game, &selection, theme);

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
#ifdef SDL_TOUCH_MOUSEID
                if (event.button.which == SDL_TOUCH_MOUSEID) {
                    continue;
                }
#endif
                handle_graphics_pointer(&game, window, &textures, &selection, seed, &theme,
                                        &menu_state, &menu_focus, &flag_mode, &focus_x, &focus_y,
                                        event.button.x, event.button.y, event.button.button);
            } else if (event.type == SDL_FINGERDOWN) {
                int window_width = 0;
                int window_height = 0;
                SDL_GetWindowSize(window, &window_width, &window_height);
                handle_graphics_pointer(&game, window, &textures, &selection, seed, &theme,
                                        &menu_state, &menu_focus, &flag_mode, &focus_x, &focus_y,
                                        (int)(event.tfinger.x * window_width),
                                        (int)(event.tfinger.y * window_height),
                                        SDL_BUTTON_LEFT);
            } else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                if (menu_state != MENU_CLOSED) {
                    if (key == SDLK_ESCAPE || key == SDLK_o || key == SDLK_TAB) {
                        set_menu_state(&menu_state, &menu_focus, MENU_CLOSED);
                    } else if (key == SDLK_q) {
                        running = 0;
                    } else if (key == SDLK_UP) {
                        menu_focus--;
                        clamp_menu_focus(menu_state, &menu_focus);
                    } else if (key == SDLK_DOWN) {
                        menu_focus++;
                        clamp_menu_focus(menu_state, &menu_focus);
                    } else if (key == SDLK_LEFT && menu_state != MENU_ROOT) {
                        set_menu_state(&menu_state, &menu_focus, MENU_ROOT);
                    } else if (key == SDLK_RIGHT && menu_state == MENU_ROOT && menu_focus >= 0 && menu_focus <= 2) {
                        invoke_graphics_menu_row(&game, window, &textures, &selection, seed, &theme,
                                                 &menu_state, &menu_focus, &flag_mode,
                                                 &focus_x, &focus_y, menu_focus);
                    } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                        invoke_graphics_menu_row(&game, window, &textures, &selection, seed, &theme,
                                                 &menu_state, &menu_focus, &flag_mode,
                                                 &focus_x, &focus_y, menu_focus);
                    }
                } else if (key == SDLK_o || key == SDLK_TAB) {
                    set_menu_state(&menu_state, &menu_focus, MENU_ROOT);
                } else if (key == SDLK_ESCAPE || key == SDLK_q) {
                    running = 0;
                } else if (key == SDLK_LEFT && focus_x > 0) {
                    focus_x--;
                } else if (key == SDLK_RIGHT && focus_x < game.width - 1) {
                    focus_x++;
                } else if (key == SDLK_UP && focus_y > 0) {
                    focus_y--;
                } else if (key == SDLK_DOWN && focus_y < game.height - 1) {
                    focus_y++;
                } else if (key == SDLK_SPACE || key == SDLK_RETURN) {
                    graphics_reveal_or_chord(&game, focus_x, focus_y);
                    update_window_title(window, &game, &selection, theme);
                } else if (key == SDLK_f) {
                    game_toggle_flag(&game, focus_x, focus_y);
                    update_window_title(window, &game, &selection, theme);
                } else if (key == SDLK_t) {
                    theme = theme == THEME_DARK ? THEME_LIGHT : THEME_DARK;
                    apply_texture_theme(&textures, theme);
                    update_window_title(window, &game, &selection, theme);
                } else if (key == SDLK_r) {
                    reset_graphics_game(&game, window, &selection, seed, &focus_x, &focus_y, theme);
                } else if (key == SDLK_1 || key == SDLK_s || key == SDLK_b) {
                    selection_set_preset(&selection, SIZE_SMALL, selection.difficulty);
                    reset_graphics_game(&game, window, &selection, seed, &focus_x, &focus_y, theme);
                } else if (key == SDLK_2 || key == SDLK_m || key == SDLK_i) {
                    selection_set_preset(&selection, SIZE_MEDIUM, selection.difficulty);
                    reset_graphics_game(&game, window, &selection, seed, &focus_x, &focus_y, theme);
                } else if (key == SDLK_3 || key == SDLK_l || key == SDLK_x) {
                    selection_set_preset(&selection, SIZE_LARGE, selection.difficulty);
                    reset_graphics_game(&game, window, &selection, seed, &focus_x, &focus_y, theme);
                } else if (key == SDLK_e) {
                    selection_set_preset(&selection, selection.level, MINE_EASY);
                    reset_graphics_game(&game, window, &selection, seed, &focus_x, &focus_y, theme);
                } else if (key == SDLK_n) {
                    selection_set_preset(&selection, selection.level, MINE_NORMAL);
                    reset_graphics_game(&game, window, &selection, seed, &focus_x, &focus_y, theme);
                } else if (key == SDLK_h) {
                    selection_set_preset(&selection, selection.level, MINE_HARD);
                    reset_graphics_game(&game, window, &selection, seed, &focus_x, &focus_y, theme);
                }
            }
        }

        render_graphics(window, renderer, &textures, &game, &selection, focus_x, focus_y, theme,
                        menu_state, menu_focus, flag_mode);
        SDL_Delay(16);
    }

    destroy_textures(&textures);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    game_free(&game);
    SDL_Quit();
    return 0;
}
#endif

static int assert_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "self-test failed: %s\n", message);
        return 0;
    }
    return 1;
}

static int self_test_first_reveal_safe(void) {
    Game game;
    int ok = 1;
    game_init(&game, 9, 9, 10, 123u);
    game_reveal(&game, 4, 4);
    ok &= assert_true(!cell_at(&game, 4, 4)->mine, "first reveal is safe");
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            ok &= assert_true(!cell_at(&game, 4 + dx, 4 + dy)->mine, "first reveal neighbor is safe");
        }
    }
    game_free(&game);
    return ok;
}

static int self_test_flag_and_loss(void) {
    Game game;
    int ok = 1;
    game_init(&game, 2, 2, 1, 1u);
    game.first_reveal = 0;
    game.status = GAME_PLAYING;
    cell_at(&game, 1, 1)->mine = 1;
    game_toggle_flag(&game, 0, 1);
    game_reveal(&game, 1, 1);
    ok &= assert_true(game.status == GAME_LOST, "loss state");
    ok &= assert_true(cell_at(&game, 1, 1)->hit, "hit mine marked");
    ok &= assert_true(cell_at(&game, 0, 1)->wrong, "wrong flag marked");
    game_free(&game);
    return ok;
}

static int self_test_win(void) {
    Game game;
    int ok = 1;
    game_init(&game, 2, 2, 1, 1u);
    game.first_reveal = 0;
    game.status = GAME_PLAYING;
    cell_at(&game, 1, 1)->mine = 1;
    cell_at(&game, 0, 0)->adjacent = 1;
    cell_at(&game, 1, 0)->adjacent = 1;
    cell_at(&game, 0, 1)->adjacent = 1;
    game_reveal(&game, 0, 0);
    game_reveal(&game, 1, 0);
    game_reveal(&game, 0, 1);
    ok &= assert_true(game.status == GAME_WON, "win state");
    ok &= assert_true(cell_at(&game, 1, 1)->flagged, "mine auto-flagged on win");
    game_free(&game);
    return ok;
}

static int run_self_tests(void) {
    int ok = 1;
    ok &= self_test_first_reveal_safe();
    ok &= self_test_flag_and_loss();
    ok &= self_test_win();
    if (ok) {
        printf("self-test passed\n");
        return 0;
    }
    return 1;
}

static void usage(void) {
    printf(APP_NAME " " APP_VERSION "\n");
    printf("Usage: clean-mines [options]\n");
    printf("  --level NAME    board size level: small, medium, or large\n");
    printf("  --difficulty NAME mine difficulty: easy, normal, or hard\n");
    printf("  --width N       board width, 2..60\n");
    printf("  --height N      board height, 2..40\n");
    printf("  --mines N       mine count\n");
    printf("  --seed N        deterministic seed\n");
    printf("  --graphics      run graphical mode");
#ifdef CLEAN_MINES_WITH_SDL
    printf("\n");
#else
    printf(" (not available in this build)\n");
#endif
    printf("  --theme NAME    graphical theme: light or dark\n");
    printf("  --dark          use dark graphical theme (default)\n");
    printf("  --light         use light graphical theme\n");
    printf("  --terminal      run terminal mode\n");
    printf("  --commands      use line-command terminal mode\n");
    printf("  --self-test     run built-in tests\n");
    printf("  --help          show help\n");
    printf("  --version       show version\n");
}

static int parse_int_arg(const char *value, int *out) {
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed < -2147483647L || parsed > 2147483647L) {
        return 0;
    }
    *out = (int)parsed;
    return 1;
}

static int parse_u32_arg(const char *value, uint32_t *out) {
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (end == value || *end != '\0' || parsed > 0xffffffffUL) {
        return 0;
    }
    *out = (uint32_t)parsed;
    return 1;
}

static int parse_theme_arg(const char *value, ThemeMode *out) {
    if (strcmp(value, "light") == 0) {
        *out = THEME_LIGHT;
        return 1;
    }
    if (strcmp(value, "dark") == 0) {
        *out = THEME_DARK;
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    SizeLevel level = SIZE_SMALL;
    MineDifficulty difficulty = MINE_NORMAL;
    int width = 0;
    int height = 0;
    int mines = 0;
    int custom_width = 0;
    int custom_height = 0;
    int custom_mines = 0;
    uint32_t seed = 0u;
    int force_terminal = 0;
    int force_graphics = 0;
    int force_commands = 0;
    ThemeMode theme = THEME_DARK;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            printf(APP_VERSION "\n");
            return 0;
        }
        if (strcmp(argv[i], "--self-test") == 0) {
            return run_self_tests();
        }
        if (strcmp(argv[i], "--terminal") == 0) {
            force_terminal = 1;
            continue;
        }
        if (strcmp(argv[i], "--commands") == 0 || strcmp(argv[i], "--command-mode") == 0) {
            force_terminal = 1;
            force_commands = 1;
            continue;
        }
        if (strcmp(argv[i], "--graphics") == 0) {
            force_graphics = 1;
            continue;
        }
        if (strcmp(argv[i], "--dark") == 0) {
            theme = THEME_DARK;
            continue;
        }
        if (strcmp(argv[i], "--light") == 0) {
            theme = THEME_LIGHT;
            continue;
        }
        if (strcmp(argv[i], "--theme") == 0) {
            if (i + 1 >= argc || !parse_theme_arg(argv[++i], &theme)) {
                fprintf(stderr, "invalid theme; expected light or dark\n");
                return 2;
            }
            continue;
        }
        if (strcmp(argv[i], "--level") == 0 || strcmp(argv[i], "--size") == 0) {
            char value[32];
            if (i + 1 >= argc) {
                fprintf(stderr, "missing value for %s\n", argv[i]);
                return 2;
            }
            snprintf(value, sizeof(value), "%s", argv[++i]);
            lowercase_text(value);
            if (!parse_size_level_arg(value, &level)) {
                fprintf(stderr, "invalid level; expected small, medium, or large\n");
                return 2;
            }
            continue;
        }
        if (strcmp(argv[i], "--difficulty") == 0) {
            char value[32];
            if (i + 1 >= argc) {
                fprintf(stderr, "missing value for %s\n", argv[i]);
                return 2;
            }
            snprintf(value, sizeof(value), "%s", argv[++i]);
            lowercase_text(value);
            if (!parse_mine_difficulty_arg(value, &difficulty)) {
                fprintf(stderr, "invalid difficulty; expected easy, normal, or hard\n");
                return 2;
            }
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "missing value for %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--width") == 0) {
            if (!parse_int_arg(argv[++i], &width)) {
                fprintf(stderr, "invalid width\n");
                return 2;
            }
            custom_width = 1;
        } else if (strcmp(argv[i], "--height") == 0) {
            if (!parse_int_arg(argv[++i], &height)) {
                fprintf(stderr, "invalid height\n");
                return 2;
            }
            custom_height = 1;
        } else if (strcmp(argv[i], "--mines") == 0) {
            if (!parse_int_arg(argv[++i], &mines)) {
                fprintf(stderr, "invalid mines\n");
                return 2;
            }
            custom_mines = 1;
        } else if (strcmp(argv[i], "--seed") == 0) {
            if (!parse_u32_arg(argv[++i], &seed)) {
                fprintf(stderr, "invalid seed\n");
                return 2;
            }
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 2;
        }
    }

    Difficulty preset = preset_config(level, difficulty);
    if (!custom_width) {
        width = preset.width;
    }
    if (!custom_height) {
        height = preset.height;
    }
    if (!custom_mines) {
        mines = preset.mines;
    }

    if (!validate_config(width, height, mines)) {
        fprintf(stderr, "invalid board: width=%d height=%d mines=%d\n", width, height, mines);
        return 2;
    }

    BoardSelection selection;
    selection_set_preset(&selection, level, difficulty);
    if (custom_width || custom_height || custom_mines) {
        selection_set_custom(&selection, width, height, mines);
    }

    if (force_terminal && force_graphics) {
        fprintf(stderr, "--terminal and --graphics cannot be used together\n");
        return 2;
    }

#ifdef CLEAN_MINES_WITH_SDL
    if (!force_terminal) {
        return run_graphics(selection, seed, theme);
    }
#else
    (void)theme;
    if (force_graphics) {
        fprintf(stderr, "graphics mode is not available in this build\n");
        return 2;
    }
#endif

    if (force_commands) {
        return interactive(selection, seed);
    }
    return terminal_controller(selection, seed);
}
