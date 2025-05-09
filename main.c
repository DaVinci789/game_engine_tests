#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "raylib.lib")

#define countof(a) (sizeof(a) / sizeof(a[0]))

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

void game_draw(Game game, Color tint)
{
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
        DrawRectangle(game.player_pos.x, game.player_pos.y, 10, 20, tint);
    }
}

#include "os_interface.c"
#include "os_windows.c"

#define TICK_HZ 15
#define MAX_FRAME_SKIP 8
#define TIME_HISTORY_COUNT 4

int main(void)
{
    // SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 700, "Fixed Timestep Raylib");

    Input tick_input = {0};
    Game game = {0};
    Game temp_game = {0};

    int user_fps = 60;

    int64_t clocks_per_second = GetPerformanceFrequency();
    /* double fixed_deltatime = 1.0 / TICK_HZ; */
    /* int64_t desired_frametime = clocks_per_second / TICK_HZ; */
    double fixed_deltatime = 1.0 / user_fps;
    int64_t desired_frametime = clocks_per_second / user_fps;

    int64_t vsync_maxerror = desired_frametime * 0.0002; // ~0.2 ms tolerance

    int display_framerate = GetMonitorRefreshRate(GetCurrentMonitor());
    int64_t snap_hz = display_framerate;
    if (snap_hz <= 0) snap_hz = 60;

    int64_t snap_frequencies[8] = {0};
    for (int i = 0; i < 8; i++) {
        snap_frequencies[i] = (clocks_per_second / snap_hz) * (i + 1);
    }

    int64_t time_averager[TIME_HISTORY_COUNT];
    for (int i = 0; i < TIME_HISTORY_COUNT; ++i)
        time_averager[i] = desired_frametime;
    int64_t averager_residual = 0;

    _Bool running = 1;
    _Bool resync = 1;
    _Bool vsync = 0;
    _Bool locked_framerate = 0;

    int64_t prev_frame_time = GetPerformanceCounter();
    int64_t frame_accumulator = 0;

    while (!WindowShouldClose()) {
        // Recompute timing for new target FPS
        desired_frametime = clocks_per_second / user_fps;
        fixed_deltatime   = 1.0 / user_fps;

        int64_t current_frame_time = GetPerformanceCounter();
        int64_t delta_time = current_frame_time - prev_frame_time;
        prev_frame_time = current_frame_time;

        // handle unexpected timer anomalies (overflow, extra slow frames, etc)
        if (delta_time > desired_frametime * MAX_FRAME_SKIP)
        { // ignore extra-slow frames
            delta_time = desired_frametime;
        }
        if (delta_time < 0)
        {
            delta_time = 0;
        }

        // Vsync snapping
        if (vsync)
        {
            for (int i = 0; i < countof(snap_frequencies); i++) {
                if (llabs(delta_time - snap_frequencies[i]) < vsync_maxerror) {
                    delta_time = snap_frequencies[i];
                    break;
                }
            }
        }

        for (int i = 0; i < countof(time_averager) - 1; i++)
        {
            time_averager[i] = time_averager[i + 1];
        }
        time_averager[countof(time_averager) - 1] = delta_time;

        int64_t sum = 0;
        for (int i = 0; i < countof(time_averager); i++)
        {
            sum += time_averager[i];
        }

        delta_time         = sum /               TIME_HISTORY_COUNT;
        averager_residual += sum %               TIME_HISTORY_COUNT;
        delta_time        += averager_residual / TIME_HISTORY_COUNT;
        averager_residual %=                     TIME_HISTORY_COUNT;

        frame_accumulator += delta_time;
        if (frame_accumulator > desired_frametime * MAX_FRAME_SKIP)
        {
            resync = 1;
        }

        if (resync)
        {
            frame_accumulator = 0;
            // frame_accumulator = desired_frametime;
            delta_time = desired_frametime;
            resync = 0;
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

        double seconds_per_tick = (double)desired_frametime / clocks_per_second;

        _Bool any_tick = frame_accumulator >= desired_frametime;
        while (frame_accumulator >= desired_frametime) {
            // game_tick(&game, tick_input, seconds_per_tick);
            game_tick(&game, tick_input, fixed_deltatime);
            input_clear_temp(&tick_input);
            frame_accumulator -= desired_frametime;
        }

        memcpy(&temp_game, &game, sizeof(Game));
        // game_tick(&temp_game, tick_input, ((double) consumed_delta_time / clocks_per_second));
        game_tick(&temp_game, tick_input, seconds_per_tick * ((double)frame_accumulator / desired_frametime));

        game_draw(game, WHITE);
        game_draw(temp_game, RED);

        // DrawText(TextFormat("CURRENT FPS: %i", (int)((double) clocks_per_second / (double) delta_time)), GetScreenWidth() - 420, 40, 20, GREEN);
        DrawText(TextFormat("TARGET FPS: %i | ACTUAL: %i", user_fps, (int)((double) clocks_per_second / (double) delta_time)), GetScreenWidth() - 420, 40, 20, GREEN);

        // Interpolation percentage
        double interp_ratio = (double)frame_accumulator / desired_frametime;
        int bar_width = 200;
        int bar_height = 10;
        int bar_x = GetScreenWidth() - 420;
        int bar_y = 70;

        DrawText(TextFormat("Interp: %.2f%%, F: %d D: %d", interp_ratio * 100.0, frame_accumulator, desired_frametime), bar_x, bar_y - 20, 20, LIGHTGRAY);

        // Draw bar background
        DrawRectangle(bar_x, bar_y, bar_width, bar_height, DARKGRAY);

        // Draw filled portion of bar
        DrawRectangle(bar_x, bar_y, (int)(bar_width * interp_ratio), bar_height, ORANGE);

        if (any_tick) memset(&tick_input, 0, sizeof(tick_input));

        EndDrawing();

        if (locked_framerate)
        {
            int64_t time_taken = GetPerformanceCounter() - current_frame_time;
            int64_t target_frame_time = clocks_per_second / user_fps;

            if (time_taken < target_frame_time)
            {
                int64_t sleep_ns = (target_frame_time - time_taken) * 1000000000LL / clocks_per_second;
                NanoSleep(sleep_ns);
            }
        }
        else if (vsync)
        {
            int64_t time_taken = GetPerformanceCounter() - current_frame_time;
            int64_t target_frame_time = clocks_per_second / GetMonitorRefreshRate(GetCurrentMonitor());

            if (time_taken < target_frame_time)
            {
                int64_t sleep_ns = (target_frame_time - time_taken) * 1000000000LL / clocks_per_second;
                NanoSleep(sleep_ns);
            }
        }

        // Allow runtime framerate adjustments
        if (IsKeyPressed(KEY_UP))   user_fps += 5;
        if (IsKeyPressed(KEY_DOWN)) user_fps = user_fps > 5 ? user_fps - 5 : 1;
        if (IsKeyPressed(KEY_V)) vsync = !vsync;
        if (IsKeyPressed(KEY_L)) locked_framerate = !locked_framerate;

        SwapScreenBuffer(); // for SUPPORT_CUSTOM_FRAME_CONTROL
    }

    CloseWindow();
    return 0;
}
