/*******************************************************************************
*
*   GON_C v1.0 - A C implementation of Glaiel Object Notation
*                                      https://github.com/TylerGlaiel/GON
*
*   LICENSE: MIT
*   Copyright (c) 2025 davinci789
*
*   Permission is hereby granted, free of charge, to any person obtaining a copy
*   of this software and associated documentation files (the "Software"), to deal
*   in the Software without restriction, including without limitation the rights
*   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*   copies of the Software, and to permit persons to whom the Software is
*   furnished to do so, subject to the following conditions:
*
*   The above copyright notice and this permission notice shall be included in all
*   copies or substantial portions of the Software.
*
*   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*   SOFTWARE.
*
********************************************************************************/

#ifndef GON_H
#define GON_H

#define GON_MAX_SUB_OBJECT_DEPTH 5

#if defined(_WIN32)
    // Microsoft attibutes to tell compiler that symbols are imported/exported from a .dll
    #if defined(BUILD_LIBTYPE_SHARED)
        #define GON_API __declspec(dllexport)     // We are building gon_c as a Win32 shared library (.dll)
    #elif defined(USE_LIBTYPE_SHARED)
        #define GON_API __declspec(dllimport)     // We are using gon_c as a Win32 shared library (.dll)
    #else
        #define GON_API   // We are building or using gon_c as a static library
    #endif
#else
    #if defined (__ELF__)
        #define GON_API __attribute__((visibility "default"))
    #else
        #define GON_API
    #endif
#endif

#include <stdint.h>

typedef struct
{
    void *(*malloc)(ptrdiff_t, void *ctx);
    void  (*free)(void *, void *ctx);
    void   *ctx;
} GON_Allocator;

typedef enum
{
    GON_Ident = 1,
    GON_Widget,
    GON_Block,
    GON_List,
    GON_Block_End,
    GON_List_End,
} GON_Type;

typedef enum
{
    GON_Subtype_Anonymous = 1 << 0,
    GON_Subtype_List_Item = 1 << 1,
    GON_Subtype_String    = 1 << 2,
} GON_Subtype;

typedef struct GON_Object GON_Object;
struct GON_Object
{
    struct {
        char *data;
        ptrdiff_t len;
    }name;

    struct {
        char *data;
        ptrdiff_t len;
    }value;

    GON_Object *parent;

    GON_Object *children;
    int children_len;

    GON_Type type;
    GON_Subtype subtype;
};

typedef struct
{
    GON_Object *results;
    _Bool ok;

    void *free_this;
    ptrdiff_t free_this_size;

    int top_level_results_len;
    int all_results_len;
} GON_Results;

/* Essential parsing functions */

GON_API int gon_object_count(char *source);
GON_API int gon_object_count2(char *source, ptrdiff_t source_len);

/* `source` should live as long as all of your GON_Objects. */
/* Each GON_Object's name and value fields point somewhere in `source`. */
GON_API GON_Results gon_load(char *source);
GON_API GON_Results gon_load2(char *source, ptrdiff_t source_len);
GON_API GON_Results gon_load3(char *source, ptrdiff_t source_len, GON_Allocator *alloc);

GON_API void gon_free(GON_Results results);
GON_API void gon_free1(GON_Results results, GON_Allocator alloc);

/* Querying functions */

GON_API GON_Object gon_top_level(GON_Results results, char *name);
GON_API GON_Object gon_top_level1(GON_Results results, char *name, ptrdiff_t name_len);
GON_API GON_Object gon_get(GON_Object object, char *name);
GON_API GON_Object gon_get1(GON_Object object, char *name, ptrdiff_t name_len);

GON_API int gon_children_total(GON_Object object);
GON_API int gon_children_total2(GON_Object a, GON_Object b);

#endif // GON_H

#ifdef GON_IMPLEMENTATION
#include "gon.c"
#endif
