#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>

#undef main

#define FRAME_RATE                      60
#define FRAME_DELAY_MILLISEC            (1000 / FRAME_RATE)
#define BLOCK_AUTO_MOVE_DOWN_MILLISEC   500

#define TETRIS_WIDTH         16
#define TETRIS_HEIGHT        28

/**
 * under normal circumstances, players want a part of the block to appear 
 * from the top of the screen and then gradually appear as a whole, that's
 * why a extra height is used here.
 * 
 * 4 is the longest block: I's length, so these 4 rows could contain all kinds of blocks.
*/
#define TETRIS_EXTRA_HEIGHT  4

#define TETRIS_ALL_HEIGHT    (TETRIS_HEIGHT + TETRIS_EXTRA_HEIGHT)
#define BLOCK_WIDTH          20

#define WINDOW_TITLE   "Tetris"
#define WINDOW_WIDTH   (TETRIS_WIDTH * BLOCK_WIDTH)
#define WINDOW_HEIGHT  (TETRIS_HEIGHT * BLOCK_WIDTH)

typedef enum Block {
    BLOCK_I,
    BLOCK_O,
    BLOCK_T,
    BLOCK_S,
    BLOCK_Z,
    BLOCK_J,
    BLOCK_L,
    BLOCK_EMPTY
} Block;

typedef struct Pos {
    int row, col;
} Pos;

typedef struct TetrisContext {
    SDL_Window* window;
    SDL_Renderer* renderer;
    Block data[TETRIS_ALL_HEIGHT][TETRIS_WIDTH];
    Block currentBlock;
    Pos currentBlockPos;
    int currentBlockRotateTimes;
    int gameOver;
} TetrisContext;

/**
 * (row, column)
 * 
 *      O           O           O             row axis 
 *   (0, -1)     (0, 0)      (0, 1)
 * 
 *                  O
 *               (1, 0)
 * 
 * 
 *             column  axis
*/
static const Pos blockShapeMap[][4][4] = {
    /* block I */
    {
        { { 0, 0 }, {  0, -1 }, {  0,  1, }, {  0,  2 } },   /* 0 degrees. */
        { { 0, 0 }, { -1,  0 }, {  1,  0, }, {  2,  0 } },   /* 90 degrees. */
        { { 0, 0 }, {  0,  1 }, {  0, -1, }, {  0, -2 } },   /* 180 degrees. */
        { { 0, 0 }, {  1,  0 }, { -1,  0, }, { -2,  0 } },   /* 270 degrees. */
    },
    /* block O */
    {
        { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } },
        { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } },
        { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } },
        { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } },
    },
    /* block T */
    {
        { { 0, 0 }, {  0, -1 }, {  0,  1 }, {  1,  0 } },
        { { 0, 0 }, { -1,  0 }, {  1,  0 }, {  0, -1 } },
        { { 0, 0 }, {  0,  1 }, {  0, -1 }, { -1,  0 } },
        { { 0, 0 }, {  1,  0 }, { -1,  0 }, {  0,  1 } },
    },
    /* block S */
    {
        { { -1, -1 }, {  0, -1 }, { 0, 0 }, {  1,  0 } }, 
        { { -1,  1 }, { -1,  0 }, { 0, 0 }, {  0, -1 } }, 
        { {  1,  1 }, {  0,  1 }, { 0, 0 }, { -1,  0 } },
        { {  1, -1 }, {  1,  0 }, { 0, 0 }, {  0,  1 } },
    },
    /* block Z */
    {
        { { -1,  0 }, { 0, 0 }, {  0, -1 }, {  1, -1 } },
        { {  0,  1 }, { 0, 0 }, { -1,  0 }, { -1, -1 } },
        { {  1,  0 }, { 0, 0 }, {  0,  1 }, { -1,  1 } },
        { {  0, -1 }, { 0, 0 }, {  1,  0 }, {  1,  1 } },
    },
    /* block J */
    {
        { { 0, 0 }, { -1,  0 }, { -2,  0 }, {  0, -1 } },
        { { 0, 0 }, {  0,  1 }, {  0,  2 }, { -1,  0 } },
        { { 0, 0 }, {  1,  0 }, {  2,  0 }, {  0,  1 } },
        { { 0, 0 }, {  0, -1 }, {  0, -2 }, {  1,  0 } }
    },
    /* block L */
    {
        { { 0, 0 }, { -1,  0 }, { -2,  0 }, {  0,  1 } }, 
        { { 0, 0 }, {  0,  1 }, {  0,  2 }, {  1,  0 } },
        { { 0, 0 }, {  1,  0 }, {  2,  0 }, {  0, -1 } },
        { { 0, 0 }, {  0, -1 }, {  0, -2 }, { -1,  0 } },
    }
};

