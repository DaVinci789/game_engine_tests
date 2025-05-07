float tick_nums[] = {
    1.0f,
    2.0f,
    4.0f,
    5.0f,
    10.0f,
    15.0f,
    30.0f,
    40.0f,
    50.0f,
    60.0f,
    75.0f,
    100.0f,
    120.0f,
    180.0f,
    240.0f,
    480.0f,
}; // basically lets you scale the fps for testing purposes
#include "raylib.h"
#include "raymath.h"

#include <stdint.h>
#include <string.h>

typedef enum {
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_DASH,
    INPUT_SHOOT,
    INPUT_ACTION_COUNT
} Input_Action;

typedef enum {
    INPUT_FLAG_DOWN     = 1 << 0,
    INPUT_FLAG_PRESSED  = 1 << 1,
    INPUT_FLAG_RELEASED = 1 << 2
} Input_Flag;

typedef struct {
    Vector2 cursor;
    uint8_t actions[INPUT_ACTION_COUNT]; // Each entry is a bit set of Input_Flag
} Input;

uint8_t input_flags_from_key(int key) {
    uint8_t flags = 0;
    if (IsKeyDown(key))     flags |= INPUT_FLAG_DOWN;
    if (IsKeyPressed(key))  flags |= INPUT_FLAG_PRESSED;
    if (IsKeyReleased(key)) flags |= INPUT_FLAG_RELEASED;
    return flags;
}

uint8_t input_flags_from_mouse_button(int button) {
    uint8_t flags = 0;
    if (IsMouseButtonDown(button))     flags |= INPUT_FLAG_DOWN;
    if (IsMouseButtonPressed(button))  flags |= INPUT_FLAG_PRESSED;
    if (IsMouseButtonReleased(button)) flags |= INPUT_FLAG_RELEASED;
    return flags;
}

void input_clear_temp(Input *input) {
    for (int i = 0; i < INPUT_ACTION_COUNT; ++i) {
        input->actions[i] &= INPUT_FLAG_DOWN; // Keep only DOWN flag
    }
}

typedef struct
{
    Vector2 player_pos;
    Vector2 player_dir;
    float player_anim_timer;
    _Bool player_moving;
} Game;

void game_tick(Game *game, Input input, float delta)
{
    Vector2 move_dir = {0};
    if (input.actions[INPUT_LEFT]  & INPUT_FLAG_DOWN) move_dir.x -= 1;
    if (input.actions[INPUT_RIGHT] & INPUT_FLAG_DOWN) move_dir.x += 1;
    if (input.actions[INPUT_UP]    & INPUT_FLAG_DOWN) move_dir.y -= 1;
    if (input.actions[INPUT_DOWN]  & INPUT_FLAG_DOWN) move_dir.y += 1;

    _Bool moving = move_dir.x != 0 || move_dir.y != 0;

    /* if (moving && (input.actions[INPUT_DASH] & INPUT_FLAG_PRESSED)) { */
    /*     game->player_pos = Vector2Add(game->player_pos, Vector2Scale(move_dir, 120)); */
    /* } */

    if (moving) {
        move_dir = Vector2Normalize(move_dir);
        game->player_dir = move_dir;
    }

    game->player_pos        = Vector2Add(game->player_pos, Vector2Scale(move_dir, delta * 150));
    game->player_anim_timer += delta;
    game->player_moving     = moving;
}

void game_draw(Game game)
{
    Color tint = WHITE;
    int PIXEL_SCALE = 5;

    // Player
    {
        /* tex := game.player_moving ? g_player_walk_texture : g_player_idle_texture */

        /* // HACK */
        /* dir_row := 0 */
        /* if game.player_dir.y >= 0 { */
        /*     if game.player_dir.y > 0.86 { */
        /*         dir_row = 0 */
        /*     } else { */
        /*         dir_row = game.player_dir.x > 0 ? 5 : 1 */
        /*     } */
        /* } else { */
        /*     if game.player_dir.y < -0.86 { */
        /*         dir_row = 3 */
        /*     } else { */
        /*         dir_row = game.player_dir.x > 0 ? 4 : 2 */
        /*     } */
        /* } */

        /* src_rect := rl.Rectangle{ */
        /*     f32(int(game.player_anim_timer * 10)) * PLAYER_SPRITE_SIZE_X, */
        /*     f32(dir_row) * PLAYER_SPRITE_SIZE_Y, */
        /*     PLAYER_SPRITE_SIZE_X, */
        /*     PLAYER_SPRITE_SIZE_Y, */
        /* } */

        /* dst_rect := rl.Rectangle{ */
        /*     game.player_pos.x, */
        /*     game.player_pos.y, */
        /*     PLAYER_SPRITE_SIZE_X * PIXEL_SCALE, */
        /*     PLAYER_SPRITE_SIZE_Y * PIXEL_SCALE, */
        /* } */

        /* DrawTexturePro(tex, src_rect, dst_rect, {dst_rect.width * 0.5, dst_rect.height * 0.5}, 0, tint) */
        DrawRectangle(game.player_pos.x, game.player_pos.y, 10, 20, RED);
    }
}

