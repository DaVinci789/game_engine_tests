typedef struct
{
    char *data;
    ptrdiff_t len;
} Str;
#define S(s) (Str) {s, sizeof(s) - 1}

uint64_t hash64(Str s)
{
    uint64_t h = 0x100;
    for (ptrdiff_t i = 0; i < s.len; i++) {
        h ^= s.data[i] & 255;
        h *= 1111111111111111111;
    }
    return h;
}

_Bool equals(Str a, Str b)
{
    if (a.len != b.len) {
        return 0;
    }
    return !a.len || !memcmp(a.data, b.data, a.len);
}

typedef struct
{
    char *beg;
    char *end;
} Arena;
#define new(a, n, t)    (t *)alloc(a, n, sizeof(t), _Alignof(t))

void *alloc(Arena *a, ptrdiff_t count, ptrdiff_t size, ptrdiff_t align)
{
    ptrdiff_t pad = -(uintptr_t)a->beg & (align - 1);
    if (count > (a->end - a->beg - pad)/size)
    {
        assert(0);
        return 0;
    }
    void *r = a->beg + pad;
    a->beg += pad + count*size;
    return memset(r, 0, count*size);
}

typedef enum
{
    UI_WidgetFlag_Clickable,
    UI_WidgetFlag_DrawBorder,
    UI_WidgetFlag_DrawText,
    UI_WidgetFlag_DrawBackground,
    UI_WidgetFlag_AnimationHot,
    UI_WidgetFlag_AnimationActive,
} UI_WidgetFlag;


typedef struct UI_Widget UI_Widget;
struct UI_Widget
{
    UI_Widget *first;
    UI_Widget *last;
    UI_Widget *next;
    UI_Widget *prev;
    UI_Widget *parent;

    UI_WidgetFlag flags;
    Str string;

    // generation info
    int64_t key;

    // computed every frame
    Vector2 size;
    Vector2 position;
    Rectangle rect;

    // persistent data
    float hot;
    float active;
};

typedef struct
{
    UI_Widget *widget;
    Vector2 mouse;
    Vector2 drag_delta;
    _Bool clicked;
    _Bool double_clicked;
    _Bool right_clicked;
    _Bool pressed;
    _Bool released;
    _Bool dragging;
    _Bool hovering;
} UI_Comm;

typedef struct UI_Map UI_Map;
struct UI_Map
{
    UI_Map *child[4];
    Str key;
    UI_Widget *value;
};

typedef struct
{
    Arena arena;
    UI_Map *map;
    UI_Widget *start;

    Vector2 starting_origin;
    Font font;
} UI_State;

UI_State ui_state = {0};

UI_Widget **UI_Lookup(UI_Map **map, Str key, Arena *a)
{
    uint64_t h = hash64(key);
    while (*map)
    {
        if (equals(key, (*map)->key))
        {
            return &(*map)->value;
        }
        map = &(*map)->child[h >> 62];
        h <<= 2;
    }

    if (!a) return 0;

    *map = new(a, 1, UI_Map);
    (*map)->key = key;
    return &(*map)->value;
}

UI_Widget *UI_WidgetMake(UI_WidgetFlag flags, Str string)
{
    UI_Widget **slot = UI_Lookup(&ui_state.map, string, &ui_state.arena);
    if (!*slot)
    {
        *slot = new(&ui_state.arena, 1, UI_Widget);
    }

    UI_Widget *widget = *slot;
    widget->flags = flags;
    widget->string = string;

    widget->size = (Vector2) {100, 100};
    widget->position = ui_state.starting_origin;
    widget->rect.x = widget->position.x;
    widget->rect.y = widget->position.y;
    widget->rect.width = widget->size.x;
    widget->rect.height = widget->size.y;
    
    if (!ui_state.start)
    {
        ui_state.start = widget;
    }

    return widget;
}

UI_Comm UI_CommFromWidget(UI_Widget *widget)
{
    Vector2 mouse = GetMousePosition();

    UI_Comm c = {0};
    c.widget = widget;
    c.mouse = mouse;

    c.hovering = CheckCollisionPointRec(mouse, widget->rect);
    c.pressed = c.hovering && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    c.clicked = c.hovering && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

    return c;
}

_Bool UI_Button(Str text)
{
    UI_Widget *widget = UI_WidgetMake(UI_WidgetFlag_Clickable |
                                      UI_WidgetFlag_DrawBorder |
                                      UI_WidgetFlag_DrawBackground |
                                      UI_WidgetFlag_DrawText |
                                      UI_WidgetFlag_AnimationHot |
                                      text);
    UI_Comm comm = UI_CommFromWidget(widget);
    return comm.clicked;
}

void UI_RenderWidget(UI_Widget *widget)
{
    if (!widget) return;

    if (widget->flags & UI_WidgetFlag_DrawBackground)
    {
        DrawRectangleRec(widget->rect, GRAY);
    }

    if (widget->flags & UI_WidgetFlag_DrawBackground)
    {
        DrawRectangleRec(widget->rect, GRAY);
    }

    // Draw border if requested
    if (widget->flags & UI_WidgetFlag_DrawBorder)
    {
        DrawRectangleLinesEx(widget->rect, 2.0f, DARKGRAY);
    }

    // Draw text if requested
    if (widget->flags & UI_WidgetFlag_DrawText)
    {
        const char *text = widget->string.data;
        int fontSize = 20;
        Vector2 textSize = MeasureTextEx(ui_state.font, text, fontSize, 1.0f);
        Vector2 textPos = {
            widget->rect.x + (widget->rect.width - textSize.x) / 2,
            widget->rect.y + (widget->rect.height - textSize.y) / 2
        };
        DrawTextEx(ui_state.font, text, textPos, fontSize, 1.0f, BLACK);
    }
}

void UI_Render()
{
    for (UI_Widget *child = ui_state.start; child; child = child->next)
    {
        UI_RenderWidget(child);
    }
    ui_state.start = 0;
}

#ifdef ENGINE_UNIT
int main(void)
{
    InitWindow(480, 270, "Fixed Timestep Demo");
    while (!WindowShouldClose())
    {
        if (UI_Button(S("Click me!")))
        {
            TraceLog(LOG_INFO, "You clicked me!");
        }
        
        BeginDrawing();
        ClearBackground(RAYWHITE);
        UI_Render();
        EndDrawing();
    }
}
#endif
