// tetris8x8.c — 8x8 Tetris with Menu/Options; bottom row clearable
// build: gcc -O2 -std=c11 tetris8x8.c -o tetris -lm
// run:   ./tetris
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <math.h>
#include <stdbool.h>
#include <sys/types.h>

// ============================= INPUT =============================
enum {
    KEY_LEFT_MOVE = 10,
    KEY_RIGHT_MOVE,
    KEY_ROTATE,
    KEY_SPACE,
    KEY_ARROW_LEFT,
    KEY_ARROW_RIGHT,
    KEY_ESC
};

// for the menu
typedef enum { ST_MENU, ST_PLAYING, ST_OPTIONS, ST_EXIT } GameState;

// ============================= TIMING ============================
static long now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec*1000LL + ts.tv_nsec/1000000LL);
}

// ============================= BOARD/STATE =======================
#define W 8
#define H 8

// gameplay/options state (globals used across functions)
static int x = 2;                  // spawn column 0..4
static int new_block = 1;          // flag to spawn new piece
static int score = 0;

static int level = 0, lines_total = 0;
static int fall_interval_ms = 1000;
static long fall_timer_ms = 0;
static long lock_timer_ms = 0;

static int opt_start_level = 0;       // Options menu
static int opt_soft_drop_enabled = 0; // Options menu

// board
static char one[W]   = {'0','0','0','0','0','0','0','0'};
static char two[W]   = {'0','0','0','0','0','0','0','0'};
static char three[W] = {'0','0','0','0','0','0','0','0'};
static char four[W]  = {'0','0','0','0','0','0','0','0'};
static char five[W]  = {'0','0','0','0','0','0','0','0'};
static char six[W]   = {'0','0','0','0','0','0','0','0'};
static char seven[W] = {'0','0','0','0','0','0','0','0'};
static char eight[W] = {'0','0','0','0','0','0','0','0'}; // clearable bottom
static char *rows[]  = { one, two, three, four, five, six, seven, eight };

static void clear_board(void) {
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c)
            rows[r][c] = '0';
}

static void print_pixels(void) {
    printf("Score: %d\n", score);
    for (int i = 0; i < W; ++i) printf("%c", one[i]);   printf("\n");
    for (int i = 0; i < W; ++i) printf("%c", two[i]);   printf("\n");
    for (int i = 0; i < W; ++i) printf("%c", three[i]); printf("\n");
    for (int i = 0; i < W; ++i) printf("%c", four[i]);  printf("\n");
    for (int i = 0; i < W; ++i) printf("%c", five[i]);  printf("\n");
    for (int i = 0; i < W; ++i) printf("%c", six[i]);   printf("\n");
    for (int i = 0; i < W; ++i) printf("%c", seven[i]); printf("\n");
    for (int i = 0; i < W; ++i) printf("%c", eight[i]); printf("\n");
}
static int has_active_piece(void) {
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c)
            if (rows[r][c] == '1') return 1;
    return 0;
}

// ============================= RNG ===============================
static int roll_dn(int n) {                        // unbiased 0..n-1
    int limit = RAND_MAX - (RAND_MAX % n), r;
    do { r = rand(); } while (r >= limit);
    return r % n;
}
static unsigned int get_seed(void) {               // read seed once
    unsigned int s;
    printf("Enter a seed: ");
    if (scanf("%u", &s) != 1) { fprintf(stderr, "Invalid input\n"); exit(1); }
    return s;
}
static void flush_stdin_line(void) {               // eat leftover newline
    int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}
}

// ============================= RAW INPUT =========================
static struct termios oldt;
static void term_raw_enable(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &oldt);
    t = oldt;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN]  = 0;  // non-blocking
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}
static void term_raw_disable(void) { tcsetattr(STDIN_FILENO, TCSANOW, &oldt); }

static int poll_key(void) {
    unsigned char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n <= 0) return 0; // no key

    if (ch == ' ') return KEY_SPACE;
    if (ch == 'a' || ch == 'A') return KEY_LEFT_MOVE;
    if (ch == 'd' || ch == 'D') return KEY_RIGHT_MOVE;
    if (ch == 'w' || ch == 'W') return KEY_ROTATE;

    if (ch == 0x1B) { // ESC or an escape sequence
        unsigned char b1=0, b2=0;
        ssize_t n1 = read(STDIN_FILENO, &b1, 1);
        if (n1 <= 0) return KEY_ESC; // plain ESC
        ssize_t n2 = read(STDIN_FILENO, &b2, 1);
        if (n2 <= 0) return KEY_ESC;
        if (b1 == '[' && b2 == 'D') return KEY_ARROW_LEFT;
        if (b1 == '[' && b2 == 'C') return KEY_ARROW_RIGHT;
        return KEY_ESC;
    }
    // menu letters handled directly where used
    return ch; // return raw char for 'p','o','q','b','s'
}