#if 0
int main(void)
{
    InitWindow(1000, 700, "Fixed Timestep Demo");
    
    Input tick_input = {0};
    Game game = {0};
    Game temp_game = {0};

    int delta_index = 6;
    float accumulator = 0;
    float frame_time = 1.0 / 60.0;
    double last_time = GetTime();

    while (!WindowShouldClose())
    {
        double current_time = GetTime();
        frame_time = current_time - last_time;
        last_time = current_time;
        if (frame_time > 0.25) frame_time = 0.25; // max 250ms lag

        PollInputEvents();              // Poll input events (SUPPORT_CUSTOM_FRAME_CONTROL)
        BeginDrawing();
        ClearBackground((Color) {40, 45, 50, 255});

        float target_delta = 1.0 / tick_nums[delta_index];
        accumulator += frame_time;

        Input frame_input = {0};
        frame_input.cursor = GetMousePosition();
        frame_input.actions[INPUT_LEFT ] = input_flags_from_key(KEY_A);
        frame_input.actions[INPUT_RIGHT] = input_flags_from_key(KEY_D);
        frame_input.actions[INPUT_UP   ] = input_flags_from_key(KEY_W);
        frame_input.actions[INPUT_DOWN ] = input_flags_from_key(KEY_S);
        frame_input.actions[INPUT_DASH ] = input_flags_from_key(KEY_SPACE);
        frame_input.actions[INPUT_SHOOT] = input_flags_from_key(KEY_M);

        tick_input.cursor = frame_input.cursor;
        for (int action = 0; action < INPUT_ACTION_COUNT; ++action) {
            tick_input.actions[action] |= frame_input.actions[action];
        }

        // Fixed Render Tick
        _Bool any_tick = accumulator > target_delta;

        for (;accumulator > target_delta; accumulator -= target_delta) {
            game_tick(&game, tick_input, target_delta);
            input_clear_temp(&tick_input);
        }
        TraceLog(LOG_INFO, "%f %f", accumulator, target_delta);

        memcpy(&temp_game, &game, sizeof(Game));
        game_tick(&temp_game, tick_input, accumulator);
        game_draw(temp_game);

        if (any_tick) memset(&tick_input, 0, sizeof(tick_input));
        EndDrawing();

        SwapScreenBuffer();         // Flip the back buffer to screen (front buffer)
    }
}
#endif

typedef union _LARGE_INTEGER {
  struct {
    unsigned int LowPart;
    long  HighPart;
  } DUMMYSTRUCTNAME;
  struct {
    unsigned int LowPart;
    long  HighPart;
  } u;
  long long QuadPart;
} LARGE_INTEGER;

#define W32(r) __declspec(dllimport) r __stdcall
W32(int) QueryPerformanceCounter(LARGE_INTEGER *i);
W32(int) QueryPerformanceFrequency(LARGE_INTEGER *i);

int64_t GetPerformanceCounter(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (int64_t)counter.QuadPart;
}
int64_t GetPerformanceFrequency(void) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (int64_t)freq.QuadPart;
}

#define TICK_HZ 15
#define MAX_FRAME_SKIP 8
#define TIME_HISTORY_COUNT 4

