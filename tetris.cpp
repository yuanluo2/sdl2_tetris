#include <SDL2/SDL.h>
#include <iostream>
#include <exception>
#include <string>
#include <functional>
#include <algorithm>
#include <random>
#include <memory>

#undef main

using namespace std::string_literals;

constexpr int FRAME_RATE                    = 60;
constexpr int FRAME_DELAY_MILLISEC          = 1000 / FRAME_RATE;
constexpr int BLOCK_AUTO_MOVE_DOWN_MILLISEC = 500;

constexpr int TETRIS_WIDTH = 16;
constexpr int TETRIS_HEIGHT = 28;

/**
 * under normal circumstances, players want a part of the block to appear 
 * from the top of the screen and then gradually appear as a whole, that's
 * why a extra height is used here.
 * 
 * 4 is the longest block: I's length, so these 4 rows could contain all kinds of blocks.
*/
constexpr int TETRIS_EXTRA_HEIGHT = 4;

constexpr int TETRIS_ALL_HEIGHT = TETRIS_HEIGHT + TETRIS_EXTRA_HEIGHT;
constexpr int BLOCK_WIDTH = 20;

const std::string WINDOW_TITLE = "Tetris";
constexpr int WINDOW_WIDTH = TETRIS_WIDTH * BLOCK_WIDTH;
constexpr int WINDOW_HEIGHT = TETRIS_HEIGHT * BLOCK_WIDTH;

enum Block {
    I, O, T, S, Z, J, L, Empty
};

struct Pos {
    int row, col;
};

constexpr SDL_Color COLOR_BLACK = { 0, 0, 0, 255 };

constexpr SDL_Color blockColorMap[] = {
  // block I RGBA.
  {  57, 197, 187, 255 },
  // block O.
	{ 255, 165,   0, 255 },
	// block T.
	{ 255, 255,   0, 255 },
	// block S.
	{   0, 128,   0, 255 },
	// block Z.
	{ 255,   0,   0, 255 },
	// block J.
	{   0,   0, 255, 255 },
	// block L.
	{ 128,   0, 128, 255 } 
};

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
constexpr Pos blockShapeMap[][4][4] = {
    // block I.
    {
        { { 0, 0 }, {  0, -1 }, {  0,  1, }, {  0,  2 } },   // 0 degrees.
        { { 0, 0 }, { -1,  0 }, {  1,  0, }, {  2,  0 } },   // 90 degrees.
        { { 0, 0 }, {  0,  1 }, {  0, -1, }, {  0, -2 } },   // 180 degrees.
        { { 0, 0 }, {  1,  0 }, { -1,  0, }, { -2,  0 } },   // 270 degrees.
    },
    // block O.
    {
        { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } },
        { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } },
        { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } },
        { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } },
    },
    // block T.
    {
        { { 0, 0 }, {  0, -1 }, {  0,  1 }, {  1,  0 } },
        { { 0, 0 }, { -1,  0 }, {  1,  0 }, {  0, -1 } },
        { { 0, 0 }, {  0,  1 }, {  0, -1 }, { -1,  0 } },
        { { 0, 0 }, {  1,  0 }, { -1,  0 }, {  0,  1 } },
    },
    // block S.
    {
        { { -1, -1 }, {  0, -1 }, { 0, 0 }, {  1,  0 } }, 
        { { -1,  1 }, { -1,  0 }, { 0, 0 }, {  0, -1 } }, 
        { {  1,  1 }, {  0,  1 }, { 0, 0 }, { -1,  0 } },
        { {  1, -1 }, {  1,  0 }, { 0, 0 }, {  0,  1 } },
    },
    // block Z.
    {
        { { -1,  0 }, { 0, 0 }, {  0, -1 }, {  1, -1 } },
        { {  0,  1 }, { 0, 0 }, { -1,  0 }, { -1, -1 } },
        { {  1,  0 }, { 0, 0 }, {  0,  1 }, { -1,  1 } },
        { {  0, -1 }, { 0, 0 }, {  1,  0 }, {  1,  1 } },
    },
    // block J.
    {
        { { 0, 0 }, { -1,  0 }, { -2,  0 }, {  0, -1 } },
        { { 0, 0 }, {  0,  1 }, {  0,  2 }, { -1,  0 } },
        { { 0, 0 }, {  1,  0 }, {  2,  0 }, {  0,  1 } },
        { { 0, 0 }, {  0, -1 }, {  0, -2 }, {  1,  0 } }
    },
    // block L.
    {
        { { 0, 0 }, { -1,  0 }, { -2,  0 }, {  0,  1 } }, 
        { { 0, 0 }, {  0,  1 }, {  0,  2 }, {  1,  0 } },
        { { 0, 0 }, {  1,  0 }, {  2,  0 }, {  0, -1 } },
        { { 0, 0 }, {  0, -1 }, {  0, -2 }, { -1,  0 } },
    }
};