static const SDL_Color COLOR_BLACK = { 0, 0, 0, 255 };

static const SDL_Color blockColorMap[] = {
    /* block I RGBA. */
    {  57, 197, 187, 255 },
    /* block O */
	{ 255, 165,   0, 255 },
	/* block T */
	{ 255, 255,   0, 255 },
	/* block S */
	{   0, 128,   0, 255 },
	/* block Z */
	{ 255,   0,   0, 255 },
	/* block J */
	{   0,   0, 255, 255 },
	/* block L */
	{ 128,   0, 128, 255 } 
};

static void gen_random_block(TetrisContext* context) {
    context->currentBlock = rand() % 7;                /* 7 kind of blocks: I, O, T, S, Z, J, L. */
    context->currentBlockRotateTimes = rand() % 4;     /* 4 rotations: 0, 1, 2, 3, present 0, 90, 180, 270 degrees. */
    context->currentBlockPos.col = TETRIS_WIDTH / 2;   /* new block should be centered. */
    
    /**
     * Default row can't be 0. because the blockShapeMap we defined above,
     * are based on the center point. Assuming we get a block I, and it is vertical,
     * then on the top of the center point, there should be at least 2 blocks space.
     * that's why the default row should be 2 here.
    */
    context->currentBlockPos.row = 2;
}

static void reset_tetris_map(TetrisContext* context) {
    srand(time(NULL));

    int r, c;
    for (r = 0; r < TETRIS_ALL_HEIGHT; ++r){
        for (c = 0; c < TETRIS_WIDTH; ++c){
            context->data[r][c] = BLOCK_EMPTY;
        }
    }

    gen_random_block(context);
    context->gameOver = 0;
}

static int init_tetris_context(TetrisContext* context) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0){
		SDL_Log("SDL_Init() failed: %s\n", SDL_GetError());
		return 0;
	}

	context->window = SDL_CreateWindow(WINDOW_TITLE, 
								SDL_WINDOWPOS_CENTERED, 
								SDL_WINDOWPOS_CENTERED, 
								WINDOW_WIDTH, 
								WINDOW_HEIGHT, 
								0);
							
	if (context->window == NULL){
		SDL_Log("create window failed: %s\n", SDL_GetError());
		return 0;
	}

	context->renderer = SDL_CreateRenderer(context->window, -1, SDL_RENDERER_SOFTWARE);
	if (context->renderer == NULL){
		SDL_Log("create renderer failed: %s\n", SDL_GetError());
		return 0;
	}

    reset_tetris_map(context);
	return 1;
}

static void free_tetris_context(TetrisContext* context) {
    if (context->renderer != NULL){
        SDL_DestroyRenderer(context->renderer);
    }

    if (context->window != NULL){
        SDL_DestroyWindow(context->window);
    }

    SDL_Quit();
}

#define macro_check_line_if(tetrisContextPtr, row, condition)   \
do {                                                            \
    Block block;                                                \
    int c;                                                      \
    for (c = 0; c < TETRIS_WIDTH; ++c){                         \
        block = tetrisContextPtr->data[(row)][c];               \
        if (!(condition)) {                                     \
            return 0;                                           \
        }                                                       \
    }                                                           \
                                                                \
    return 1;                                                   \
} while(0)

static int check_row_is_full(TetrisContext* context, int row) {
    macro_check_line_if(context, row, block != BLOCK_EMPTY);
}

static int check_row_is_empty(TetrisContext* context, int row) {
    macro_check_line_if(context, row, block == BLOCK_EMPTY);
}

