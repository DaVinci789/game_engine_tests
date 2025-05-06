#ifndef GON_C
#define GON_C

#include "gon.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

typedef struct
{
    char *data;
    ptrdiff_t len;
} GON_Str;

#define S(s) (GON_Str) {s, sizeof(s) - 1} // Str from literal
#define U(d, l) (GON_Str) {d, l}          // User supplied data
#define R(lh, d) (__typeof__(lh)) {d.data, d.len}          // Data to return to userland

typedef struct
{
    GON_Str   head;
    GON_Str   tail;
    _Bool ok;
} GON_Cut;

typedef struct
{
    char *beg;
    char *end;
} GON_Linear_Allocator;

typedef enum
{
    GON_Token_Nothing,
    GON_Token_Ident,
    GON_Token_Block_Begin,
    GON_Token_Block_End,
    GON_Token_List_Begin,
    GON_Token_List_End,
    GON_Token_Colon,
    GON_Token_Comma,
} GON_Token_Type;

typedef struct
{
    GON_Str str;
    GON_Token_Type type;
    _Bool is_string;
} GON_Token;

typedef struct
{
    GON_Token token;
    _Bool success;
} GON_Token_Result;

typedef struct GON_Single_Result
{
    GON_Object result;
    _Bool success;
} GON_Single_Result;

typedef struct
{
    char *source;
    ptrdiff_t source_len;
    GON_Str current;
    // this being an immediate api, need to store the stack here
    // rather than using the call stack
    _Bool in_list_depth[GON_MAX_SUB_OBJECT_DEPTH + 1];
    int current_list_depth;

    _Bool error;
} GON_State;

static GON_Str span(char *beg, char *end)
{
    GON_Str r = {0};
    r.data = beg;
    r.len  = beg ? end-beg : 0;
    return r;
}

static _Bool equals(GON_Str a, GON_Str b)
{
    return a.len == b.len && (!a.len || !memcmp(a.data, b.data, a.len));
}

static GON_Str narrow(GON_Str s, ptrdiff_t i)
{
    s.len = i >= s.len ? 0 : i;
    return s;
}

static GON_Str substring(GON_Str s, ptrdiff_t i)
{
    if (i) {
        s.data += i;
        s.len  -= i;
    }
    return s;
}

static GON_Cut cut(GON_Str s, char c)
{
    GON_Cut r = {0};
    if (!s.len) return r;  // null pointer special case
    char *beg = s.data;
    char *end = s.data + s.len;
    char *cut = beg;
    for (; cut<end && *cut!=c; cut++) {}
    r.ok   = cut < end;
    r.head = span(beg, cut);
    r.tail = span(cut+r.ok, end);
    return r;
}

#define new(a, t, n)  (t *)alloc(a, n, sizeof(t), _Alignof(t))
static void *alloc(GON_Linear_Allocator *a, ptrdiff_t count, ptrdiff_t size, ptrdiff_t align)
{
    ptrdiff_t pad = (uintptr_t)a->end & (align - 1);
    if (count > (a->end - a->beg - pad)/size) {
        return 0;
    }
    void *r = a->beg + pad;
    a->beg += pad + count*size;
    return memset(r, 0, count*size);
}

#ifndef GON_IGNORE_STD_ALLOC
static void *gon_std_malloc(ptrdiff_t size, void *ctx)
{
    (void) ctx;
    return malloc(size);
}

static void gon_std_free(void *ptr, void *ctx)
{
    (void) ctx;
    free(ptr);
}
#endif

static GON_Allocator gon_get_stdlib_allocator()
{
    GON_Allocator allocator = {0};

#ifndef GON_IGNORE_STD_ALLOC
    allocator.malloc = &gon_std_malloc;
    allocator.free   = &gon_std_free;
#endif

    return allocator;
}