class TetrisMap {
    Block blocks[TETRIS_ALL_HEIGHT][TETRIS_WIDTH];

    void copy_row_to_row(int fromRow, int toRow) noexcept {
        for (int c = 0; c < TETRIS_WIDTH; ++c){
            blocks[toRow][c] = blocks[fromRow][c];
        }
    }

    /**
    * search the bottom empty line.
    * In most cases, players will be stuck in a situation where there are 
    * half of the blocks on the screen, so search from the middle line will be faster.
    */
    int find_the_bottom_empty_line() {
        int bottomEmptyLine = TETRIS_ALL_HEIGHT / 2;

        // if middle line is empty, search down.
        if (check_row_is_empty(bottomEmptyLine)){
            for (int r = bottomEmptyLine + 1; r < TETRIS_ALL_HEIGHT; ++r){
                if (check_row_is_empty(r)){
                    bottomEmptyLine = r;
                }
            }
        }
        else {   // else, search up.
            for (int r = bottomEmptyLine - 1; r >= TETRIS_EXTRA_HEIGHT; --r){
                if (check_row_is_empty(r)){
                    bottomEmptyLine = r;
                }
            }
        }

        return bottomEmptyLine;
    }
public:
    TetrisMap() {
        for (int r = 0; r < TETRIS_ALL_HEIGHT; ++r){
            for (int c = 0; c < TETRIS_WIDTH; ++c){
                blocks[r][c] = Block::Empty;
            }
        }
    }

    Block get(int row, int col) const noexcept {
        return blocks[row][col];
    }

    void set(int row, int col, Block block) noexcept {
        blocks[row][col] = block;
    }

    void render_block(SDL_Renderer* renderer, int row, int col, Block block) noexcept {
        SDL_Color const& color = blockColorMap[static_cast<int>(block)];
        SDL_Rect rect = { 
            col * BLOCK_WIDTH, 
            (row - TETRIS_EXTRA_HEIGHT) * BLOCK_WIDTH, 
            BLOCK_WIDTH, 
            BLOCK_WIDTH
        };

        // render a filled rectangle.
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &rect);

        // render a outlined rectangle.
        SDL_SetRenderDrawColor(renderer, COLOR_BLACK.r, COLOR_BLACK.g, COLOR_BLACK.b, COLOR_BLACK.a);
        SDL_RenderDrawRect(renderer, &rect);
    }

    bool check_row_is_full(int rowIndex) const noexcept {
        const auto& row = blocks[rowIndex];
		    return std::none_of(std::cbegin(row), std::cend(row), [](Block block) { return block == Block::Empty; });
    }

    bool check_row_is_empty(int rowIndex) const noexcept {
        const auto& row = blocks[rowIndex];
		    return std::all_of(std::cbegin(row), std::cend(row), [](Block block) { return block == Block::Empty; });
    }

    void render(SDL_Renderer* renderer) noexcept {
        for (int r = TETRIS_EXTRA_HEIGHT; r < TETRIS_ALL_HEIGHT; ++r){
            for (int c = 0; c < TETRIS_WIDTH; ++c){
                Block block = get(r, c);

                if (block != Block::Empty){
                    render_block(renderer, r, c, block);
                }
            }
        }
    }

    void eliminate_lines() noexcept {
        int bottomEmptyLine = find_the_bottom_empty_line();
        
        for(int r = TETRIS_ALL_HEIGHT - 1; r > bottomEmptyLine; ){
            if (check_row_is_full(r)){
                // scroll down to eliminate lines.
                for (int rr = r - 1; rr >= bottomEmptyLine; --rr){
                    copy_row_to_row(rr, rr + 1);
                }
            }
            else {
                // if this line is not full, then search up.
                --r;
            }
        }
    }
};

