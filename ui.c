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
    RectCut_Left,
    RectCut_Right,
    RectCut_Top,
    RectCut_Bottom,
} RectCutSide;

typedef struct
{
    Rectangle result;
    Rectangle remainder;
    RectCutSide side;
    _Bool ok;
} RectCut;

#define min(a, b) ((a) < (b) ? (a) : (b))

RectCut cut_left(Rectangle rect, float a)
{
    RectCut result = {0};
    result.ok = rect.width > 0 && a > 0;
    result.side = RectCut_Left;

    float cut = min(a, rect.width);
    result.result = rect;
    result.result.width = cut;

    result.remainder = rect;
    result.remainder.x += cut;
    result.remainder.width -= cut;

    return result;
}

RectCut cut_right(Rectangle rect, float a)
{
    RectCut result = {0};
    result.ok = rect.width > 0 && a > 0;
    result.side = RectCut_Right;

    float cut = min(a, rect.width);
    result.result = rect;
    result.result.x = rect.x + rect.width - cut;
    result.result.width = cut;

    result.remainder = rect;
    result.remainder.width -= cut;

    return result;
}

RectCut cut_top(Rectangle rect, float a)
{
    RectCut result = {0};
    result.ok = rect.height > 0 && a > 0;
    result.side = RectCut_Top;

    float cut = min(a, rect.height);
    result.result = rect;
    result.result.height = cut;

    result.remainder = rect;
    result.remainder.y += cut;
    result.remainder.height -= cut;

    return result;
}

RectCut cut_bottom(Rectangle rect, float a)
{
    RectCut result = {0};
    result.ok = rect.height > 0 && a > 0;
    result.side = RectCut_Bottom;

    float cut = min(a, rect.height);
    result.result = rect;
    result.result.y = rect.y + rect.height - cut;
    result.result.height = cut;

    result.remainder = rect;
    result.remainder.height -= cut;

    return result;
}

RectCut cut_side(RectSide rect, float a)
{
    RectSide result = {0};
    switch (rect.side)
    {
        case RectCut_Left:   result = cut_left(rect.result,   a);
        case RectCut_Right:  result = cut_right(rect.result,  a);
        case RectCut_Top:    result = cut_top(rect.result,    a);
        case RectCut_Bottom: result = cut_bottom(rect.result, a);
    }
    return result;
}

typedef enum
{
    UI_BoxFlag_Clickable,
    UI_BoxFlag_DrawBorder,
    UI_BoxFlag_DrawText,
    UI_BoxFlag_DrawBackground,
    UI_BoxFlag_AnimationHot,
    UI_BoxFlag_AnimationActive,
} UI_BoxFlag;


typedef struct UI_Box UI_Box;
struct UI_Box
{
    UI_Box *first;
    UI_Box *last;
    UI_Box *next;
    UI_Box *prev;
    UI_Box *parent;

    int child_count;

    // builder parameters
    UI_BoxFlag flags;
    Str string;

    // computed every frame ...usually
    Vector2 size;
    Vector2 position;
    Rectangle rect;

    // generation info
    int64_t key;

    // persistent data
    float hot;
    float active;
};

typedef struct
{
    UI_Box *widget;
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
    UI_Box *value;
};

typedef struct UI_ParentNode UI_ParentNode;
struct UI_ParentNode {
    UI_ParentNode *next;
    UI_Box *value;
};

typedef struct
{
    Arena arena;
    UI_Map *map;

    UI_Box *root;
    struct {
        UI_ParentNode *top;
        UI_ParentNode *free;
    } parent_stack;
    UI_ParentNode parent_nil_stack_top;

    Vector2 starting_origin;
    Font font;
} UI_State;

UI_State ui_state = {0};

UI_Box ui_g_nil_box =
{
 &ui_g_nil_box,
 &ui_g_nil_box,
 &ui_g_nil_box,
 &ui_g_nil_box,
 &ui_g_nil_box,
 &ui_g_nil_box,
 &ui_g_nil_box,
};

_Bool UI_BoxIsNil(UI_Box *b) {
    return b == &ui_g_nil_box;
}

#define UI_BoxSetNil(b) ((b) = &ui_g_nil_box)

UI_Box **UI_Lookup(UI_Map **map, Str key, Arena *a)
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

    *map = new(a, 1, UI_Map);
    (*map)->key = key;
    (*map)->value = &ui_g_nil_box;
    return &(*map)->value;
}

