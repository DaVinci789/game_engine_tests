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

#define assert(c) if (!(c)) __debugbreak()

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "raylib.h"
#include "raymath.h"
#include "ui.c"

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
        DrawRectangle(game.player_pos.x, game.player_pos.y, 40, 80, tint);
    }
}

#define TICK_HZ 15
#define MAX_FRAME_SKIP 8
#define TIME_HISTORY_COUNT 4

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    int game_width = 480;
    int game_height = 270;
    int window_width = game_width * 3;
    int window_height = game_height * 3;

    InitWindow(window_width, window_height, "Fixed Timestep Raylib");
    RenderTexture2D target = LoadRenderTexture(game_width, game_height);

    Input tick_input = {0};
    Game game = {0};
    Game temp_game = {0};

    void *ui_arena_start = malloc(1 << 20);
    ui_state.arena.beg = ui_arena_start; 
    ui_state.arena.end = ui_state.arena.beg + (1 << 20);

    ui_state.font = LoadFont("MonteCarloFixed12-Bold.ttf");

    int user_fps = 60;
    SetTargetFPS(user_fps);

    _Bool vsync = 0;

    float accumulator = 0;
    while (!WindowShouldClose())
    {
        float target_delta = 1.0 / 60.0;

        float frame_time = GetFrameTime();
        accumulator += frame_time;

        if (IsKeyPressed(KEY_UP))   {
            user_fps += 5;
            SetTargetFPS(user_fps);
        }
        if (IsKeyPressed(KEY_DOWN)) {
            user_fps = user_fps > 5 ? user_fps - 5 : 1;
            SetTargetFPS(user_fps);
        }

        if (IsKeyPressed(KEY_V)) {
            vsync = !vsync;
            if (vsync)
            {
                TraceLog(LOG_INFO, "Vsync disabled");
                SetConfigFlags(FLAG_WINDOW_RESIZABLE);
            }
            else
            {
                TraceLog(LOG_INFO, "Vsync enabled");
                SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
            }
        }

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

        _Bool any_tick = accumulator >= target_delta;
        int num_updates = 0;
        for (; accumulator >= target_delta; accumulator -= target_delta)
        {
            game_tick(&game, tick_input, target_delta);
            input_clear_temp(&tick_input);
            num_updates += 1;
        }
        if (num_updates > 1) TraceLog(LOG_INFO, "%d updates", num_updates);

        if (any_tick) memset(&tick_input, 0, sizeof(tick_input));

        memcpy(&temp_game, &game, sizeof(Game));
        game_tick(&temp_game, tick_input, accumulator);

        BeginTextureMode(target);
        ClearBackground((Color){40, 45, 50, 255});
        game_draw(game, WHITE);
        game_draw(temp_game, RED);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(RAYWHITE);
      
        // The target's height is flipped (in the source Rectangle), due to OpenGL reasons
        float virtual_ratio = (float)window_width /(float) game_width;
        Rectangle source_rec = { 0.0f, 0.0f, (float)target.texture.width, -(float)target.texture.height };
        Rectangle dest_rec = { -virtual_ratio, -virtual_ratio, window_width + (virtual_ratio*2), window_height + (virtual_ratio*2) }; 
        DrawTexturePro(target.texture, source_rec, dest_rec, (Vector2) {0}, 0.0f, WHITE);

        if (UI_Button(S("Click me!")))
        {
            TraceLog(LOG_INFO, "You clicked me!");
        }
        UI_Render();

        // DrawText(TextFormat("CURRENT FPS: %i", (int)((double) clocks_per_second / (double) delta_time)), GetScreenWidth() - 420, 40, 20, GREEN);
        DrawText(TextFormat("TARGET FPS: %i | ACTUAL: %i", user_fps, GetFPS()), GetScreenWidth() - 420, 40, 20, GREEN);

        // Interpolation percentage
        double interp_ratio = (double)accumulator / target_delta;
        int bar_width = 200;
        int bar_height = 10;
        int bar_x = GetScreenWidth() - 420;
        int bar_y = 70;

        DrawText(TextFormat("Interp: %.2f%%, F: %f D: %f", interp_ratio * 100.0, accumulator, target_delta), bar_x, bar_y - 20, 20, LIGHTGRAY);

        // Draw bar background
        DrawRectangle(bar_x, bar_y, bar_width, bar_height, DARKGRAY);

        // Draw filled portion of bar
        DrawRectangle(bar_x, bar_y, (int)(bar_width * interp_ratio), bar_height, ORANGE);
        EndDrawing();
    }
}