static GON_Token_Result gon_next_token(GON_State *state)
{
    GON_Token_Result result = {0};

    GON_Str comment_begin = S("--");
    GON_Str multiline_comment_begin = S("--[[");
    GON_Str multiline_comment_end = S("]]");

    while (state->current.len > 0)
    {
        /* Keep advancing until we fail to hit a whitespace character */
        _Bool done = 1;

        char c = state->current.data[0];
        if (c == ' ' ||
            c =='\r' ||
            c =='\t' ||
            c =='\n')
        {
            state->current = substring(state->current, 1);
            done = 0;
        }
        else if (equals(multiline_comment_begin, narrow(state->current, multiline_comment_begin.len)))
        {
            while (state->current.len > 0 && !equals(multiline_comment_end, narrow(state->current, multiline_comment_end.len)))
            {
                state->current = substring(state->current, 1);
            }
            if (equals(multiline_comment_end, narrow(state->current, multiline_comment_end.len)))
            {
                state->current = substring(state->current, multiline_comment_end.len);
            }
            done = 0;
        }
        else if (equals(comment_begin, narrow(state->current, 2)))
        {
            while (state->current.len > 0 && (c = state->current.data[0]) != '\n')
            {
                state->current = substring(state->current, 1);
            }
            state->current = substring(state->current, 1); // overstep newline char
            done = 0;
        }

        if (done)
        {
            break;
        }
    }

    GON_Token_Type type = 0;
    if (state->current.len > 0)
    {
        _Bool is_string = 0;
        result.token.str.data = state->current.data;
        result.success = 1;
        char c = state->current.data[0];
        switch (c)
        {
        case '{':
            type = GON_Token_Block_Begin;
            state->current = substring(state->current, 1);
            break;
        case '}':
            type = GON_Token_Block_End;
            state->current = substring(state->current, 1);
            break;
        case '[':
            type = GON_Token_List_Begin;
            state->current = substring(state->current, 1);
            break;
        case ']':
            type = GON_Token_List_End;
            state->current = substring(state->current, 1);
            break;
        case ':':
            type = GON_Token_Colon;
            state->current = substring(state->current, 1);
            break;
        case ',':
            type = GON_Token_Comma;
            state->current = substring(state->current, 1);
            break;
        case '"':
            type = GON_Token_Ident;
            result.token.str.data++; // needs result.token.str.len--?
            result.token.is_string = 1;
            is_string = 1;
            state->current = substring(state->current, 1);
            _Bool escape = 0;
            while (state->current.len > 0 && (escape || (c = state->current.data[0]) != '"'))
            {
                c = state->current.data[0];

                if (escape)
                {
                    escape = 0;
                }
                else if (c == '\\')
                {
                    escape = 1;
                }

                state->current = substring(state->current, 1);
            }
            state->current = substring(state->current, 1);
            break;
        default:
            type = GON_Token_Ident;
            while (!(c == ' ' || c == ',' ||
                     c == ':' || c == '\n' ||
                     c == '\t' || c == '\r' ||
                     c == '}' || c == ']' ||
                     c == '{' || c == '[')
                   && state->current.len > 0)
            {
                state->current = substring(state->current, 1);
                c = state->current.len > 0 ? state->current.data[0] : 0;
            }
            break;
        }

        result.token.str.len = state->current.data - result.token.str.data;
        if (is_string)
        {
            result.token.str.len -= 1;
        }
    }

    result.token.type = type;

    return result;
}