// ========== Active-block bounds + rotation helpers (4x4 box) ==========
typedef struct { int x0, y0, x1, y1, found; } AABB;

static AABB get_active_block_bounds(void) {
    AABB b = {W, H, -1, -1, 0};
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c)
            if (rows[r][c] == '1') {
                if (c < b.x0) b.x0 = c;
                if (r < b.y0) b.y0 = r;
                if (c > b.x1) b.x1 = c;
                if (r > b.y1) b.y1 = r;
                b.found = 1;
            }
    return b;
}
static void transpose4(char t[4][4]) {
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j) {
            char tmp = t[i][j]; t[i][j] = t[j][i]; t[j][i] = tmp;
        }
}
static void reverse_rows4(char t[4][4]) {
    for (int i = 0; i < 4; ++i) {
        int L = 0, R = 3;
        while (L < R) { char tmp = t[i][L]; t[i][L] = t[i][R]; t[i][R] = tmp; ++L; --R; }
    }
}
static void reverse_cols4(char t[4][4]) {
    for (int j = 0; j < 4; ++j) {
        int T = 0, B = 3;
        while (T < B) { char tmp = t[T][j]; t[T][j] = t[B][j]; t[B][j] = tmp; ++T; --B; }
    }
}
static void rotate_left4(char t[4][4])  { transpose4(t); reverse_cols4(t); }
static void rotate_right4(char t[4][4]) { transpose4(t); reverse_rows4(t); }

static void extract_active_to_4x4(char out[4][4], AABB b) {
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            out[r][c] = ' ';
    if (!b.found) return;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            int gr = b.y0 + r, gc = b.x0 + c;
            if (gr >= 0 && gr < H && gc >= 0 && gc < W && rows[gr][gc] == '1')
                out[r][c] = '1';
        }
}
static int try_apply_rotated_4x4(char rot[4][4], AABB b) {
    if (!b.found) return 0;
    char tmp[H][W];
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c)
            tmp[r][c] = rows[r][c];
    for (int r = b.y0; r <= b.y1; ++r)
        for (int c = b.x0; c <= b.x1; ++c)
            if (tmp[r][c] == '1') tmp[r][c] = '0';
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (rot[r][c] == '1') {
                int gr = b.y0 + r, gc = b.x0 + c;
                if (gr < 0 || gr >= H || gc < 0 || gc >= W) return 0;
                if (tmp[gr][gc] == '2') return 0;
            }
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c)
            rows[r][c] = tmp[r][c];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (rot[r][c] == '1') rows[b.y0 + r][b.x0 + c] = '1';
    return 1;
}
static void rotate_active_block(int dir) {
    AABB b = get_active_block_bounds();
    if (!b.found) return;
    char box[4][4], rot[4][4];
    extract_active_to_4x4(box, b);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            rot[r][c] = box[r][c];
    if (dir < 0) rotate_left4(rot); else rotate_right4(rot);
    (void)try_apply_rotated_4x4(rot, b);
}

// ======================= Movement & Gravity ======================
static int can_move_horiz(int dx) {
    for (int r = 0; r < H; ++r) {
        for (int c = 0; c < W; ++c) {
            if (rows[r][c] != '1') continue;
            int nc = c + dx;
            if (nc < 0 || nc >= W) return 0;       // wall
            if (rows[r][nc] == '2') return 0;      // settled collision
        }
    }
    return 1;
}
static void move_piece_horiz(int dx) {
    if (!dx || !can_move_horiz(dx)) return;
    if (dx > 0) {
        for (int r = 0; r < H; ++r)
            for (int c = W-1; c >= 0; --c)
                if (rows[r][c] == '1') { rows[r][c+1] = '1'; rows[r][c] = '0'; }
    } else {
        for (int r = 0; r < H; ++r)
            for (int c = 0; c < W; ++c)
                if (rows[r][c] == '1') { rows[r][c-1] = '1'; rows[r][c] = '0'; }
    }
}
static int can_piece_fall(void) {
    for (int r = H-1; r >= 0; --r)
        for (int c = 0; c < W; ++c)
            if (rows[r][c] == '1') {
                if (r + 1 >= H) return 0;          // bottom wall
                if (rows[r + 1][c] == '2') return 0;
            }
    return 1;
}
static void move_piece_down(void) {
    for (int r = H-1; r >= 0; --r)
        for (int c = 0; c < W; ++c)
            if (rows[r][c] == '1') { rows[r+1][c] = '1'; rows[r][c] = '0'; }
}