using BlockInfoAction = std::function<void(int row, int col)>;
using BlockInfoCond = std::function<bool(int row, int col)>;

class BlockInfo {
    Block block;
    Pos pos;
    int rotateTimes;
public:
    BlockInfo()
        : block{ Block::Empty }, pos{ 0, 0 }, rotateTimes{ 0 }
    {}

    BlockInfo(Block _block, int row, int col, int _rotateTimes)
        : block{ _block }, pos{ row, col }, rotateTimes{ _rotateTimes }
    {}

    Block get_block() const noexcept {
        return block;
    }

    const Pos* get_shape() const noexcept {
        return blockShapeMap[static_cast<int>(block)][rotateTimes];
    }

    void for_each_shape_point(BlockInfoAction action) {
        const Pos* shape = get_shape();

        for (int i = 0; i < 4; ++i) {
            int row = pos.row + shape[i].row;
            int col = pos.col + shape[i].col;

            action(row, col);
        }
    }

    bool for_each_shape_point_if(BlockInfoCond cond) const {
        const Pos* shape = get_shape();

        for (int i = 0; i < 4; ++i) {
            int row = pos.row + shape[i].row;
            int col = pos.col + shape[i].col;

            if (cond(row, col)) {
                return true;
            }
        }

        return false;
    }

    void rotate() noexcept {
        rotateTimes = (rotateTimes + 1) % 4;
    }

    void un_rotate() noexcept {
        rotateTimes = (rotateTimes + 3) % 4;
    }

    void go_left() noexcept {
        pos.col -= 1;
    }

    void go_right() noexcept {
        pos.col += 1;
    }

    void go_down() noexcept {
        pos.row += 1;
    }

    void go_top() noexcept {
        pos.row -= 1;
    }
};

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

class Tetris {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TetrisMap tetrisMap;
    BlockInfo blockInfo;
    bool gameOver = false;

    // random generator.
    std::mt19937 mt{ std::random_device{}() };
	  std::uniform_int_distribution<unsigned int> randomBlock{ 0, 6 };
	  std::uniform_int_distribution<unsigned int> randomRotation{ 0, 3 };