static GON_Single_Result gon_next_object(GON_State *state)
{
    GON_Single_Result result = {0};
    GON_Object *user = &result.result;

    _Bool go_again = 0;

    do
    {
        go_again = 0;
        GON_Token current = gon_next_token(state).token;

        if (state->in_list_depth[state->current_list_depth])
        {
            user->subtype |= GON_Subtype_List_Item;
        }

        if (current.is_string)
        {
            user->subtype |= GON_Subtype_String;
        }

        user->name = R(user->name, current.str);

        switch (current.type)
        {
        case GON_Token_Block_Begin: {
            result.success = 1;
            user->type = GON_Block;
            user->subtype |= GON_Subtype_Anonymous;
            if (state->in_list_depth[state->current_list_depth])
            {
                if (state->current_list_depth + 1 < GON_MAX_SUB_OBJECT_DEPTH)
                {
                    state->current_list_depth += 1;
                }
                else
                {
                // todo bounds checking
                    result.success = 0;
                    state->error = 1;
                }
            }
            break;
        }
        case GON_Token_Block_End: {
            result.success = 1;
            user->type = GON_Block_End;
            if (state->current_list_depth - 1 > -1 && state->in_list_depth[state->current_list_depth - 1])
            {
                if (state->current_list_depth - 1 >= 0)
                {
                    state->current_list_depth -= 1;
                }
                else
                {
                    result.success = 0;
                    state->error = 1;
                }
            }
            break;
        }
        case GON_Token_List_Begin: {
            result.success = 1;
            user->type = GON_List;
            user->subtype |= GON_Subtype_Anonymous;
    
            if (state->current_list_depth + 1 < GON_MAX_SUB_OBJECT_DEPTH)
            {
                state->current_list_depth += 1;
                state->in_list_depth[state->current_list_depth] = 1;
            }
            else
            {
                state->error = 1;
            }
            break;
        }
        case GON_Token_List_End: {
            if (state->in_list_depth[state->current_list_depth])
            {
                result.success = 1;
                user->type = GON_List_End;
    
                state->in_list_depth[state->current_list_depth] = 0;
    
                if (state->current_list_depth - 1 >= 0)
                {
                    state->current_list_depth -= 1;
                }
    
                GON_State peek_state = *state;
                GON_Token peek = gon_next_token(&peek_state).token;
                if (peek.type == GON_Token_Comma)
                {
                    gon_next_token(state);
                }
            }
            break;
        }
        case GON_Token_Ident: {
            result.success = 1;
            GON_State peek_state = *state;
            GON_Token peek = gon_next_token(&peek_state).token;
    
            if (peek.type > 0)
            {
                if (peek.type != GON_Token_Colon && state->in_list_depth[state->current_list_depth])
                {
                    if (peek.type == GON_Token_Comma)
                    {
                        gon_next_token(state);
                    }
    
                    user->type  = GON_Ident;
                }
                else
                {
                    GON_Token context = gon_next_token(state).token;
    
                    if (context.type == GON_Token_Comma)
                    {
                        context = gon_next_token(state).token;
                    }
                    else if (context.type == GON_Token_Colon)
                    {
                        context = gon_next_token(state).token;
                    }
    
                    switch (context.type)
                    {
                    case GON_Token_Block_Begin:
                        user->type = GON_Block;
                        break;
                    case GON_Token_List_Begin:
                        user->type = GON_List;
                        if (state->current_list_depth + 1 < GON_MAX_SUB_OBJECT_DEPTH)
                        {
                            state->current_list_depth += 1;
                            state->in_list_depth[state->current_list_depth] = 1;
                        }
                        else
                        {
                            state->error = 1;
                        }
                        break;
                    case GON_Token_Ident:
                        user->type  = GON_Widget;
                        user->value = R(user->value, context.str);
                        break;
                    default:
                        //assert(0);
                        break;
                    }
                }
            }
            break;
        }
        case GON_Token_Colon: {
            break;
        }
        case GON_Token_Comma: {
            // This happens if it's a comma at the end of a block or list.
            // The lexer parsers the block/list entirely and spits out the 
            // raw comma next.
            // When that happens, just get the next object.
            go_again = 1;
            break;
        }
        }
    
        if (state->error)
        {
            result.success = 0;
        }
    } while (go_again);

    return result;
}