// =============== Line clear + collapse + scoring/level ===========
static int clear_full_lines_and_collapse(void) {
    int cleared = 0;
    for (int r = H-1; r >= 0; --r) { // include bottom row
        int cnt = 0;
        for (int c = 0; c < W; ++c) if (rows[r][c] == '2') cnt++;
        if (cnt == W) {
            for (int rr = r; rr > 0; --rr)
                for (int c = 0; c < W; ++c)
                    rows[rr][c] = rows[rr-1][c];
            for (int c = 0; c < W; ++c) rows[0][c] = '0';
            ++cleared;
            ++r; // re-check same row after collapse
        }
    }
    return cleared;
}
static void apply_scoring_and_level(int lines_cleared) {
    if (lines_cleared == 1) score += 100;
    else if (lines_cleared == 2) score += 300;
    else if (lines_cleared == 3) score += 500;
    else if (lines_cleared >= 4) score += 800;

    lines_total += lines_cleared;
    int new_level = lines_total / 10; // 10 lines per level
    if (new_level != level) {
        level = new_level;
        double ms = 1000.0 * pow(0.90, (double)level);
        if (ms < 80.0) ms = 80.0;
        fall_interval_ms = (int)ms;
    }
}
static void lock_piece(void) {
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c)
            if (rows[r][c] == '1') rows[r][c] = '2';
    int lines = clear_full_lines_and_collapse();
    apply_scoring_and_level(lines);
    new_block = 1;
    lock_timer_ms = 0;
    fall_timer_ms = 0;
}

// ============================== Shapes ===========================
static const char *SHAPE_NAMES[] = {
    "square", "Lleft", "Lright", "zigzagleft", "zigzagright", "straight"
};
static const char SHAPES[6][4][4] = {
    { {'1','1',' ',' '}, {'1','1',' ',' '}, {' ',' ',' ',' '}, {' ',' ',' ',' '} }, // O
    { {'1',' ',' ',' '}, {'1',' ',' ',' '}, {'1','1',' ',' '}, {' ',' ',' ',' '} }, // J
    { {' ','1',' ',' '}, {' ','1',' ',' '}, {'1','1',' ',' '}, {' ',' ',' ',' '} }, // L
    { {' ','1','1',' '}, {'1','1',' ',' '}, {' ',' ',' ',' '}, {' ',' ',' ',' '} }, // S
    { {'1','1',' ',' '}, {' ','1','1',' '}, {' ',' ',' ',' '}, {' ',' ',' ',' '} }, // Z
    { {'1',' ',' ',' '}, {'1',' ',' ',' '}, {'1',' ',' ',' '}, {'1',' ',' ',' '} }, // I (vertical)
};

// ============================== Spawn ============================
static void spawn_random_block(void) {
    if (!new_block) return;
    if (x < 0) x = 0; if (x > 4) x = 4;

    int idx = roll_dn(6);
    const char (*shape)[4] = SHAPES[idx];

    // game over check: overlap with settled in the 4x4 spawn area (rows 0..3)
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (shape[r][c] == '1' && rows[r][x + c] == '2') {
                print_pixels();
                printf("Game Over (blocked by settled cells)\n");
                exit(0);
            }

    // clear any stray active cells
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c)
            if (rows[r][c] == '1') rows[r][c] = '0';

    // place the active shape at top (rows 0..3)
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (shape[r][c] == '1') rows[r][x + c] = '1';

    new_block = 0;
    printf("[spawned: %s]\n", SHAPE_NAMES[idx]);
}

// ========================= Gravity (time-based) ==================
static void gravity_step(int dt_ms) {
    if (!has_active_piece()) return;

    fall_timer_ms += dt_ms;
    while (fall_timer_ms >= fall_interval_ms) {
        fall_timer_ms -= fall_interval_ms;

        if (can_piece_fall()) {
            move_piece_down();
            if (can_piece_fall()) lock_timer_ms = 0; // still airborne afterwards
        } else {
            lock_timer_ms += fall_interval_ms;
            if (lock_timer_ms >= 500) { // LOCK_DELAY_MS
                lock_piece();
                return;
            }
        }
    }
}

