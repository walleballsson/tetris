#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // for sleep()
// a simple sleep function for a delay
void tick(void) { sleep(1); }  // macOS (POSIX). Requires: #include <unistd.h>

//define x cordinates
int x = 0;
int fps = 30;

// define rows first
char one[8]   = {'0','0','0','0','0','0','0','0'};
char two[8]   = {'0','0','0','0','0','0','0','0'};
char three[8] = {'0','0','0','0','0','0','0','0'};
char four[8]  = {'1','1','0','0','0','0','0','0'};
char five[8]  = {'1','1','0','0','0','0','0','0'};
char six[8]   = {'1','1','0','0','0','0','0','0'};
char seven[8] = {'0','0','0','0','0','0','0','0'};
char eight[8] = {'1','1','1','1','1','1','1','1'};

// array of pointers to ALL rows
char *rows[] = { one, two, three, four, five, six, seven, eight };


// example access: rows[row][col]

//function to print the pixels
static void print_pixels(void) {
    for (int i = 0; i < 8; ++i) {
        printf("%c", one[i]);
    }
    printf("\n");
    for (int i = 0; i < 8; ++i) {
        printf("%c", two[i]);
    }
    printf("\n");
    for (int i = 0; i < 8; ++i) {
        printf("%c", three[i]);
    }
    printf("\n");
    for (int i = 0; i < 8; ++i) {
        printf("%c", four[i]);
    }
    printf("\n");
    for (int i = 0; i < 8; ++i) {
        printf("%c", five[i]);
    }
    printf("\n");
    for (int i = 0; i < 8; ++i) {
        printf("%c", six[i]);
    }
    printf("\n");
    for (int i = 0; i < 8; ++i) {
        printf("%c", seven[i]);
    }
    printf("\n");
    for (int i = 0; i < 8; ++i) {
        printf("%c", eight[i]);
    }
    printf("\n");

}

//define size of the block
void block(int x) {
    int block[4][2] = {
        {0, x}, {0, x + 1},
        {1, x}, {1, x + 1}
    };
}
//add block to pixels at position x


// Put a 2x2 block at columns x and x+1 on rows 0 and 1
void add_block_at_x(int x) {
    if (x < 0 || x + 1 >= 8) return;   // keep it on the 8-wide board
    rows[0][x]     = '1';
    rows[0][x + 1] = '1';
    rows[1][x]     = '1';
    rows[1][x + 1] = '1';
}

void clear_row(int j) {
    for (int i = 0; i < 8; i++) {
        rows[j][i] = '0';
    }
}

//function to determine top pixel of a block


//function to move pixels down
void move_down(int )
for(int row = 0; row < 7; row++) {
    for(int pos = 0; pos < 8; pos++){
        if(rows[7 - row][pos] == '0') {
             rows[7 - row][pos] = rows[7 - (row + 1)][pos];  // safe: 3..1 from 2..0
        }
    }
}
int main(void) {
    
    print_pixels();
    printf("\n");
    for (int fps; fps > 0; fps--) {
    for (int i = 0; i < 7; ++i) {                 // i = 0,1,2
        for (int j = 0; j < 8; ++j) {
            if (rows[7 - i][j] == '0') {
                rows[7 - i][j] = rows[7 - (i + 1)][j];  // safe: 3..1 from 2..0
                //clear the pixel above
                rows[7 - (i + 1)][j] = '0';
                
            }
        }
        
    }
    for(int j = 7; j > 0; j--) {
        int check = 0;
        for(int r  = 0; r < 8; ++r) {
            
            if(rows[j][r] == '1') {
                check++;
                if(check == 8) {
                    clear_row(j);
                }
            
            }
            
        }
        check = 0;
    }
     for (int i = 0; i < 8; i++) {
        int count = 0;
        for (int j = 0; j < 8; j++) {
            if (rows[j][i] == '1') {
               count++;
               if (count == 8) {
                   //end game
                     printf("Game Over\n");
                        return 0;
               }
            }
        }
        count = 0;
    }
    if (fps % 5 == 0) {
        add_block_at_x(x);
        x = x + 2;
        if (x > 7) {
            x = 0;
        }
    }
    print_pixels();
    printf("\n");
    fps++;
    tick();
}

}