static GON_Results gon_objects(GON_State *base_state, GON_Linear_Allocator *arena)
{
    GON_Results result = {0};

    int num_top_level_objects = 0;
    int num_non_terminator_objects = 0;

    GON_State state = *base_state;
    GON_Single_Result raw_result = {0};
    int scope = 0;
    while ((raw_result = gon_next_object(&state)).success)
    {
        GON_Object object = raw_result.result;

        if (object.type != GON_Block_End && object.type != GON_List_End)
        {
            num_non_terminator_objects += 1;
        }

        if (scope == 0)
        {
            num_top_level_objects += 1;
            switch (object.type)
            {
            case GON_Block:
            case GON_List:
                scope += 1;
                break;
            default:
                break;
            }
        }
        else
        {
            switch (object.type)
            {
            case GON_Block:
            case GON_List:
                scope += 1;
                break;
            case GON_Block_End:
            case GON_List_End:
                scope -= 1;
                break;
            default:
                break;
            }
        }
    }

    /*    Add blocks and their children to buffer `final`.
     *    The resulting memory layout looks something like this:

     *    +-----------+
     *    | Top Level |
     *    |   Scope   |
     *    +-----------+
     *    | Inner     |
     *    | Scope  #1 |
     *    +-----------+
     *    | Inner     |
     *    | Scope  #2 |
     *    +-----------+
     *    | Inner     |
     *    | Scope ... |
     *    +-----------+
     *    | Inner     |
     *    | Scope  #N |
     *    +-----------+

     *    As an example, an input like the following would return the following result:

     *    a {
     *      b 10
     *      c 15
     *      d {
     *        z 2
     *        y {
     *          innermost X
     *        }
     *        x 6
     *      }
     *      e 20
     *    }

     *    +-----------+
     *    |     a     |  Top Level Scope
     *    |  (Block)  |
     *    +-----------+
     *    |   b  10   |  Inner Scope #1
     *    |           |
     *    +-----------+
     *    |   c  15   |  ""
     *    |           |
     *    +-----------+
     *    |     d     |  ""
     *    |  (Block)  |
     *    +-----------+
     *    |   e  20   |  ""
     *    |           |
     *    +-----------+
     *    |   z  2    |  Inner Scope #2
     *    |           |
     *    +-----------+
     *    |     y     |  ""
     *    |  (Block)  |
     *    +-----------+
     *    |   x  6    |  ""
     *    |           |
     *    +-----------+
     *    | innermost |  Inner Scope #3
     *    |     X     |
     *    +-----------+

     *    Since the children are adjacent in memory, this allows the
     *    GON_Object struct to store its children as just a pointer + length.

     *    The way we do this is we first add all of the top level blocks
     *    to `final`.

     *    Then we just iterate straight through all of `final`'s slots.
     *    For each block or list encountered, add all children that are
     *    _one scope inwards_ of that object to the end of `final`.
     *    (breadth first rather than depth first)

     *    It's important that we only get the first level children since we
     *    don't know how many more children of the current scope we have yet
     *    to add. If we added children depth first, it would break the
     *    contigious memory. Example:

     *    d {
     *      z 2
     *      y {
     *        innermost X
     *      }
     *      x 6
     *    }

     *    +-----------+
     *    |     d     | d's children starting from `z` is no longer z,y,x
     *    |  (Block)  | there's the `innermost` object inbetween.
     *    +-----------+
     *    |   z  2    |
     *    |           |
     *    +-----------+
     *    |     y     |
     *    |  (Block)  |
     *    +-----------+
     *    | innermost | :(
     *    |     X     |
     *    +-----------+
     *    |   x  6    |
     *    |(Separated)|
     *    +-----------+

     *    Done properly, read left to right, top to bottom:

     *     current +-----------+  current +-----------+    +-----------+    +-----------+
     *     object  |     d     |  object  |     d     | co |     d     | co |     d     |
     *     ---->   |  (Block)  |  ---->   |  (Block)  | -> |  (Block)  | -> |  (Block)  |
     *             +-----------+          +-----------+    +-----------+    +-----------+
     *                                    |   z  2    |    |   z  2    |    |   z  2    |   
     *                                    |           |    |           |    |           |
     *                                    +-----------+    +-----------+    +-----------+
     *                                                     |     y     |    |     y     |
     *                                                     |  (Block)  |    |  (Block)  |
     *                                                     +-----------+    +-----------+
     *                                                                      |   x  6    |
     *                                                                      |           |
     *                                                                      +-----------+

     *     Then we iterate to the next Block or List...

     *        +-----------+    +-----------+     +-----------+              +-----------+
     *     co |     d     |    |     d     |     |     d     |              |     d     |
     *     -> |  (Block)  |    |  (Block)  |     |  (Block)  |              |  (Block)  |
     *        +-----------+    +-----------+     +-----------+              +-----------+
     *        |   z  2    | co |   z  2    |     |   z  2    |              |   z  2    |
     *        |           | -> |           |     |           |              |           |
     *        +-----------+    +-----------+     +-----------+              +-----------+
     *        |     y     |    |     y     |  co |     y     | Now that  co |     y     |
     *        |  (Block)  |    |  (Block)  |  -> |  (Block)  | we found  -> |  (Block)  |
     *        +-----------+    +-----------+     +-----------+ it, we       +-----------+
     *        |   x  6    |    |   x  6    |     |   x  6    | add its      |   x  6    |
     *        |           |    |           |     |           | children.    |           |
     *        +-----------+    +-----------+     +-----------+              +-----------+
     *                                                                   _\ | innermost | /_
     *                                                                    / |     X     | \
     *                                                                      +-----------+

     *    And we do that until we reach the end of `final`.

     *   A benefit to adding objects to the same memory we're iterating over
     *   is that we never have to worry about running into an
     *   uninitialized block in memory.
    */

    if (num_non_terminator_objects > 0)
    {
        GON_Object *final = new(arena, GON_Object, num_non_terminator_objects);
        GON_Object *current = final;

        state = *base_state;
        scope = 0;

        // Pass 1: Collect top-level objects.
        int num_appended = 0;
        while ((raw_result = gon_next_object(&state)).success) {
            GON_Object object = raw_result.result;
            if (scope == 0 && object.type != GON_Block_End)
            {
                *current = object;
                current++;
                num_appended += 1;
            }

            if (object.type == GON_Block || object.type == GON_List) {
                scope += 1;
            } else if (object.type == GON_Block_End || object.type == GON_List_End) {
                scope -= 1;
            }
        }

        // Second pass: Collect direct children for each object
        for (int i = 0; i < num_non_terminator_objects; i++) {
            GON_Object *parent = &final[i];
            if (parent->type == GON_Block || parent->type == GON_List) {
                state = *base_state;

                // Assume that the name of an Object always points to its place in the source.
                state.current = span(parent->name.data, base_state->source + base_state->source_len);

                // Strings break the above assumption. Their name field begins one character in
                // as to not overlap their surrounding quotes.
                // So to regain this assumption, we expand the string to overlap the quotes again.
                if (parent->subtype & GON_Subtype_String)
                {
                    state.current.data--;
                    state.current.len++;
                }
                gon_next_object(&state); // Skip over processing parent

                // Process direct children, children with scope of 1.
                scope = 1; 
                while (scope > 0 && (raw_result = gon_next_object(&state)).success) {
                    GON_Object object = raw_result.result;

                    if (scope == 1 && object.type != GON_Block_End && object.type != GON_List_End) {
                        if (parent->children_len == 0) {
                            parent->children = current;
                        }
                        *current = object;
                        current->parent = parent;
                        current++;
                        parent->children_len += 1;
                    }
                    if (object.type == GON_Block || object.type == GON_List) {
                        scope += 1;
                    } else if (object.type == GON_Block_End || object.type == GON_List_End) {
                        scope -= 1;
                    }
                }
            }
        }

        result.results = final;
        result.top_level_results_len = num_top_level_objects;
        result.all_results_len = num_non_terminator_objects;
        result.ok = 1;
    }

    return result;
}