#define macro_check_collision(context, outOfBoundCondition)                                      \
do {                                                                                             \
    const Pos* shape = blockShapeMap[context->currentBlock][context->currentBlockRotateTimes];   \
                                                                                                 \
    int i, r, c;                                                                                 \
    for (i = 0; i < 4; ++i){                                                                     \
        r = context->currentBlockPos.row + shape[i].row;                                         \
        c = context->currentBlockPos.col + shape[i].col;                                         \
                                                                                                 \
        if ((outOfBoundCondition) || context->data[r][c] != BLOCK_EMPTY) {                       \
            return 1;                                                                            \
        }                                                                                        \
    }                                                                                            \
                                                                                                 \
    return 0;                                                                                    \
} while(0)

static int check_left_collision(TetrisContext* context) {
    macro_check_collision(context, c < 0);
}

static int check_right_collision(TetrisContext* context) {
    macro_check_collision(context, c >= TETRIS_WIDTH);
}

static int check_down_collision(TetrisContext* context) {
    macro_check_collision(context, r >= TETRIS_ALL_HEIGHT);
}

static int check_left_right_down_collision(TetrisContext* context) {
    macro_check_collision(context, (c < 0) || (c >= TETRIS_WIDTH) || (r >= TETRIS_ALL_HEIGHT));
}

static void move_left(TetrisContext* context) {
    context->currentBlockPos.col -= 1;

    if (check_left_collision(context)){
        context->currentBlockPos.col += 1;
    }
}

static void move_right(TetrisContext* context) {
    context->currentBlockPos.col += 1;

    if (check_right_collision(context)){
        context->currentBlockPos.col -= 1;
    }
}

static void save_current_block(TetrisContext* context) {
    const Pos* shape = blockShapeMap[context->currentBlock][context->currentBlockRotateTimes];

    int i, r, c;
    for (i = 0; i < 4; ++i) {
        r = context->currentBlockPos.row + shape[i].row;
        c = context->currentBlockPos.col + shape[i].col;

        context->data[r][c] = context->currentBlock;
    }
}

/**
 * search the bottom empty line.
 * In most cases, players will be stuck in a situation where there are 
 * half of the blocks on the screen, so search from the middle line will be faster.
*/
static int find_the_bottom_empty_line(TetrisContext* context) {
    int bottomEmptyLine = TETRIS_ALL_HEIGHT / 2;
    int r;

    /* if middle line is empty, search down. */
    if (check_row_is_empty(context, bottomEmptyLine)){
        for (r = bottomEmptyLine + 1; r < TETRIS_ALL_HEIGHT; ++r){
            if (check_row_is_empty(context, r)){
                bottomEmptyLine = r;
            }
        }
    }
    else {   /* else, search up. */
        for (r = bottomEmptyLine - 1; r >= TETRIS_EXTRA_HEIGHT; --r){
            if (check_row_is_empty(context, r)){
                bottomEmptyLine = r;
            }
        }
    }

    return bottomEmptyLine;
}

static void eliminate_lines(TetrisContext* context) {
    int bottomEmptyLine = find_the_bottom_empty_line(context);
    int r, rr, c;
    
    for(r = TETRIS_ALL_HEIGHT - 1; r > bottomEmptyLine; ){
        if (check_row_is_full(context, r)){
            /* scroll down to eliminate lines. */
            for (rr = r - 1; rr >= bottomEmptyLine; --rr){
                for (c = 0; c < TETRIS_WIDTH; ++c) {
                    context->data[rr + 1][c] = context->data[rr][c];
                }
            }
        }
        else {
            /* if this line is not full, then search up. */
            --r;
        }
    }
}

static void move_down(TetrisContext* context) {
    context->currentBlockPos.row += 1;

    if (check_down_collision(context)){
        context->currentBlockPos.row -= 1;

        save_current_block(context);       
        eliminate_lines(context);

        /* if TETRIS_EXTRA_HEIGHT row has any blocks, then game over. */
        if (!check_row_is_empty(context, TETRIS_EXTRA_HEIGHT)){
            context->gameOver = 1;
        }

        gen_random_block(context);
    }
}

static void rotate(TetrisContext* context) {
    int* rotateTimes = &(context->currentBlockRotateTimes);
    *rotateTimes = (*rotateTimes + 1) % 4;

    if (check_left_right_down_collision(context)){
        *rotateTimes = (*rotateTimes + 3) % 4;
    }
}

