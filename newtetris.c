// Tetris for the DE10-Lite running Riscv

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <sys/types.h>
// #include "vga.h"

/* Written by Konrad Rosenberg 2025 */



// BEGIN VGA

#include "font.h"
#define SCREEN_W 320
#define SCREEN_H 240

volatile int *VGA = (volatile int*) 0x08000000;
volatile int *VGA_CTRL = (volatile int*) 0x04000100;
volatile int *VGA_BUFFER = (volatile int*) 0x04000104;

struct vector2 {
 int x;
 int y;
};


void vga_push(void) {
    VGA_CTRL[1] = (uint32_t)VGA_BUFFER; // pekar ut vår framebuffer
    VGA_CTRL[0] = 0; // “kicka igång” visningen

    __asm__ volatile ("" ::: "memory");
}

void set_pixel(int x, int y, char color){
 
  volatile char *pixel = (volatile char *)((uintptr_t)0x08000000 + (y * SCREEN_W) + x);
  *pixel = color;
  return;
}

void set_all_pixels(char color){
    for(int x = 0; x < SCREEN_W; x++){
        for(int y = 0; y < SCREEN_H; y++){
            set_pixel(x, y, color);
        }
    }
}


struct vector2 draw_string(char* string, int start_x, int start_y, char foreground, char background) {
  int cur_x = start_x;
  int cur_y = start_y;
 
  while (*string != 0) {
    if (*string == 10) { // new line terminator
      cur_y += FONT_H + SPACING_H;
    }
    else if (*string == 13) { // carriage return
      cur_x = start_x;
    }
    else {
      unsigned char *cur_shape = font_shapes[*string - 32];
      for (int i = 0; i < FONT_H; i++) {
        unsigned int row_hex = cur_shape[i];
        for (int j = 0; j <= FONT_W; j++) {
          if (row_hex & 0x1)
            set_pixel(cur_x + j, cur_y + i, foreground);
          else {
            set_pixel(cur_x + j, cur_y + i, background);
          }
          row_hex = row_hex >> 1;
        }
      }
      cur_x += FONT_W + SPACING_W;
    }
    string++;
  }
 
  struct vector2 last_pos = {cur_x, cur_y};
  return last_pos;
}


// END VGA


// ============================= INPUT =============================
enum {
    NO_KEY,
    KEY_LEFT_MOVE,
    KEY_RIGHT_MOVE,
    KEY_ROTATE,
    KEY_SPACE,
    KEY_ARROW_LEFT,
    KEY_ARROW_RIGHT,
    KEY_ESC
};


void handle_interrupt(unsigned cause)
{}

// top-left of the active piece's 4x4 frame; -1 = none
static int ax = -1, ay = -1;


// ============================= BASIC FUNCTIONS ============================


void int2asc(int num, char *buffer) {
    int i = 0;

    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    while (num != 0) {
        int rem = num % 10;
        buffer[i++] = rem + '0';
        num = num / 10;
    }

    buffer[i] = '\0';

    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }
}

// ============================= TIMING ============================
long clockms = 0;

volatile unsigned int *status  = (volatile unsigned int*)0x04000020;
volatile unsigned int *control = (volatile unsigned int*)0x04000024;
volatile unsigned int *periodl = (volatile unsigned int*)0x04000028;
volatile unsigned int *periodh = (volatile unsigned int*)0x0400002C;

void timeinit() {
    int period = 3000; // 1 millisecond period
    *periodl = period & 0xffff;
    *periodh = (period >> 16) & 0xFFFF;
    *control = 0x6;
}

// ============================= BOARD/STATE =======================
#define W 10
#define H 20

// gameplay/options state (globals used across functions)

typedef enum { ST_MENU, ST_PLAYING, ST_OPTIONS, ST_EXIT } GameState;
GameState state = ST_MENU;

static int new_block;          // flag to spawn new piece
static int score = 0;

static int level = 0, lines_total = 0;
static int fall_interval_ms = 1000;
static long lock_timer_ms = 0;

static int opt_start_level = 0;       // Options menu
static int opt_soft_drop_enabled = 0; // Options menu

// board
static char board[H][W];

static void clear_board(void) {
    for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++)
            board[r][c] = 0;
}

static int has_active_piece(void) {
    for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++)
            if (board[r][c] == 1) return 1;
    return 0;
}


// ============================= RAW INPUT =========================
int input_limit_ms = 100;

int get_sw(void) {
  volatile unsigned int *switches = (volatile unsigned int*)0x04000010;
  return *switches & 0x3ff; // 0011 1111 1111
}

int get_btn(void) {
  volatile unsigned int *button = (volatile unsigned int*)0x040000d0;
  return *button & 0x1;
}