int main(void)
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 700, "Fixed Timestep Raylib");

    Input tick_input = {0};
    Game game = {0};
    Game temp_game = {0};

    double target_delta = 1.0 / TICK_HZ;
    int64_t perf_freq = (int64_t)GetTime() * 0 + 1000000000; // assume nanosecond accuracy

    // Simulate performance counter using GetTime
    int64_t snap_frequencies[8] = {0};
    for (int i = 0; i < 8; i++) {
        snap_frequencies[i] = (int64_t)(target_delta * (i + 1) * perf_freq);
    }

    int64_t vsync_maxerror = perf_freq * 0.0002; // ~0.2 ms tolerance
    int64_t time_averager[TIME_HISTORY_COUNT];
    for (int i = 0; i < TIME_HISTORY_COUNT; ++i) time_averager[i] = (int64_t)(target_delta * perf_freq);
    int64_t averager_residual = 0;

    int64_t frame_accumulator = 0;
    int64_t prev_frame_time = (int64_t)(GetTime() * perf_freq);
    int64_t desired_frametime = (int64_t)(target_delta * perf_freq);

    bool resync = true;

    while (!WindowShouldClose()) {
        // Fake performance counter using GetTime
        int64_t now = (int64_t)(GetTime() * perf_freq);
        int64_t delta_time = now - prev_frame_time;
        prev_frame_time = now;

        if (delta_time > desired_frametime * MAX_FRAME_SKIP) delta_time = desired_frametime;
        if (delta_time < 0) delta_time = 0;

        // Vsync snapping
        for (int i = 0; i < 8; i++) {
            if (llabs(delta_time - snap_frequencies[i]) < vsync_maxerror) {
                delta_time = snap_frequencies[i];
                break;
            }
        }

        // Delta averaging
        for (int i = 0; i < TIME_HISTORY_COUNT - 1; i++)
            time_averager[i] = time_averager[i + 1];
        time_averager[TIME_HISTORY_COUNT - 1] = delta_time;

        int64_t sum = 0;
        for (int i = 0; i < TIME_HISTORY_COUNT; i++)
            sum += time_averager[i];

        delta_time = sum / TIME_HISTORY_COUNT;
        averager_residual += sum % TIME_HISTORY_COUNT;
        delta_time += averager_residual / TIME_HISTORY_COUNT;
        averager_residual %= TIME_HISTORY_COUNT;

        frame_accumulator += delta_time;

        if (frame_accumulator > desired_frametime * MAX_FRAME_SKIP)
            resync = true;

        if (resync) {
            frame_accumulator = 0;
            delta_time = desired_frametime;
            resync = false;
        }

        PollInputEvents();
        BeginDrawing();
        ClearBackground((Color){40, 45, 50, 255});

        // Gather frame input
        Input frame_input = {0};
        frame_input.cursor = GetMousePosition();
        frame_input.actions[INPUT_LEFT ] = input_flags_from_key(KEY_A);
        frame_input.actions[INPUT_RIGHT] = input_flags_from_key(KEY_D);
        frame_input.actions[INPUT_UP   ] = input_flags_from_key(KEY_W);
        frame_input.actions[INPUT_DOWN ] = input_flags_from_key(KEY_S);
        frame_input.actions[INPUT_DASH ] = input_flags_from_key(KEY_SPACE);
        frame_input.actions[INPUT_SHOOT] = input_flags_from_key(KEY_M);

        // Merge frame input into tick_input
        tick_input.cursor = frame_input.cursor;
        for (int i = 0; i < INPUT_ACTION_COUNT; ++i)
            tick_input.actions[i] |= frame_input.actions[i];

        double seconds_per_tick = (double)desired_frametime / perf_freq;

        _Bool any_tick = frame_accumulator >= desired_frametime;
        while (frame_accumulator >= desired_frametime) {
            game_tick(&game, tick_input, seconds_per_tick);
            input_clear_temp(&tick_input);
            frame_accumulator -= desired_frametime;
        }

        memcpy(&temp_game, &game, sizeof(Game));
        game_tick(&temp_game, tick_input, seconds_per_tick * ((double)frame_accumulator / desired_frametime));
        game_draw(game);
        game_draw(temp_game);

        if (any_tick) memset(&tick_input, 0, sizeof(tick_input));

        EndDrawing();
        SwapScreenBuffer(); // for SUPPORT_CUSTOM_FRAME_CONTROL
    }

    CloseWindow();
    return 0;
}
