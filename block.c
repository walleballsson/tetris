#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // for sleep()
// a simple sleep function for a delay
void tick(void) { sleep(1); }  // macOS (POSIX). Requires: #include <unistd.h>

//define x cordinates
int x = 0;
int fps = 30;
int new_block = 1;
// define rows first
char one[8]   = {'0','0','0','0','0','0','0','0'};
char two[8]   = {'0','0','0','0','0','0','0','0'};
char three[8] = {'0','0','0','0','0','0','0','0'};
char four[8]  = {'0','0','0','0','0','0','0','0'};
char five[8]  = {'0','0','0','0','0','0','0','0'};
char six[8]   = {'0','0','0','0','0','0','0','0'};
char seven[8] = {'0','0','0','0','0','0','0','0'};
char eight[8] = {'2','2','2','2','2','2','2','2'};

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

//create block 
void block(int x) {
    rows[0][x]     = '1';
    rows[0][x + 1] = '1';
    rows[1][x]     = '1';
    rows[1][x + 1] = '1';


}
// track cordinates of block
void cord()


//change block 1 to 2 when it hits the bottom or another block
void change() {
    int update = 0;
    for(int row = 0; row < 8; row++) {
        for(int pos = 0; pos < 8; pos++){
            if (rows[row][pos] == '1' && (row == 7 || rows[row + 1][pos] == '2')){ 
                    update = 1;
                break;
        }
    }
}
 if (update == 1) {
                 for (int i = 0; i < 8; i++) {
                     for ( int j = 0; j < 8; j++) {
                         if (rows[i][j] == '1') {
                             rows[i][j] = '2';
                         }
                     }
                 }
                    new_block = 1;
            }
} 




//clear pixel above when it moves down
//void clear_pixel(int p, int r) {
  //  for(int i = 1; i < r; i++) {
    //    if (rows[i][p] == '1' && rows[i - 1][p] == '0') {
      //      rows[i][p] = '0';
       // }
    //}
//}

//move down pixels
void move_down() {
for(int r = 7; r > 0; r--) {
    for(int p = 0; p < 8; p++){
        if (rows[r][p] == '0'){ 
            if(rows[r - 1][p] == '1') {
                 rows[r][p] = rows[r - 1][p];  // safe: 3..1 from 2..0
                // clear_pixel(p, r);
                rows[r - 1][p] = '0';
            }
        }
        if (rows[r][p] == 2){ 
            change();
            break;
            }
        }
    }
}

//function to clear a row of 2s
void clear_row(int row) {
    for (int i = 0; i < 8; i++) {
        rows[row][i] = '0';
    }
}

//check if a row is full
void full() {
    for(int row = 7; row > 0; row--) {
        int check = 0;
        for( int pos = 0; pos <8; pos++) {
            if(rows[row][pos] == '2') {
                check++;
                if(check == 8) {
                    clear_row(row);
                }
            }
        }
    }
}

int main (void) {

    for(fps = 0; fps < 30; fps++) {
    print_pixels();
    printf("\n");
    if(new_block == 1) {
        if (rows[0][x] == '2' || rows[0][x + 1] == '2' || rows[1][x] == '2' || rows[1][x + 1] == '2') {
        printf("Game Over\n");
        exit(0);
    }
        block(x);
        x = (x) % 7;
        new_block = 0;
    }
    full();
    move_down();
    change();
    tick();
}
}