int poll_key(void) {
    volatile unsigned int switches = get_sw();
    volatile static int prev_switch;

    int key = 0;

    char buffer[100];

    int2asc(switches, buffer);
    draw_string(buffer, 200, 150, 255, 0);

    if ((switches & 0x1) != (prev_switch & 0x1)) {
        key = KEY_RIGHT_MOVE;
    } else if ((switches & 0x2) != (prev_switch & 0x2))  {
        key = KEY_LEFT_MOVE;
    } else if ((switches & 0x4) != (prev_switch & 0x4))  {
        key = KEY_ROTATE;
    } else if ((switches & 0x512) != (prev_switch & 0x512))  {
        key = KEY_ESC;
    }

    prev_switch = switches;
    return key;
}

// ============================= RNG ===============================

static int seed;

void snrand(unsigned int s) {
    seed = s;
}

unsigned int nrand(void) {
    // pseudo-random algorithm from sources online
    seed = seed * 1103515245 + 12345;
    return (seed / 65536) % 32768;
}

static unsigned int get_seed(void) {               // read seed once
    draw_string("Set a seed on the switches and press button to continue!: ", 15, 90, 255, 0);
    
    while (!get_btn()) {}

    return get_sw();
}


// ========== Active-block bounds + rotation helpers (4x4 box) ==========
typedef struct { int x0, y0, x1, y1, found; } AABB;
static void extract_active_frame_4(char out[4][4]) {
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            int gr = ay + r, gc = ax + c;
            out[r][c] = (gr >= 0 && gr < H && gc >= 0 && gc < W && board[gr][gc] == 1) ? 1 : 0;
        }
}

static int can_place_frame_4(char m[4][4], int fx, int fy) {
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (m[r][c]) {
                int gr = fy + r, gc = fx + c;
                if (gr < 0 || gr >= H || gc < 0 || gc >= W) return 0;
                if (board[gr][gc] == 2) return 0;
            }
    return 1;
}

static void write_frame_4(char m[4][4], int fx, int fy) {
    // clear old active cells inside (fx,fy) 4x4, then write new
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            int gr = fy + r, gc = fx + c;
            if (gr >= 0 && gr < H && gc >= 0 && gc < W && board[gr][gc] == 1)
                board[gr][gc] = 0;
        }
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (m[r][c]) board[fy + r][fx + c] = 1;
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
static void rotate_left4 (char t[4][4]) { transpose4(t); reverse_cols4(t); }
static void rotate_right4(char t[4][4]) { transpose4(t); reverse_rows4(t); }

static void rotate_active_block(int dir) {
    if (ax == -1 || ay == -1) return; // no active frame

    char box[4][4], rot[4][4];
    extract_active_frame_4(box);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            rot[r][c] = box[r][c];

    if (dir < 0) rotate_left4(rot); else rotate_right4(rot);

    // tiny SRS-ish wall kicks: try in-place, then ±1 x, then -1 y
    const int kicks[][2] = { {0,0}, {+1,0}, {-1,0}, {0,-1} };
    for (int i = 0; i < 4; ++i) {
        int nfx = ax + kicks[i][0];
        int nfy = ay + kicks[i][1];
        if (can_place_frame_4(rot, nfx, nfy)) {
            // wipe current active cells of the WHOLE board first
            for (int r = 0; r < H; ++r)
                for (int c = 0; c < W; ++c)
                    if (board[r][c] == 1) board[r][c] = 0;
            write_frame_4(rot, nfx, nfy);
            ax = nfx; ay = nfy;   // update frame if we kicked
            return;
        }
    }
    // if all kicks fail, keep orientation unchanged
}


// ======================= Movement & Gravity ======================
static int can_move_horiz(int dx) {
    for (int r = 0; r < H; ++r) {
        for (int c = 0; c < W; ++c) {
            if (board[r][c] != 1) continue;
            int nc = c + dx;
            if (nc < 0 || nc >= W) return 0;       // wall
            if (board[r][nc] == 2) return 0;      // settled collision
        }
    }
    return 1;
}
static void move_piece_horiz(int dx) {
    if (!dx || !can_move_horiz(dx)) return;
    if (dx > 0) {
        for (int r = 0; r < H; r++)
            for (int c = W-1; c >= 0; c--)
                if (board[r][c] == 1) { board[r][c+1] = 1; board[r][c] = 0; }
    } else {
        for (int r = 0; r < H; r++)
            for (int c = 0; c < W; c++)
                if (board[r][c] == 1) { board[r][c-1] = 1; board[r][c] = 0; }
    }
    if (ax != -1) ax += dx;  // keep frame in sync
}

static int can_piece_fall(void) {
    for (int r = H-1; r >= 0; r--)
        for (int c = 0; c < W; c++)
            if (board[r][c] == 1) {
                if (r + 1 >= H) {
                    return 0;
                }          // bottom wall
                if (board[r + 1][c] == 2) {
                    return 0;
                }
            }
    return 1;
}
static void move_piece_down(void) {
    for (int r = H-1; r >= 0; r--)
        for (int c = 0; c < W; c++)
            if (board[r][c] == 1) { board[r+1][c] = 1; board[r][c] = 0; }
    if (ay != -1) ay += 1;    // keep frame in sync
}