/**
 * timer callback function.
 * it will let the current block move down in every BLOCK_AUTO_MOVE_DOWN_MILLISEC. 
 * 
 * But be careful here, cause SDL's timer is multi-threaded, so if we deal with the tetrisContext
 * here, maybe we will touch the race-condition, so a better solution is, push a SDL_USEREVENT, 
 * then in the main event loop, we can handle this event in a single thread.
*/
Uint32 move_down_timer_callback(Uint32 interval, void* param) {
    SDL_Event event;
    event.type = SDL_USEREVENT;
    SDL_PushEvent(&event);

    return interval;
}

void render_block(TetrisContext* context, int row, int col, const SDL_Color* color) {
    SDL_Rect rect = { col * BLOCK_WIDTH, (row - TETRIS_EXTRA_HEIGHT) * BLOCK_WIDTH, BLOCK_WIDTH, BLOCK_WIDTH };

    /* render a filled rectangle. */
    SDL_SetRenderDrawColor(context->renderer, color->r, color->g, color->b, color->a);
    SDL_RenderFillRect(context->renderer, &rect);

    /* render a outlined rectangle. */
    SDL_SetRenderDrawColor(context->renderer, COLOR_BLACK.r, COLOR_BLACK.g, COLOR_BLACK.b, COLOR_BLACK.a);
    SDL_RenderDrawRect(context->renderer, &rect);
}

void render(TetrisContext* context) {
    int i, r, c, rotateTimes;
    Block block;
    Block* currentBlock;
    const Pos* shape;
    SDL_Color* color;

    /* using black color to clear the screen first. */
    SDL_SetRenderDrawColor(context->renderer, COLOR_BLACK.r, COLOR_BLACK.g, COLOR_BLACK.b, COLOR_BLACK.a);
    SDL_RenderClear(context->renderer);

    /* render the old blocks. */
    for (r = TETRIS_EXTRA_HEIGHT; r < TETRIS_ALL_HEIGHT; ++r){
        for (c = 0; c < TETRIS_WIDTH; ++c){
            block = context->data[r][c];

            if (block != BLOCK_EMPTY){
                render_block(context, r, c, &(blockColorMap[block]));
            }
        }
    }

    /* render the current block. */
    block = context->currentBlock;
    rotateTimes = context->currentBlockRotateTimes;
    shape = blockShapeMap[block][rotateTimes];

    for (i = 0; i < 4; ++i){
        r = context->currentBlockPos.row + shape[i].row;
        c = context->currentBlockPos.col + shape[i].col;
        render_block(context, r, c, &(blockColorMap[block]));
    }

    SDL_RenderPresent(context->renderer);
}

int main() {
    TetrisContext context;
    Uint32 startTime, endTime, frameTime;
    int running = 1;
    SDL_Event event;
    SDL_TimerID moveDownTimer;

    if (!init_tetris_context(&context)){
        goto finally;
    }

    moveDownTimer = SDL_AddTimer(BLOCK_AUTO_MOVE_DOWN_MILLISEC, move_down_timer_callback, NULL);

    while (running) {
		startTime = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
			}
            else if (event.type == SDL_USEREVENT) {   /* associated with move_down_timer_callback() */
                move_down(&context);
            }
            else if (event.type == SDL_KEYDOWN) {
                switch(event.key.keysym.sym) {
                    case SDLK_UP:
                        rotate(&context);
                        break;
                    case SDLK_LEFT:
                        move_left(&context);
                        break;
                    case SDLK_RIGHT:
                        move_right(&context);
                        break;
                    case SDLK_DOWN:
                        move_down(&context);
                        break;
                    default:
                        break;
                }
        	}
        }

        render(&context);

        if (context.gameOver) {
            running = 0;
        }

		endTime = SDL_GetTicks();
        frameTime = endTime - startTime;

        if (frameTime < FRAME_DELAY_MILLISEC) {
            SDL_Delay(FRAME_DELAY_MILLISEC - frameTime);
        }
    }

finally:
    SDL_RemoveTimer(moveDownTimer);
    free_tetris_context(&context);
}