// ======================= Menu / Options helpers ==================
static void draw_menu(void) {
    printf("==== T E T R I S ====\n");
    printf("[p] Play\n");
    printf("[o] Options\n");
    printf("[q] Quit\n");
    printf("\nControls in-game: A/D move, W rotate, Space = soft drop (if enabled), ESC = menu\n\n");
}
static void draw_options(int start_level, int soft_drop) {
    printf("==== O P T I O N S ====\n");
    printf("Starting Level: %d  (←/→ to change)\n", start_level);
    printf("Soft Drop: %s      (s to toggle)\n", soft_drop ? "ON" : "OFF");
    printf("\n[b] Back\n\n");
}

// Reset game using current options (MUST be after globals)
static void reset_game_with_options(void) {
    clear_board();
    score = 0;
    lines_total = 0;
    level = opt_start_level;

    double ms = 1000.0 * pow(0.90, (double)level);
    if (ms < 80.0) ms = 80.0;
    fall_interval_ms = (int)ms;

    fall_timer_ms = 0;
    lock_timer_ms = 0;
    new_block = 1;
    x = 2;
}

// ================================ MAIN ===========================
int main(void) {
    unsigned int s = get_seed();
    srand(s ? s : 1);
    flush_stdin_line();

    term_raw_enable();
    atexit(term_raw_disable);

    GameState state = ST_MENU;
    long prev = now_ms();

    while (state != ST_EXIT) {
        long t = now_ms();
        int dt = (int)(t - prev);
        if (dt < 0) dt = 0;
        prev = t;

        // Clear screen (ANSI)
        printf("\x1b[2J\x1b[H");

        switch (state) {
            case ST_MENU: {
                draw_menu();
                for (;;) {
                    int key = poll_key();
                    if (!key) break;
                    if (key == 'p' || key == 'P') {
                        reset_game_with_options();
                        state = ST_PLAYING;
                        break;
                    } else if (key == 'o' || key == 'O') {
                        state = ST_OPTIONS;
                        break;
                    } else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                        state = ST_EXIT;
                        break;
                    }
                }
                struct timespec ts = { .tv_sec = 0, .tv_nsec = 20*1000*1000 };
                nanosleep(&ts, NULL);
            } break;

            case ST_OPTIONS: {
                draw_options(opt_start_level, opt_soft_drop_enabled);
                for (;;) {
                    int key = poll_key();
                    if (!key) break;
                    if (key == KEY_ARROW_LEFT && opt_start_level > 0)  opt_start_level--;
                    if (key == KEY_ARROW_RIGHT && opt_start_level < 20) opt_start_level++;
                    if (key == 's' || key == 'S') opt_soft_drop_enabled ^= 1;
                    if (key == 'b' || key == 'B' || key == KEY_ESC) { state = ST_MENU; break; }
                }
                struct timespec ts = { .tv_sec = 0, .tv_nsec = 20*1000*1000 };
                nanosleep(&ts, NULL);
            } break;

            case ST_PLAYING: {
                // input
                for (;;) {
                    int key = poll_key();
                    if (!key) break;
                    if (key == KEY_LEFT_MOVE)       move_piece_horiz(-1);
                    else if (key == KEY_RIGHT_MOVE) move_piece_horiz(+1);
                    else if (key == KEY_ROTATE)     rotate_active_block(+1);
                    else if (key == KEY_SPACE && opt_soft_drop_enabled && can_piece_fall())
                        move_piece_down();
                    else if (key == KEY_ESC) { state = ST_MENU; break; }
                }

                if (state == ST_PLAYING) {
                    gravity_step(dt);
                    if (!has_active_piece() && new_block) spawn_random_block();

                    print_pixels();
                    printf("Level: %d  Fall: %d ms  SoftDrop:%s\n\n",
                           level, fall_interval_ms, opt_soft_drop_enabled ? "ON" : "OFF");

                    struct timespec ts = { .tv_sec = 0, .tv_nsec = 16*1000*1000 };
                    nanosleep(&ts, NULL);
                }
            } break;

            default: state = ST_EXIT; break;
        }
    }
    return 0;
}