// =============== Line clear + collapse + scoring/level ===========
static int clear_full_lines_and_collapse(void) {
    int cleared = 0;
    for (int r = H-1; r >= 0; --r) { // include bottom row
        int cnt = 0;
        for (int c = 0; c < W; ++c) if (board[r][c] == 2) cnt++;
        if (cnt == W) {
            for (int rr = r; rr > 0; --rr)
                for (int c = 0; c < W; ++c)
                    board[rr][c] = board[rr-1][c];
            for (int c = 0; c < W; ++c) board[0][c] = 0;
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
    level = lines_total / 10;
    double ms = 1000.0 * 0.90 * (double)level; //double ms = 1000.0 * pow(0.90, (double)level);
    if (ms < 80.0) ms = 80.0;
    fall_interval_ms = (int)ms;
}
static void lock_piece(void) {
    for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++)
            if (board[r][c] == 1) board[r][c] = 2;
    int lines = clear_full_lines_and_collapse();
    apply_scoring_and_level(lines);
    new_block = 1;
    lock_timer_ms = 0;
    ax = ay = -1;           // clear active 4x4 frame
}


// ============================== Shapes ===========================
static const char *SHAPE_NAMES[] = {
    "square", "l-right", "l-left", "zigright", "zigleft", "straight", "tee-block"
};

static const char SHAPES[7][4][4] = {  // <— was [6], must be [7]
    { {1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} }, // O
    { {1,0,0,0}, {1,0,0,0}, {1,1,0,0}, {0,0,0,0} }, // L
    { {0,1,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,0,0} }, // J
    { {0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} }, // S
    { {1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} }, // Z
    { {1,0,0,0}, {1,0,0,0}, {1,0,0,0}, {1,0,0,0} }, // I (vertical)
    { {0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, // T
};


// ============================== Spawn ============================

unsigned int next_block;

static void spawn_random_block(void) {
    unsigned int spawn_x = nrand() % (W - 3); // ensure room for 4-wide frame
    unsigned int randshape = next_block;
    next_block = nrand() % 7;
    const char (*shape)[4] = SHAPES[randshape];

    ax = (int)spawn_x;
    ay = 0;

    // game over if the 4×4 frame collides with settled cells where shape has 1s
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (shape[r][c] == 1 && board[ay + r][ax + c] == 2) {
                draw_string("Game Over (blocked by settled cells)", 50, 50, 255, 0);
                state = ST_EXIT;
                return;
            }

    // place the active shape into the 4×4 frame
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (shape[r][c] == 1) board[ay + r][ax + c] = 1;

    // (Optional) remove this test line fill from reset:
    // for (int i = 0; i < 10; i++) { board[18][i] = 1; }
}

// ========================= Gravity (time-based) ==================
static void gravity_step() {
    if (!has_active_piece()) return;
    if (can_piece_fall()) {
        move_piece_down();
//        if (can_piece_fall()) lock_timer_ms = 0; // still airborne afterwards
    } else {
        lock_timer_ms += fall_interval_ms;
        if (lock_timer_ms >= 50) { // LOCK_TIMER_MS
            lock_piece();
            return;
        }
    }
}

// ======================= Menu / Options helpers ==================
static void draw_menu(void) {
    draw_string("==== T E T R I S ====\n\r[p] Play\n\r[o] Options\n\r[q] Quit\n\n\rControls in-game:\n\rA/D move,\n\rW rotate,\n\rSpace = soft drop (if enabled),\n\rESC = menu", 15, 15, 255, 0);
}

static void draw_options(int start_level, int soft_drop) {
    struct vector2 last_pos = draw_string("==== O P T I O N S ====\n\rStarting Level: ", 15, 15+150, 255, 0);
    
    char buffer[100];
    int2asc(start_level, buffer);
    last_pos = draw_string(buffer, last_pos.x, last_pos.y, 255, 0);

    last_pos = draw_string("(←/→ to change)\n\rSoft Drop: ", last_pos.x, last_pos.y, 255, 0);
    last_pos = draw_string(soft_drop ? "ON" : "OFF", last_pos.x, last_pos.y, 255, 0);
    draw_string("(s to toggle)\n\n\r[b] Back\n\n\r", last_pos.x, last_pos.y, 255, 0);

}

// Reset game using current options (MUST be after globals)
static void reset_game_with_options(void) {
    clear_board();
    score = 0;
    lines_total = 0;
    level = opt_start_level;

    double ms = 1000.0 * 0.90 * (double)level; // double ms = 1000.0 * pow(0.90, (double)level);
    if (ms < 80.0) ms = 80.0;
    fall_interval_ms = (int)ms;

    lock_timer_ms = 0;
    new_block = 1;
    next_block = nrand() % 7;

    for (int i = 0; i < 10; i++) {
        board[18][i] = 1;
    }
}

static void draw_block(int x, int y, int type) {
    char colors[] = {1, 50, 150};
    int box_x = 50; // these define the absolute position of the top left corner
    int box_y = 50;

    int size_xy = 8;

    for (int i = 0; i < size_xy; i++)
        for (int j = 0; j < size_xy; j++)
            set_pixel(box_x + (x * size_xy + i), box_y + (y * size_xy + j), colors[type]);
}

static int refresh_rate_ms = 20;

static void draw(void) {
    set_all_pixels(0);
    struct vector2 last_pos;

    char buffer[100];
    int2asc(score, buffer);

    last_pos = draw_string("Score: ", 200, 40, 255, 0);
    draw_string(buffer, last_pos.x, last_pos.y, 255, 0);

    last_pos = draw_string("Level: ", 200, 80, 255, 0);
    int2asc(level, buffer);
    last_pos = draw_string(buffer, last_pos.x, last_pos.y, 255, 0);

    last_pos = draw_string("\nFall (ms): ", 200, last_pos.y, 255, 0);
    int2asc(fall_interval_ms, buffer);
    last_pos = draw_string(buffer, last_pos.x, last_pos.y, 255, 0);

    last_pos = draw_string("\nNEXT: ", 200, last_pos.y, 255, 0);
    last_pos = draw_string(SHAPE_NAMES[next_block], last_pos.x, last_pos.y, 255, 0);
    
    // draw blocks
    for (int i = 0; i < H; i++) {
        for (int j = 0; j < W; j++) draw_block(j, i, board[i][j]);
    }

}

// ================================ MAIN ===========================
int main() {
    vga_push();

    char background = 0;
    set_all_pixels(background);

    unsigned int s = get_seed();
    snrand(s ? s : 1);

    long prev_input = 0;

    timeinit();

    while (state != ST_EXIT) {

        switch (state) {
            case ST_MENU: {
                draw_menu();
                while (true) {
                    reset_game_with_options();
                    state = ST_PLAYING;
                    set_all_pixels(background);
                    break;

                    int key = poll_key();
                    if (!key) break;
                    if (key == 'p' || key == 'P') {
                        reset_game_with_options();
                        state = ST_PLAYING;
                        set_all_pixels(background);
                        break;
                    } else if (key == 'o' || key == 'O') {
                        state = ST_OPTIONS;
                        set_all_pixels(background);
                        break;
                    } else if (key == 'q' || key == 'Q' || key == KEY_ESC) {
                        state = ST_EXIT;
                        set_all_pixels(background);
                        break;
                    }
                }
            } break;

            case ST_OPTIONS: {
                draw_options(opt_start_level, opt_soft_drop_enabled);
                while (true) {
                //    if (clockms - prev_input >= input_limit_ms) {
                        prev_input = clockms;
                        int key = poll_key();
                        if (key == KEY_ARROW_LEFT && opt_start_level > 0)  opt_start_level--;
                        if (key == KEY_ARROW_RIGHT && opt_start_level < 20) opt_start_level++;
                    //  if (key == 's' || key == 'S') opt_soft_drop_enabled ^= 1;
                        if (key == 'b' || key == 'B' || key == KEY_ESC) { state = ST_MENU; break; }
                //    }
                }
            } break;

            case ST_PLAYING: {
                if ((*status) & 0x1) { // if timeout = 1
                    *status = 0; // clear timeout
                    clockms++;

                    if (clockms % (fall_interval_ms / (1 + get_btn() * 10)) == 0) {
                        gravity_step();
                        if (!has_active_piece() && new_block) {
                            new_block = 0;
                            spawn_random_block();
                        }
                    }

                    if (clockms % refresh_rate_ms == 0) {
                        draw();
                        vga_push();
                    }
                }

            //    if (clockms - prev_input >= input_limit_ms) {
                    int key = poll_key();
                    if (key == KEY_LEFT_MOVE) {
                        move_piece_horiz(-1);
                        prev_input = clockms;
                    } else if (key == KEY_RIGHT_MOVE) {
                        move_piece_horiz(+1);
                        prev_input = clockms;
                    } else if (key == KEY_ROTATE) {
                        rotate_active_block(+1);
                        prev_input = clockms;
                    } else if (key == KEY_SPACE && opt_soft_drop_enabled && can_piece_fall()) {
                        move_piece_down();
                        prev_input = clockms;
                    } else if (key == KEY_ESC) {
                        state = ST_MENU;
                        break;
                    }
            //    }
            
            } break;

            default: state = ST_EXIT; break;
        }
    }
    return 0;
}