    void init_graphics(){
        if (SDL_Init(SDL_INIT_VIDEO) < 0){
            throw std::runtime_error{ "SDL_Init() failed: "s + SDL_GetError() };
        }

        window = SDL_CreateWindow(WINDOW_TITLE.c_str(), 
                                    SDL_WINDOWPOS_CENTERED, 
                                    SDL_WINDOWPOS_CENTERED, 
                                    WINDOW_WIDTH, 
                                    WINDOW_HEIGHT, 
                                    0);
                                
        if (window == nullptr){
            throw std::runtime_error{ "create window failed: "s + SDL_GetError() };
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (renderer == nullptr){
            throw std::runtime_error{ "create renderer failed: "s + SDL_GetError() };
        }
    }

    void random_gen_current_block() {
        // 7 kind of blocks: I, O, T, S, Z, J, L.
        Block block = static_cast<Block>(randomBlock(mt)); 
        
        // 4 rotations: 0, 1, 2, 3, present 0, 90, 180, 270 degrees.
        int rotateTimes = static_cast<int>(randomRotation(mt));  

        // new block should be centered.
        int col = TETRIS_WIDTH / 2;   
    
        /**
        * Default row can't be 0. because the blockShapeMap we defined above,
        * are based on the center point. Assuming we get a block I, and it is vertical,
        * then on the top of the center point, there should be at least 2 blocks space.
        * that's why the default row should be 2 here.
        */
        int row = 2;

        blockInfo = BlockInfo{ block, row, col, rotateTimes };
    }

    void save_current_block() noexcept {
        blockInfo.for_each_shape_point([this](int row, int col) {
            tetrisMap.set(row, col, blockInfo.get_block());
        });
    }

    bool check_left_collision() const noexcept {
        return blockInfo.for_each_shape_point_if([this](int row, int col) {
            return col < 0 || tetrisMap.get(row, col) != Block::Empty;
        });
    }

    bool check_right_collision() const noexcept {
        return blockInfo.for_each_shape_point_if([this](int row, int col) {
            return col >= TETRIS_WIDTH || tetrisMap.get(row, col) != Block::Empty;
        });
    }

    bool check_down_collision() const noexcept {
        return blockInfo.for_each_shape_point_if([this](int row, int col) {
            return row >= TETRIS_ALL_HEIGHT || tetrisMap.get(row, col) != Block::Empty;
        });
    }

    bool check_left_right_down_collision() const noexcept {
        return blockInfo.for_each_shape_point_if([this](int row, int col) {
            return col < 0 || col >= TETRIS_WIDTH || row >= TETRIS_ALL_HEIGHT || tetrisMap.get(row, col) != Block::Empty;
        });
    }

    void move_left() noexcept {
        blockInfo.go_left();

        if (check_left_collision()){
            blockInfo.go_right();
        }
    }

    void move_right() noexcept {
        blockInfo.go_right();

        if (check_left_collision()){
            blockInfo.go_left();
        }
    }

    void move_down() noexcept {
        blockInfo.go_down();

        if (check_down_collision()){
            blockInfo.go_top();

            save_current_block();
            tetrisMap.eliminate_lines();

            // if TETRIS_EXTRA_HEIGHT row has any blocks, then game over.
            if (!tetrisMap.check_row_is_empty(TETRIS_EXTRA_HEIGHT)){
                gameOver = true;
            }

            random_gen_current_block();
        }
    }

    void rotate() noexcept {
        blockInfo.rotate();

        if (check_left_right_down_collision()){
            blockInfo.un_rotate();
        }
    }

    void render(){
        // using black color to clear the screen first.
        SDL_SetRenderDrawColor(renderer, COLOR_BLACK.r, COLOR_BLACK.g, COLOR_BLACK.b, COLOR_BLACK.a);
        SDL_RenderClear(renderer);

        tetrisMap.render(renderer);

        blockInfo.for_each_shape_point([this] (int row, int col) {
            tetrisMap.render_block(renderer, row, col, blockInfo.get_block());
        });

        SDL_RenderPresent(renderer);
    }
public:
    Tetris() = default;

    ~Tetris() noexcept {
        if (renderer != nullptr){
            SDL_DestroyRenderer(renderer);
        }

        if (window != nullptr){
            SDL_DestroyWindow(window);
        }

        SDL_Quit();
    }

    void start() {
        random_gen_current_block();
        init_graphics();

        Uint32 startTime, endTime, frameTime;
        bool running = true;
        SDL_Event event;
        SDL_TimerID moveDownTimer = SDL_AddTimer(BLOCK_AUTO_MOVE_DOWN_MILLISEC, move_down_timer_callback, nullptr);

        while (running) {
		    startTime = SDL_GetTicks();

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
			          }
                else if (event.type == SDL_USEREVENT) {   // associated with move_down_timer_callback().
                    move_down();
                }
                else if (event.type == SDL_KEYDOWN) {
                    switch(event.key.keysym.sym) {
                        case SDLK_UP:
                            rotate();
                            break;
                        case SDLK_LEFT:
                            move_left();
                            break;
                        case SDLK_RIGHT:
                            move_right();
                            break;
                        case SDLK_DOWN:
                            move_down();
                            break;
                        default:
                            break;
                    }
        	      }
            }

            render();

            if (gameOver) {
                running = false;
            }

            endTime = SDL_GetTicks();
            frameTime = endTime - startTime;

            if (frameTime < FRAME_DELAY_MILLISEC) {
                SDL_Delay(FRAME_DELAY_MILLISEC - frameTime);
            }
        }
    }
};

int main(){
    try {
        auto tetris = std::make_unique<Tetris>();
        tetris->start();
    }
    catch(std::exception const& e){
        std::cerr << e.what() << "\n";
    }
}