GON_API int gon_object_count(char *source)
{
    return gon_object_count2(source, strlen(source));
}

GON_API int gon_object_count2(char *source, ptrdiff_t source_len)
{
    GON_State state = {0};
    state.source = source;
    state.source_len = source_len;
    state.current.data = source;
    state.current.len  = source_len;

    int count = 0;
    GON_Single_Result result;

    while ((result = gon_next_object(&state)).success)
    {
        GON_Object object = result.result;

        if (object.type != GON_Block_End && object.type != GON_List_End)
        {
            count += 1;
        }
    }

    return count;
}

GON_API GON_Results gon_load(char *source)
{
    ptrdiff_t source_len = strlen(source);

    GON_Allocator allocator = gon_get_stdlib_allocator();
    ptrdiff_t cap = sizeof(GON_Object) * (1 + gon_object_count2(source, source_len));
    char *mem = allocator.malloc(cap, allocator.ctx);
    GON_Linear_Allocator perm = {0};
    perm.beg = mem;
    perm.end = mem + cap;

    GON_State state = {0};
    state.source = source;
    state.source_len = source_len;
    state.current.data = source;
    state.current.len  = source_len;

    GON_Results results = gon_objects(&state, &perm);
    results.free_this = mem;
    results.free_this_size = cap;
    return results;
}