UI_Box *UI_BoxMake(UI_BoxFlag flags, Str string)
{
    UI_Box **slot = UI_Lookup(&ui_state.map, string, &ui_state.arena);
    if (UI_BoxIsNil(*slot))
    {
        *slot = new(&ui_state.arena, 1, UI_Box);
    }

    UI_Box *box = *slot;
    box->flags = flags;

    if (flags & UI_BoxFlag_DrawText)
    {
        box->string = string;
    }

    box->size = (Vector2) {100, 100};
    box->position = ui_state.starting_origin;
    box->rect.x = box->position.x;
    box->rect.y = box->position.y;
    box->rect.width = box->size.x;
    box->rect.height = box->size.y;
    
    UI_Box *parent = ui_state.parent_stack.top->value;
    if (UI_BoxIsNil(parent)) // Parent stuff
    {
        ui_state.root = box;
    }
    else
    {
        // push to end of parent linked list
        if (UI_BoxIsNil(parent->first)) {
            // List is empty
            parent->first = box;
            parent->last = box;
            UI_BoxSetNil(box->next);
            UI_BoxSetNil(box->prev);
        } else if (UI_BoxIsNil(parent->last)) {
            // Should not happen if logic is correct - fallback: treat as push front
            UI_BoxSetNil(box->prev);
            box->next = parent->first;
            if (!UI_BoxIsNil(parent->first)) {
                parent->first->prev = box;
            }
            parent->first = box;
        } else {
            // Regular case: insert after parent->last
            if (!UI_BoxIsNil(parent->last->next)) {
                parent->last->next->prev = box;
            }
            box->next = parent->last->next;
            box->prev = parent->last;
            parent->last->next = box;
            parent->last = box;
        }

        parent->child_count += 1;
        box->parent = parent;
    }

    return box;
}

UI_Comm UI_CommFromBox(UI_Box *box)
{
    Vector2 mouse = GetMousePosition();

    UI_Comm c = {0};
    c.box = box;
    c.mouse = mouse;

    // Handle these function within BeginBuild/EndBuild
    c.hovering = CheckCollisionPointRec(mouse, box->rect);
    c.pressed = c.hovering && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    c.clicked = c.hovering && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

    return c;
}

_Bool UI_Button(Str text)
{
    UI_Box *box = UI_BoxMake(UI_BoxFlag_Clickable |
                                      UI_BoxFlag_DrawBorder |
                                      UI_BoxFlag_DrawBackground |
                                      UI_BoxFlag_DrawText |
                                      UI_BoxFlag_AnimationHot |
                                      text);
    UI_Comm comm = UI_CommFromBox(box);
    return comm.clicked;
}

UI_Box *UI_PushParent(UI_Box *box)
{
    UI_ParentNode *node = ui_state->parent_stack.free;
    if (node)
    {
        StackPop(ui_state->parent_stack.free);
    }
    else
    {
        node = new(&ui_state.arena, UI_ParentNode, 1);
    }

    UI_Box *old_value = ui_state.parent_stack.top->value;
    node->value = box;

    // StackPush(ui_state.parent_stack.top, node);
    node->next = ui_state.parent_stack.top;
    ui_state.parent_stack.top = node;

    return old_value;
}

UI_Box *UI_PopParent(void)
{
    UI_ParentNode *popped = ui_state->parent_stack.top;
    if (popped != &ui_state->parent_nil_stack_top)
    {
        // StackPop(ui_state->parent_stack.top);
        if (ui_state.parent_stack.top)
        {
            ui_state.parent.top = ui_state.parent.top->next;
        }

        // StackPush(ui_state->parent_stack.free, popped);
        popped->next = ui_state.parent_stack.free;
        ui_state.parent_stack.top = popped;
    }

    return popped;
}

#define UI_Parent(v) DeferLoop(UI_PushParent(v), UI_PopParent())

void UI_RenderBox(UI_Box *box)
{
    if (!box) return;

    if (box->flags & UI_BoxFlag_DrawBackground)
    {
        DrawRectangleRec(box->rect, GRAY);
    }

    if (box->flags & UI_BoxFlag_AnimationHot)
    {
        DrawRectangleRec(box->rect, GRAY);
    }

    if (box->flags & UI_BoxFlag_DrawBackground)
    {
        DrawRectangleRec(box->rect, GRAY);
    }

    // Draw border if requested
    if (box->flags & UI_BoxFlag_DrawBorder)
    {
        DrawRectangleLinesEx(box->rect, 2.0f, DARKGRAY);
    }

    // Draw text if requested
    if (box->flags & UI_BoxFlag_DrawText)
    {
        const char *text = box->string.data;
        int fontSize = 20;
        Vector2 textSize = MeasureTextEx(ui_state.font, text, fontSize, 1.0f);
        Vector2 textPos = {
            box->rect.x + (box->rect.width - textSize.x) / 2,
            box->rect.y + (box->rect.height - textSize.y) / 2
        };
        DrawTextEx(ui_state.font, text, textPos, fontSize, 1.0f, BLACK);
    }
}

void UI_BeginBuild(int win_x, int win_y, float dt)
{
}

void UI_EndBuild()
{
}

void UI_Render()
{
    for (UI_Box *child = ui_state.start; child; child = child->next)
    {
        UI_RenderBox(child);
    }
    ui_state.start = 0;
}

#ifdef ENGINE_UNIT
int main(void)
{
    InitWindow(480, 270, "UI Demo");
    ui_state.parent_nil_stack_top.value = &ui_g_nil_box;
    ui_state.parent_nil_stack_top.next = 0;
    ui_state.parent_stack.top = &ui_state.parent_nil_stack_top;

    while (!WindowShouldClose())
    {
        UI_BeginBuild(480, 270, 1.0 / 60.0f);
        if (UI_Button(S("Click me!")))
        {
            TraceLog(LOG_INFO, "You clicked me!");
        }
        UI_EndBuild();
        
        BeginDrawing();

        ClearBackground(RAYWHITE);
        UI_Render();
        EndDrawing();
    }
}
#endif