GON_API GON_Results gon_load2(char *source, ptrdiff_t source_len)
{
    GON_Allocator allocator = gon_get_stdlib_allocator();
    ptrdiff_t cap = sizeof(GON_Object) * (1 + gon_object_count2(source, source_len));
    char *mem = allocator.malloc(cap, allocator.ctx);
    GON_Linear_Allocator perm = {0};
    perm.beg = mem;
    perm.end = mem + cap;

    GON_State state = {0};
    state.source = source;
    state.source_len = source_len;
    state.current.data = source;
    state.current.len  = source_len;

    GON_Results results = gon_objects(&state, &perm);
    results.free_this = mem;
    results.free_this_size = cap;
    return results;
}

GON_API GON_Results gon_load3(char *source, ptrdiff_t source_len, GON_Allocator *allocator)
{
    ptrdiff_t cap = sizeof(GON_Object) * (1 + gon_object_count2(source, source_len));
    char *mem = allocator->malloc(cap, allocator->ctx);
    GON_Linear_Allocator perm = {0};
    perm.beg = mem;
    perm.end = mem + cap;

    GON_State state = {0};
    state.source = source;
    state.source_len = source_len;
    state.current.data = source;
    state.current.len  = source_len;

    return gon_objects(&state, &perm);
}

GON_API int gon_children_total(GON_Object object)
{
    int total = object.children_len;
    for (int i = 0; i < object.children_len; i++)
    {
        total += gon_children_total(object.children[i]);
    }
    return total;
}

GON_API int gon_children_total2(GON_Object a, GON_Object b)
{
    return gon_children_total(a) + gon_children_total(b);
}

GON_API GON_Object gon_top_level(GON_Results results, char *name)
{
    return gon_top_level1(results, name, strlen(name));
}

GON_API GON_Object gon_top_level1(GON_Results results, char *name, ptrdiff_t name_len)
{
    GON_Object ret = {0};

    GON_Str hname = U(name, name_len); // handled name
    for (int i = 0; i < results.top_level_results_len; i++)
    {
        if (equals(hname, U(results.results[i].name.data, results.results[i].name.len)))
        {
            ret = results.results[i];
        }
    }

    return ret;
}

GON_API GON_Object gon_get(GON_Object object, char *name)
{
    return gon_get1(object, name, strlen(name));
}

GON_API GON_Object gon_get1(GON_Object object, char *name, ptrdiff_t name_len)
{
    GON_Object ret = {0};

    GON_Str hname = U(name, name_len); // handled name

    for (int i = 0; i < object.children_len; i++)
    {
        if (equals(hname, U(object.children[i].name.data, object.children[i].name.len)))
        {
            ret = object.children[i];
        }
    }

    return ret;
}

GON_API void gon_free(GON_Results results)
{
    GON_Allocator alloc = gon_get_stdlib_allocator();
    gon_free1(results, alloc);
}

GON_API void gon_free1(GON_Results results, GON_Allocator alloc)
{
    alloc.free(results.free_this, alloc.ctx);
}

#undef new
#undef R
#undef U
#undef S

#endif // GON_C
