/* wmem_test.c
 * Wireshark Memory Manager Tests
 * Copyright 2012, Evan Huus <eapache@gmail.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <stdio.h>
#include <glib.h>

#include "wmem.h"
#include "wmem_tree-int.h"
#include "wmem_allocator.h"
#include "wmem_allocator_block.h"
#include "wmem_allocator_block_fast.h"
#include "wmem_allocator_simple.h"
#include "wmem_allocator_strict.h"

#include <wsutil/time_util.h>

#define STRING_80               "12345678901234567890123456789012345678901234567890123456789012345678901234567890"
#define MAX_ALLOC_SIZE          (1024*64)
#define MAX_SIMULTANEOUS_ALLOCS  1024
#define CONTAINER_ITERS          10000

typedef void (*wmem_verify_func)(wmem_allocator_t *allocator);

/* A local copy of wmem_allocator_new that ignores the
 * WIRESHARK_DEBUG_WMEM_OVERRIDE variable so that test functions are
 * guaranteed to actually get the allocator type they asked for */
static wmem_allocator_t *
wmem_allocator_force_new(const wmem_allocator_type_t type)
{
    wmem_allocator_t *allocator;

    allocator = wmem_new(NULL, wmem_allocator_t);
    allocator->type = type;
    allocator->callbacks = NULL;
    allocator->in_scope = true;

    switch (type) {
        case WMEM_ALLOCATOR_SIMPLE:
            wmem_simple_allocator_init(allocator);
            break;
        case WMEM_ALLOCATOR_BLOCK:
            wmem_block_allocator_init(allocator);
            break;
        case WMEM_ALLOCATOR_BLOCK_FAST:
            wmem_block_fast_allocator_init(allocator);
            break;
        case WMEM_ALLOCATOR_STRICT:
            wmem_strict_allocator_init(allocator);
            break;
        default:
            g_assert_not_reached();
            /* This is necessary to squelch MSVC errors; is there
               any way to tell it that g_assert_not_reached()
               never returns? */
            return NULL;
    };

    return allocator;
}

/* A helper for generating pseudo-random strings. Just uses glib's random number
 * functions to generate 'numbers' in the printable character range. */
static char *
wmem_test_rand_string(wmem_allocator_t *allocator, int minlen, int maxlen)
{
    char *str;
    int len, i;

    len = g_random_int_range(minlen, maxlen);

    /* +1 for null-terminator */
    str = (char*)wmem_alloc(allocator, len + 1);
    str[len] = '\0';

    for (i=0; i<len; i++) {
        /* ASCII normal printable range is 32 (space) to 126 (tilde) */
        str[i] = (char) g_random_int_range(32, 126);
    }

    return str;
}

static int
wmem_test_compare_uint32(const void *a, const void *b)
{
    uint32_t l, r;

    l = *(const uint32_t*)a;
    r = *(const uint32_t*)b;

    return l - r;
}

/* Some helpers for properly testing callback functionality */
wmem_allocator_t *expected_allocator;
void             *expected_user_data;
wmem_cb_event_t   expected_event;
int               cb_called_count;
int               cb_continue_count;
bool              value_seen[CONTAINER_ITERS];

static bool
wmem_test_cb(wmem_allocator_t *allocator, wmem_cb_event_t event,
        void *user_data)
{
    g_assert_true(allocator == expected_allocator);
    g_assert_true(event     == expected_event);

    cb_called_count++;

    return *(bool*)user_data;
}

static bool
wmem_test_foreach_cb(const void *key _U_, void *value, void *user_data)
{
    g_assert_true(user_data == expected_user_data);

    g_assert_true(! value_seen[GPOINTER_TO_INT(value)]);
    value_seen[GPOINTER_TO_INT(value)] = true;

    cb_called_count++;
    cb_continue_count--;

    return (cb_continue_count == 0);
}

/* ALLOCATOR TESTING FUNCTIONS (/wmem/allocator/) */

static void
wmem_test_allocator_callbacks(void)
{
    wmem_allocator_t *allocator;
    bool t = true;
    bool f = false;
    unsigned cb_id;

    allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);

    expected_allocator = allocator;

    wmem_register_callback(expected_allocator, &wmem_test_cb, &f);
    wmem_register_callback(expected_allocator, &wmem_test_cb, &f);
    cb_id = wmem_register_callback(expected_allocator, &wmem_test_cb, &t);
    wmem_register_callback(expected_allocator, &wmem_test_cb, &t);
    wmem_register_callback(expected_allocator, &wmem_test_cb, &f);

    expected_event = WMEM_CB_FREE_EVENT;

    cb_called_count = 0;
    wmem_free_all(allocator);
    g_assert_true(cb_called_count == 5);

    cb_called_count = 0;
    wmem_free_all(allocator);
    g_assert_true(cb_called_count == 2);

    cb_called_count = 0;
    wmem_free_all(allocator);
    g_assert_true(cb_called_count == 2);

    wmem_unregister_callback(allocator, cb_id);
    cb_called_count = 0;
    wmem_free_all(allocator);
    g_assert_true(cb_called_count == 1);

    cb_id = wmem_register_callback(expected_allocator, &wmem_test_cb, &f);
    wmem_register_callback(expected_allocator, &wmem_test_cb, &t);

    cb_called_count = 0;
    wmem_free_all(allocator);
    g_assert_true(cb_called_count == 3);

    wmem_unregister_callback(allocator, cb_id);
    cb_called_count = 0;
    wmem_free_all(allocator);
    g_assert_true(cb_called_count == 2);

    wmem_register_callback(expected_allocator, &wmem_test_cb, &t);

    expected_event = WMEM_CB_DESTROY_EVENT;
    cb_called_count = 0;
    wmem_destroy_allocator(allocator);
    g_assert_true(cb_called_count == 3);
}

static void
wmem_test_allocator_det(wmem_allocator_t *allocator, wmem_verify_func verify,
        unsigned len)
{
    int i;
    char *ptrs[MAX_SIMULTANEOUS_ALLOCS];

    /* we use wmem_alloc0 in part because it tests slightly more code, but
     * primarily so that if the allocator doesn't give us enough memory or
     * gives us memory that includes its own metadata, we write to it and
     * things go wrong, causing the tests to fail */
    for (i=0; i<MAX_SIMULTANEOUS_ALLOCS; i++) {
        ptrs[i] = (char *)wmem_alloc0(allocator, len);
    }
    for (i=MAX_SIMULTANEOUS_ALLOCS-1; i>=0; i--) {
        /* no wmem_realloc0 so just use memset manually */
        ptrs[i] = (char *)wmem_realloc(allocator, ptrs[i], 4*len);
        memset(ptrs[i], 0, 4*len);
    }
    for (i=0; i<MAX_SIMULTANEOUS_ALLOCS; i++) {
        wmem_free(allocator, ptrs[i]);
    }

    if (verify) (*verify)(allocator);
    wmem_free_all(allocator);
    wmem_gc(allocator);
    if (verify) (*verify)(allocator);
}

static void
wmem_test_allocator_jumbo(wmem_allocator_type_t type, wmem_verify_func verify)
{
    wmem_allocator_t *allocator;
    char *ptr, *ptr1;

    allocator = wmem_allocator_force_new(type);

    ptr = (char*)wmem_alloc0(allocator, 4*1024*1024);
    wmem_free(allocator, ptr);
    wmem_gc(allocator);
    ptr = (char*)wmem_alloc0(allocator, 4*1024*1024);

    if (verify) (*verify)(allocator);
    wmem_free(allocator, ptr);
    wmem_gc(allocator);
    if (verify) (*verify)(allocator);

    ptr  = (char *)wmem_alloc0(allocator, 10*1024*1024);
    ptr1 = (char *)wmem_alloc0(allocator, 13*1024*1024);
    ptr1 = (char *)wmem_realloc(allocator, ptr1, 10*1024*1024);
    memset(ptr1, 0, 10*1024*1024);
    ptr = (char *)wmem_realloc(allocator, ptr, 13*1024*1024);
    memset(ptr, 0, 13*1024*1024);
    if (verify) (*verify)(allocator);
    wmem_gc(allocator);
    if (verify) (*verify)(allocator);
    wmem_free(allocator, ptr1);
    if (verify) (*verify)(allocator);
    wmem_free_all(allocator);
    wmem_gc(allocator);
    if (verify) (*verify)(allocator);

    wmem_destroy_allocator(allocator);
}

static void
wmem_test_allocator(wmem_allocator_type_t type, wmem_verify_func verify,
        int iterations)
{
    int i;
    char *ptrs[MAX_SIMULTANEOUS_ALLOCS];
    wmem_allocator_t *allocator;

    allocator = wmem_allocator_force_new(type);

    if (verify) (*verify)(allocator);

    /* start with some fairly simple deterministic tests */

    wmem_test_allocator_det(allocator, verify, 8);

    wmem_test_allocator_det(allocator, verify, 64);

    wmem_test_allocator_det(allocator, verify, 512);

    for (i=0; i<MAX_SIMULTANEOUS_ALLOCS; i++) {
        ptrs[i] = wmem_alloc0_array(allocator, char, 32);
    }

    if (verify) (*verify)(allocator);
    wmem_free_all(allocator);
    wmem_gc(allocator);
    if (verify) (*verify)(allocator);

    /* now do some random fuzz-like tests */

    /* reset our ptr array */
    for (i=0; i<MAX_SIMULTANEOUS_ALLOCS; i++) {
        ptrs[i] = NULL;
    }

    /* Run enough iterations to fill the array 32 times */
    for (i=0; i<iterations; i++) {
        int ptrs_index;
        int new_size;

        /* returns value 0 <= x < MAX_SIMULTANEOUS_ALLOCS which is a valid
         * index into ptrs */
        ptrs_index = g_test_rand_int_range(0, MAX_SIMULTANEOUS_ALLOCS);

        if (ptrs[ptrs_index] == NULL) {
            /* if that index is unused, allocate some random amount of memory
             * between 0 and MAX_ALLOC_SIZE */
            new_size = g_test_rand_int_range(0, MAX_ALLOC_SIZE);

            ptrs[ptrs_index] = (char *) wmem_alloc0(allocator, new_size);
        }
        else if (g_test_rand_bit()) {
            /* the index is used, and our random bit has determined we will be
             * reallocating instead of freeing. Do so to some random size
             * between 0 and MAX_ALLOC_SIZE, then manually zero the
             * new memory */
            new_size = g_test_rand_int_range(0, MAX_ALLOC_SIZE);

            ptrs[ptrs_index] = (char *) wmem_realloc(allocator,
                    ptrs[ptrs_index], new_size);

            if (new_size)
                memset(ptrs[ptrs_index], 0, new_size);
        }
        else {
            /* the index is used, and our random bit has determined we will be
             * freeing instead of reallocating. Do so and NULL the pointer for
             * the next iteration. */
            wmem_free(allocator, ptrs[ptrs_index]);
            ptrs[ptrs_index] = NULL;
        }
        if (verify) (*verify)(allocator);
    }

    wmem_destroy_allocator(allocator);
}

static void
wmem_test_allocator_block(void)
{
    wmem_test_allocator(WMEM_ALLOCATOR_BLOCK, &wmem_block_verify,
            MAX_SIMULTANEOUS_ALLOCS*64);
    wmem_test_allocator_jumbo(WMEM_ALLOCATOR_BLOCK, &wmem_block_verify);
}

static void
wmem_test_allocator_block_fast(void)
{
    wmem_test_allocator(WMEM_ALLOCATOR_BLOCK_FAST, NULL,
            MAX_SIMULTANEOUS_ALLOCS*4);
    wmem_test_allocator_jumbo(WMEM_ALLOCATOR_BLOCK, NULL);
}

static void
wmem_test_allocator_simple(void)
{
    wmem_test_allocator(WMEM_ALLOCATOR_SIMPLE, NULL,
            MAX_SIMULTANEOUS_ALLOCS*64);
    wmem_test_allocator_jumbo(WMEM_ALLOCATOR_SIMPLE, NULL);
}

static void
wmem_test_allocator_strict(void)
{
    wmem_test_allocator(WMEM_ALLOCATOR_STRICT, &wmem_strict_check_canaries,
            MAX_SIMULTANEOUS_ALLOCS*64);
    wmem_test_allocator_jumbo(WMEM_ALLOCATOR_STRICT, &wmem_strict_check_canaries);
}

/* UTILITY TESTING FUNCTIONS (/wmem/utils/) */

static void
wmem_test_miscutls(void)
{
    wmem_allocator_t   *allocator;
    const char         *source = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char               *ret;

    allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);

    ret = (char*) wmem_memdup(allocator, NULL, 0);
    g_assert_true(ret == NULL);

    ret = (char*) wmem_memdup(allocator, source, 5);
    ret[4] = '\0';
    g_assert_cmpstr(ret, ==, "ABCD");

    ret = (char*) wmem_memdup(allocator, source, 1);
    g_assert_true(ret[0] == 'A');
    wmem_strict_check_canaries(allocator);

    ret = (char*) wmem_memdup(allocator, source, 10);
    ret[9] = '\0';
    g_assert_cmpstr(ret, ==, "ABCDEFGHI");

    wmem_destroy_allocator(allocator);
}

static void
wmem_test_strutls(void)
{
    wmem_allocator_t   *allocator;
    const char         *orig_str;
    char               *new_str;

    allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);

    orig_str = "TEST1";
    new_str  = wmem_strdup(allocator, orig_str);
    g_assert_cmpstr(new_str, ==, orig_str);
    new_str[0] = 'X';
    g_assert_cmpstr(new_str, >, orig_str);
    wmem_strict_check_canaries(allocator);

    orig_str = "TEST123456789";
    new_str  = wmem_strndup(allocator, orig_str, 6);
    g_assert_cmpstr(new_str, ==, "TEST12");
    g_assert_cmpstr(new_str, <, orig_str);
    new_str[0] = 'X';
    g_assert_cmpstr(new_str, >, orig_str);
    wmem_strict_check_canaries(allocator);

    new_str = wmem_strdup_printf(allocator, "abc %s %% %d", "boo", 23);
    g_assert_cmpstr(new_str, ==, "abc boo % 23");
    new_str = wmem_strdup_printf(allocator, "%s", STRING_80);
    g_assert_cmpstr(new_str, ==, STRING_80);
    wmem_strict_check_canaries(allocator);

    orig_str = "Short String";
    new_str = wmem_strdup_printf(allocator, "TEST %s", orig_str);
    g_assert_cmpstr(new_str, ==, "TEST Short String");

    orig_str = "Very Long..............................."
               "........................................"
               "........................................"
               "........................................"
               "........................................"
               "........................................"
               "..................................String";
    new_str = wmem_strdup_printf(allocator, "TEST %s", orig_str);
    g_assert_cmpstr(new_str, ==,
               "TEST Very Long..............................."
               "........................................"
               "........................................"
               "........................................"
               "........................................"
               "........................................"
               "..................................String");

    wmem_destroy_allocator(allocator);
}

#define RESOURCE_USAGE_START get_resource_usage(&start_utime, &start_stime)

#define RESOURCE_USAGE_END \
    get_resource_usage(&end_utime, &end_stime); \
    utime_ms = (end_utime - start_utime) * 1000.0; \
    stime_ms = (end_stime - start_stime) * 1000.0

/* NOTE: You have to run "wmem_test -m perf" to run the performance tests. */
static void
wmem_test_stringperf(void)
{
#define LOOP_COUNT (1 * 1000 * 1000)
    wmem_allocator_t   *allocator;
#ifdef _WIN32
    char                buffer[1];
#endif
    char               **str_ptr = g_new(char *, LOOP_COUNT);
    char               *s_val = "test string";
    double              d_val = 1000.2;
    unsigned            u_val = 54321;
    int                 i_val = -12345;
    int                 i;
    double              start_utime, start_stime, end_utime, end_stime, utime_ms, stime_ms;

    allocator = wmem_allocator_new(WMEM_ALLOCATOR_BLOCK);

/* C99 snprintf */

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        snprintf(NULL, 0, "%s", s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "snprintf 1 string: u %.3f ms s %.3f ms", utime_ms, stime_ms);

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        snprintf(NULL, 0, "%s%s%s%s%s", s_val, s_val, s_val, s_val, s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "snprintf 5 strings: u %.3f ms s %.3f ms", utime_ms, stime_ms);

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        snprintf(NULL, 0, "%s%u%3.5f%02d", s_val, u_val, d_val, i_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "snprintf mixed args: u %.3f ms s %.3f ms", utime_ms, stime_ms);

/* GLib g_snprintf (can use C99 or Gnulib) */

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        g_snprintf(NULL, 0, "%s", s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "g_printf_string_upper_bound (via g_snprintf) 1 string: u %.3f ms s %.3f ms", utime_ms, stime_ms);

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        g_snprintf(NULL, 0, "%s%s%s%s%s", s_val, s_val, s_val, s_val, s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "g_printf_string_upper_bound (via g_snprintf) 5 strings: u %.3f ms s %.3f ms", utime_ms, stime_ms);

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        g_snprintf(NULL, 0, "%s%u%3.5f%02d", s_val, u_val, d_val, i_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "g_printf_string_upper_bound (via g_snprintf) mixed args: u %.3f ms s %.3f ms", utime_ms, stime_ms);

/* Windows _snprintf_s */

#ifdef _WIN32
    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        _snprintf_s(buffer, 1, _TRUNCATE, "%s", s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "_snprintf_s upper bound 1 string: u %.3f ms s %.3f ms", utime_ms, stime_ms);

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        _snprintf_s(buffer, 1, _TRUNCATE, "%s%s%s%s%s", s_val, s_val, s_val, s_val, s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "_snprintf_s upper bound 5 strings: u %.3f ms s %.3f ms", utime_ms, stime_ms);

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        _snprintf_s(buffer, 1, _TRUNCATE, "%s%u%3.5f%02d", s_val, u_val, d_val, i_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "_snprintf_s upper bound mixed args: u %.3f ms s %.3f ms", utime_ms, stime_ms);
#endif

/* GLib strdup */

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        str_ptr[i] = g_strdup_printf("%s%s", s_val, s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "g_strdup_printf 2 strings: u %.3f ms s %.3f ms", utime_ms, stime_ms);
    for (i = 0; i < LOOP_COUNT; i++) {
        g_free(str_ptr[i]);
    }

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        str_ptr[i] = g_strdup_printf("%s%s%s%s%s", s_val, s_val, s_val, s_val, s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "g_strdup_printf 5 strings: u %.3f ms s %.3f ms", utime_ms, stime_ms);
    for (i = 0; i < LOOP_COUNT; i++) {
        g_free(str_ptr[i]);
    }

/* wmem strdup null allocator */

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        str_ptr[i] = wmem_strdup_printf(NULL, "%s%s", s_val, s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "wmem_strdup_printf() 2 strings: u %.3f ms s %.3f ms", utime_ms, stime_ms);
    for (i = 0; i < LOOP_COUNT; i++) {
        g_free(str_ptr[i]);
    }

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        str_ptr[i] = wmem_strdup_printf(NULL, "%s%s%s%s%s", s_val, s_val, s_val, s_val, s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "wmem_strdup_printf(NULL) 5 strings: u %.3f ms s %.3f ms", utime_ms, stime_ms);
    for (i = 0; i < LOOP_COUNT; i++) {
        g_free(str_ptr[i]);
    }

/* wmem strdup strict allocator */

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        wmem_strdup_printf(allocator, "%s%s", s_val, s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "wmem_strdup_printf(allocator) 2 strings: u %.3f ms s %.3f ms", utime_ms, stime_ms);

    RESOURCE_USAGE_START;
    for (i = 0; i < LOOP_COUNT; i++) {
        wmem_strdup_printf(allocator, "%s%s%s%s%s", s_val, s_val, s_val, s_val, s_val);
    }
    RESOURCE_USAGE_END;
    g_test_minimized_result(utime_ms + stime_ms,
        "wmem_strdup_printf(allocator) 5 strings: u %.3f ms s %.3f ms", utime_ms, stime_ms);

    wmem_destroy_allocator(allocator);
    g_free(str_ptr);
}

/* DATA STRUCTURE TESTING FUNCTIONS (/wmem/datastruct/) */

static void
wmem_test_array(void)
{
    wmem_allocator_t   *allocator;
    wmem_array_t       *array;
    unsigned int        i, j, k;
    uint32_t            val, *buf;
    uint32_t            vals[8];
    uint32_t           *raw;
    uint32_t            lastint;

    allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);

    array = wmem_array_new(allocator, sizeof(uint32_t));
    g_assert_true(array);
    g_assert_true(wmem_array_get_count(array) == 0);

    for (i=0; i<CONTAINER_ITERS; i++) {
        val = i;
        wmem_array_append_one(array, val);
        g_assert_true(wmem_array_get_count(array) == i+1);

        val = *(uint32_t*)wmem_array_index(array, i);
        g_assert_true(val == i);
        g_assert_true(wmem_array_try_index(array, i, &val) == 0);
        g_assert_true(val == i);
        g_assert_true(wmem_array_try_index(array, i+1, &val) < 0);

    }
    wmem_strict_check_canaries(allocator);

    for (i=0; i<CONTAINER_ITERS; i++) {
        val = *(uint32_t*)wmem_array_index(array, i);
        g_assert_true(val == i);
        g_assert_true(wmem_array_try_index(array, i, &val) == 0);
        g_assert_true(val == i);
    }

    wmem_destroy_array(array);

    array = wmem_array_sized_new(allocator, sizeof(uint32_t), 73);
    wmem_array_set_null_terminator(array);
    for (i=0; i<75; i++)
        g_assert_true(wmem_array_try_index(array, i, &val) < 0);

    for (i=0; i<CONTAINER_ITERS; i++) {
        for (j=0; j<8; j++) {
            vals[j] = i+j;
        }

        wmem_array_append(array, vals, 8);
        g_assert_true(wmem_array_get_count(array) == 8*(i+1));
    }
    wmem_strict_check_canaries(allocator);

    buf = (uint32_t*)wmem_array_get_raw(array);
    for (i=0; i<CONTAINER_ITERS; i++) {
        for (j=0; j<8; j++) {
            g_assert_true(buf[i*8 + j] == i+j);
        }
    }

    wmem_array_sort(array, wmem_test_compare_uint32);
    for (i=0, k=0; i<8; i++) {
        for (j=0; j<=i; j++, k++) {
            val = *(uint32_t*)wmem_array_index(array, k);
            g_assert_true(val == i);
            g_assert_true(wmem_array_try_index(array, k, &val) == 0);
            g_assert_true(val == i);
        }
    }
    for (j=k; k<8*(CONTAINER_ITERS+1)-j; k++) {
            val = *(uint32_t*)wmem_array_index(array, k);
            g_assert_true(val == ((k-j)/8)+8);
            g_assert_true(wmem_array_try_index(array, k, &val) == 0);
            g_assert_true(val == ((k-j)/8)+8);
    }
    for (i=0; i<7; i++) {
        for (j=0; j<7-i; j++, k++) {
            val = *(uint32_t*)wmem_array_index(array, k);
            g_assert_true(val == CONTAINER_ITERS+i);
            g_assert_true(wmem_array_try_index(array, k, &val) == 0);
            g_assert_true(val == CONTAINER_ITERS+i);
        }
    }
    g_assert_true(k == wmem_array_get_count(array));

    lastint = 77;
    wmem_array_append_one(array, lastint);

    raw = (uint32_t*)wmem_array_get_raw(array);
    g_assert_true(raw[wmem_array_get_count(array)] == 0);
    g_assert_true(raw[wmem_array_get_count(array) - 1] == lastint);

    wmem_destroy_array(array);

    wmem_destroy_allocator(allocator);
}

static void
check_val_list(void * val, void * val_to_check)
{
    g_assert_true(val == val_to_check);
}

static int
str_compare(const void *a, const void *b)
{
    return strcmp((const char*)a, (const char*)b);
}

static void
wmem_test_list(void)
{
    wmem_allocator_t  *allocator;
    wmem_list_t       *list;
    wmem_list_frame_t *frame;
    unsigned int       i;
    int                int1;
    int                int2;
    char*              str1;
    char*              str2;

    allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);

    list = wmem_list_new(allocator);
    g_assert_true(list);
    g_assert_true(wmem_list_count(list) == 0);

    frame = wmem_list_head(list);
    g_assert_true(frame == NULL);

    for (i=0; i<CONTAINER_ITERS; i++) {
        wmem_list_prepend(list, GINT_TO_POINTER(i));
        g_assert_true(wmem_list_count(list) == i+1);
        g_assert_true(wmem_list_find(list, GINT_TO_POINTER(i)));

        frame = wmem_list_head(list);
        g_assert_true(frame);
        g_assert_true(wmem_list_frame_data(frame) == GINT_TO_POINTER(i));
    }
    wmem_strict_check_canaries(allocator);

    i = CONTAINER_ITERS - 1;
    frame = wmem_list_head(list);
    while (frame) {
        g_assert_true(wmem_list_frame_data(frame) == GINT_TO_POINTER(i));
        i--;
        frame = wmem_list_frame_next(frame);
    }

    i = 0;
    frame = wmem_list_tail(list);
    while (frame) {
        g_assert_true(wmem_list_frame_data(frame) == GINT_TO_POINTER(i));
        i++;
        frame = wmem_list_frame_prev(frame);
    }

    i = CONTAINER_ITERS - 2;
    while (wmem_list_count(list) > 1) {
        wmem_list_remove(list, GINT_TO_POINTER(i));
        i--;
    }
    wmem_list_remove(list, GINT_TO_POINTER(CONTAINER_ITERS - 1));
    g_assert_true(wmem_list_count(list) == 0);
    g_assert_true(wmem_list_head(list) == NULL);
    g_assert_true(wmem_list_tail(list) == NULL);

    for (i=0; i<CONTAINER_ITERS; i++) {
        wmem_list_append(list, GINT_TO_POINTER(i));
        g_assert_true(wmem_list_count(list) == i+1);

        frame = wmem_list_head(list);
        g_assert_true(frame);
    }
    wmem_strict_check_canaries(allocator);

    i = 0;
    frame = wmem_list_head(list);
    while (frame) {
        g_assert_true(wmem_list_frame_data(frame) == GINT_TO_POINTER(i));
        i++;
        frame = wmem_list_frame_next(frame);
    }

    i = CONTAINER_ITERS - 1;
    frame = wmem_list_tail(list);
    while (frame) {
        g_assert_true(wmem_list_frame_data(frame) == GINT_TO_POINTER(i));
        i--;
        frame = wmem_list_frame_prev(frame);
    }

    wmem_destroy_allocator(allocator);

    list = wmem_list_new(NULL);
    for (i=0; i<CONTAINER_ITERS; i++) {
        wmem_list_prepend(list, GINT_TO_POINTER(i));
    }
    g_assert_true(wmem_list_count(list) == CONTAINER_ITERS);
    wmem_destroy_list(list);

    list = wmem_list_new(NULL);
    for (i=0; i<CONTAINER_ITERS; i++) {
        wmem_list_append(list, GINT_TO_POINTER(1));
    }
    wmem_list_foreach(list, check_val_list, GINT_TO_POINTER(1));
    wmem_destroy_list(list);

    list = wmem_list_new(NULL);
    wmem_list_insert_sorted(list, GINT_TO_POINTER(5), wmem_compare_int);
    wmem_list_insert_sorted(list, GINT_TO_POINTER(8), wmem_compare_int);
    wmem_list_insert_sorted(list, GINT_TO_POINTER(1), wmem_compare_int);
    wmem_list_insert_sorted(list, GINT_TO_POINTER(2), wmem_compare_int);
    wmem_list_insert_sorted(list, GINT_TO_POINTER(9), wmem_compare_int);
    g_assert_true(wmem_list_count(list) == 5);
    frame = wmem_list_head(list);
    int1 = GPOINTER_TO_INT(wmem_list_frame_data(frame));
    while ((frame = wmem_list_frame_next(frame))) {
        int2 = GPOINTER_TO_INT(wmem_list_frame_data(frame));
        g_assert_true(int1 <= int2);
        int1 = int2;
    }
    wmem_destroy_list(list);

    list = wmem_list_new(NULL);
    wmem_list_insert_sorted(list, GINT_TO_POINTER(5), wmem_compare_int);
    wmem_list_insert_sorted(list, GINT_TO_POINTER(1), wmem_compare_int);
    wmem_list_insert_sorted(list, GINT_TO_POINTER(7), wmem_compare_int);
    wmem_list_insert_sorted(list, GINT_TO_POINTER(3), wmem_compare_int);
    wmem_list_insert_sorted(list, GINT_TO_POINTER(2), wmem_compare_int);
    wmem_list_insert_sorted(list, GINT_TO_POINTER(2), wmem_compare_int);
    g_assert_true(wmem_list_count(list) == 6);
    frame = wmem_list_head(list);
    int1 = GPOINTER_TO_INT(wmem_list_frame_data(frame));
    while ((frame = wmem_list_frame_next(frame))) {
        int2 = GPOINTER_TO_INT(wmem_list_frame_data(frame));
        g_assert_true(int1 <= int2);
        int1 = int2;
    }
    wmem_destroy_list(list);

    list = wmem_list_new(NULL);
    wmem_list_insert_sorted(list, "abc", str_compare);
    wmem_list_insert_sorted(list, "bcd", str_compare);
    wmem_list_insert_sorted(list, "aaa", str_compare);
    wmem_list_insert_sorted(list, "bbb", str_compare);
    wmem_list_insert_sorted(list, "zzz", str_compare);
    wmem_list_insert_sorted(list, "ggg", str_compare);
    g_assert_true(wmem_list_count(list) == 6);
    frame = wmem_list_head(list);
    str1 = (char*)wmem_list_frame_data(frame);
    while ((frame = wmem_list_frame_next(frame))) {
        str2 = (char*)wmem_list_frame_data(frame);
        g_assert_true(strcmp(str1, str2) <= 0);
        str1 = str2;
    }
    wmem_destroy_list(list);
}

static void
check_val_map(void * key _U_, void * val, void * user_data)
{
    g_assert_true(val == user_data);
}

static gboolean
equal_val_map(void * key _U_, void * val, void * user_data)
{
    return val == user_data;
}

static void
wmem_test_map(void)
{
    wmem_allocator_t   *allocator, *extra_allocator;
    wmem_map_t       *map;
    char             *str_key;
    const void       *str_key_ret;
    unsigned int      i;
    unsigned int     *key_ret;
    unsigned int     *value_ret;
    void             *ret;

    allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);
    extra_allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);

    /* insertion, lookup and removal of simple integer keys */
    map = wmem_map_new(allocator, g_direct_hash, g_direct_equal);
    g_assert_true(map);

    for (i=0; i<CONTAINER_ITERS; i++) {
        ret = wmem_map_insert(map, GINT_TO_POINTER(i), GINT_TO_POINTER(777777));
        g_assert_true(ret == NULL);
        ret = wmem_map_insert(map, GINT_TO_POINTER(i), GINT_TO_POINTER(i));
        g_assert_true(ret == GINT_TO_POINTER(777777));
        ret = wmem_map_insert(map, GINT_TO_POINTER(i), GINT_TO_POINTER(i));
        g_assert_true(ret == GINT_TO_POINTER(i));
    }
    for (i=0; i<CONTAINER_ITERS; i++) {
        ret = wmem_map_lookup(map, GINT_TO_POINTER(i));
        g_assert_true(ret == GINT_TO_POINTER(i));
        g_assert_true(wmem_map_contains(map, GINT_TO_POINTER(i)) == true);
        g_assert_true(wmem_map_lookup_extended(map, GINT_TO_POINTER(i), NULL, NULL));
        key_ret = NULL;
        g_assert_true(wmem_map_lookup_extended(map, GINT_TO_POINTER(i), GINT_TO_POINTER(&key_ret), NULL));
        g_assert_true(key_ret == GINT_TO_POINTER(i));
        value_ret = NULL;
        g_assert_true(wmem_map_lookup_extended(map, GINT_TO_POINTER(i), NULL, GINT_TO_POINTER(&value_ret)));
        g_assert_true(value_ret == GINT_TO_POINTER(i));
        key_ret = NULL;
        value_ret = NULL;
        g_assert_true(wmem_map_lookup_extended(map, GINT_TO_POINTER(i), GINT_TO_POINTER(&key_ret), GINT_TO_POINTER(&value_ret)));
        g_assert_true(key_ret == GINT_TO_POINTER(i));
        g_assert_true(value_ret == GINT_TO_POINTER(i));
        ret = wmem_map_remove(map, GINT_TO_POINTER(i));
        g_assert_true(ret == GINT_TO_POINTER(i));
        g_assert_true(wmem_map_contains(map, GINT_TO_POINTER(i)) == false);
        ret = wmem_map_lookup(map, GINT_TO_POINTER(i));
        g_assert_true(ret == NULL);
        ret = wmem_map_remove(map, GINT_TO_POINTER(i));
        g_assert_true(ret == NULL);
    }
    wmem_free_all(allocator);

    /* test auto-reset functionality */
    map = wmem_map_new_autoreset(allocator, extra_allocator, g_direct_hash, g_direct_equal);
    g_assert_true(map);
    for (i=0; i<CONTAINER_ITERS; i++) {
        ret = wmem_map_insert(map, GINT_TO_POINTER(i), GINT_TO_POINTER(777777));
        g_assert_true(ret == NULL);
        ret = wmem_map_insert(map, GINT_TO_POINTER(i), GINT_TO_POINTER(i));
        g_assert_true(ret == GINT_TO_POINTER(777777));
        ret = wmem_map_insert(map, GINT_TO_POINTER(i), GINT_TO_POINTER(i));
        g_assert_true(ret == GINT_TO_POINTER(i));
    }
    wmem_free_all(extra_allocator);
    for (i=0; i<CONTAINER_ITERS; i++) {
        g_assert_true(wmem_map_lookup(map, GINT_TO_POINTER(i)) == NULL);
    }
    wmem_free_all(allocator);

    map = wmem_map_new(allocator, wmem_str_hash, g_str_equal);
    g_assert_true(map);

    /* string keys and for-each */
    for (i=0; i<CONTAINER_ITERS; i++) {
        str_key = wmem_test_rand_string(allocator, 1, 64);
        wmem_map_insert(map, str_key, GINT_TO_POINTER(i));
        ret = wmem_map_lookup(map, str_key);
        g_assert_true(ret == GINT_TO_POINTER(i));
        g_assert_true(wmem_map_contains(map, str_key) == true);
        str_key_ret = NULL;
        value_ret = NULL;
        g_assert_true(wmem_map_lookup_extended(map, str_key, &str_key_ret, GINT_TO_POINTER(&value_ret)) == true);
        g_assert_true(g_str_equal(str_key_ret, str_key));
        g_assert_true(value_ret == GINT_TO_POINTER(i));
    }

    /* test foreach */
    map = wmem_map_new(allocator, wmem_str_hash, g_str_equal);
    g_assert_true(map);
    for (i=0; i<CONTAINER_ITERS; i++) {
        str_key = wmem_test_rand_string(allocator, 1, 64);
        wmem_map_insert(map, str_key, GINT_TO_POINTER(2));
    }
    wmem_map_foreach(map, check_val_map, GINT_TO_POINTER(2));
    g_assert_true(wmem_map_find(map, equal_val_map, GINT_TO_POINTER(2)) == GINT_TO_POINTER(2));
    wmem_map_foreach_remove(map, equal_val_map, GINT_TO_POINTER(2));
    g_assert_true(wmem_map_size(map) == 0);

    /* test size */
    map = wmem_map_new(allocator, g_direct_hash, g_direct_equal);
    g_assert_true(map);
    for (i=0; i<CONTAINER_ITERS; i++) {
        wmem_map_insert(map, GINT_TO_POINTER(i), GINT_TO_POINTER(i));
    }
    g_assert_true(wmem_map_size(map) == CONTAINER_ITERS);

    for (i=0; i<CONTAINER_ITERS; i+=2) {
        wmem_map_foreach_remove(map, equal_val_map, GINT_TO_POINTER(i));
    }
    g_assert_true(wmem_map_size(map) == CONTAINER_ITERS/2);

    wmem_destroy_allocator(extra_allocator);
    wmem_destroy_allocator(allocator);
}

static void
wmem_test_queue(void)
{
    wmem_allocator_t   *allocator;
    wmem_queue_t       *queue;
    unsigned int        i;

    allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);

    queue = wmem_queue_new(allocator);
    g_assert_true(queue);
    g_assert_true(wmem_queue_count(queue) == 0);

    for (i=0; i<CONTAINER_ITERS; i++) {
        wmem_queue_push(queue, GINT_TO_POINTER(i));

        g_assert_true(wmem_queue_count(queue) == i+1);
        g_assert_true(wmem_queue_peek(queue) == GINT_TO_POINTER(0));
    }
    wmem_strict_check_canaries(allocator);

    for (i=0; i<CONTAINER_ITERS; i++) {
        g_assert_true(wmem_queue_peek(queue) == GINT_TO_POINTER(i));
        g_assert_true(wmem_queue_pop(queue) == GINT_TO_POINTER(i));
        g_assert_true(wmem_queue_count(queue) == CONTAINER_ITERS-i-1);
    }
    g_assert_true(wmem_queue_count(queue) == 0);

    wmem_destroy_queue(queue);

    wmem_destroy_allocator(allocator);
}

static void
wmem_test_stack(void)
{
    wmem_allocator_t   *allocator;
    wmem_stack_t       *stack;
    unsigned int        i;

    allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);

    stack = wmem_stack_new(allocator);
    g_assert_true(stack);
    g_assert_true(wmem_stack_count(stack) == 0);

    for (i=0; i<CONTAINER_ITERS; i++) {
        wmem_stack_push(stack, GINT_TO_POINTER(i));

        g_assert_true(wmem_stack_count(stack) == i+1);
        g_assert_true(wmem_stack_peek(stack) == GINT_TO_POINTER(i));
    }
    wmem_strict_check_canaries(allocator);

    for (i=CONTAINER_ITERS; i>0; i--) {
        g_assert_true(wmem_stack_peek(stack) == GINT_TO_POINTER(i-1));
        g_assert_true(wmem_stack_pop(stack) == GINT_TO_POINTER(i-1));
        g_assert_true(wmem_stack_count(stack) == i-1);
    }
    g_assert_true(wmem_stack_count(stack) == 0);

    wmem_destroy_stack(stack);

    wmem_destroy_allocator(allocator);
}

static void
wmem_test_strbuf(void)
{
    wmem_allocator_t   *allocator;
    wmem_strbuf_t      *strbuf;
    int                 i;

    allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);

    strbuf = wmem_strbuf_new(allocator, "TEST");
    g_assert_true(strbuf);
    g_assert_cmpstr(wmem_strbuf_get_str(strbuf), ==, "TEST");
    g_assert_cmpuint(wmem_strbuf_get_len(strbuf), ==, 4);

    wmem_strbuf_append(strbuf, "FUZZ");
    g_assert_cmpstr(wmem_strbuf_get_str(strbuf), ==, "TESTFUZZ");
    g_assert_cmpuint(wmem_strbuf_get_len(strbuf), ==, 8);

    wmem_strbuf_append_printf(strbuf, "%d%s", 3, "a");
    g_assert_cmpstr(wmem_strbuf_get_str(strbuf), ==, "TESTFUZZ3a");
    g_assert_cmpuint(wmem_strbuf_get_len(strbuf), ==, 10);

    wmem_strbuf_append_c(strbuf, 'q');
    g_assert_cmpstr(wmem_strbuf_get_str(strbuf), ==, "TESTFUZZ3aq");
    g_assert_cmpuint(wmem_strbuf_get_len(strbuf), ==, 11);

    wmem_strbuf_append_unichar(strbuf, g_utf8_get_char("\xC2\xA9"));
    g_assert_cmpstr(wmem_strbuf_get_str(strbuf), ==, "TESTFUZZ3aq\xC2\xA9");
    g_assert_cmpuint(wmem_strbuf_get_len(strbuf), ==, 13);

    wmem_strbuf_append_c_count(strbuf, '+', 8);
    g_assert_cmpstr(wmem_strbuf_get_str(strbuf), ==, "TESTFUZZ3aq\xC2\xA9++++++++");
    g_assert_cmpuint(wmem_strbuf_get_len(strbuf), ==, 21);

    wmem_strbuf_truncate(strbuf, 32);
    wmem_strbuf_truncate(strbuf, 24);
    wmem_strbuf_truncate(strbuf, 16);
    wmem_strbuf_truncate(strbuf, 13);
    g_assert_cmpstr(wmem_strbuf_get_str(strbuf), ==, "TESTFUZZ3aq\xC2\xA9");
    g_assert_cmpuint(wmem_strbuf_get_len(strbuf), ==, 13);

    wmem_strbuf_truncate(strbuf, 3);
    g_assert_cmpstr(wmem_strbuf_get_str(strbuf), ==, "TES");
    g_assert_cmpuint(wmem_strbuf_get_len(strbuf), ==, 3);

    wmem_strbuf_append_len(strbuf, "TFUZZ1234", 5);
    g_assert_cmpstr(wmem_strbuf_get_str(strbuf), ==, "TESTFUZZ");
    g_assert_cmpuint(wmem_strbuf_get_len(strbuf), ==, 8);

    wmem_free_all(allocator);

    strbuf = wmem_strbuf_new(allocator, "TEST");
    for (i=0; i<1024; i++) {
        if (g_test_rand_bit()) {
            wmem_strbuf_append(strbuf, "ABC");
        }
        else {
            wmem_strbuf_append_printf(strbuf, "%d%d", 3, 777);
        }
        wmem_strict_check_canaries(allocator);
    }
    g_assert_true(strlen(wmem_strbuf_get_str(strbuf)) ==
             wmem_strbuf_get_len(strbuf));

    wmem_destroy_allocator(allocator);
}

static void
wmem_test_strbuf_validate(void)
{
    wmem_strbuf_t *strbuf;
    const char *endptr;

    strbuf = wmem_strbuf_new(NULL, "TEST\xEF ABC");
    g_assert_false(wmem_strbuf_utf8_validate(strbuf, &endptr));
    g_assert_true(endptr == &strbuf->str[4]);
    wmem_strbuf_destroy(strbuf);

    strbuf = wmem_strbuf_new(NULL, NULL);
    wmem_strbuf_append_len(strbuf, "TEST\x00\x00 ABC", 10);
    g_assert_true(wmem_strbuf_utf8_validate(strbuf, &endptr));
    wmem_strbuf_destroy(strbuf);

    strbuf = wmem_strbuf_new(NULL, NULL);
    wmem_strbuf_append_len(strbuf, "TEST\x00\xEF ABC", 10);
    g_assert_false(wmem_strbuf_utf8_validate(strbuf, &endptr));
    g_assert_true(endptr == &strbuf->str[5]);
    wmem_strbuf_destroy(strbuf);

    strbuf = wmem_strbuf_new(NULL, NULL);
    wmem_strbuf_append_len(strbuf, "TEST\x00 ABC \x00 DEF \x00", 17);
    g_assert_true(wmem_strbuf_utf8_validate(strbuf, &endptr));
    wmem_strbuf_destroy(strbuf);
}

static void
wmem_test_tree(void)
{
    wmem_allocator_t   *allocator, *extra_allocator;
    wmem_tree_t        *tree;
    uint32_t            i;
    uint32_t            rand_int;
    int                 seen_values = 0;
    int                 j;
    uint32_t            int_key;
    char               *str_key;
#define WMEM_TREE_MAX_KEY_COUNT 8
#define WMEM_TREE_MAX_KEY_LEN   4
    int                 key_count;
    wmem_tree_key_t     keys[WMEM_TREE_MAX_KEY_COUNT];

    allocator       = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);
    extra_allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);

    tree = wmem_tree_new(allocator);
    g_assert_true(tree);
    g_assert_true(wmem_tree_is_empty(tree));

    /* test basic 32-bit key operations */
    for (i=0; i<CONTAINER_ITERS; i++) {
        g_assert_true(wmem_tree_lookup32(tree, i) == NULL);
        if (i > 0) {
            g_assert_true(wmem_tree_lookup32_le_full(tree, i, &int_key) == GINT_TO_POINTER(i-1));
            g_assert_true(int_key == i - 1);
        }
        wmem_tree_insert32(tree, i, GINT_TO_POINTER(i));
        g_assert_true(wmem_tree_lookup32(tree, i) == GINT_TO_POINTER(i));
        g_assert_true(!wmem_tree_is_empty(tree));
    }
    g_assert_true(wmem_tree_count(tree) == CONTAINER_ITERS);

    rand_int = ((uint32_t)g_test_rand_int()) % CONTAINER_ITERS;
    wmem_tree_remove32(tree, rand_int);
    g_assert_true(wmem_tree_lookup32(tree, rand_int) == NULL);
    if (rand_int > 0) {
        g_assert_true(wmem_tree_lookup32_le(tree, rand_int) == GINT_TO_POINTER(rand_int - 1));
    }
    if (rand_int + 1 < CONTAINER_ITERS) {
        g_assert_true(wmem_tree_lookup32_ge(tree, rand_int) == GINT_TO_POINTER(rand_int + 1));
    }
    g_assert_true(wmem_tree_count(tree) == CONTAINER_ITERS - 1);
    wmem_free_all(allocator);

    tree = wmem_tree_new(allocator);
    for (i=0; i<CONTAINER_ITERS; i++) {
        do {
            rand_int = g_test_rand_int();
        } while (wmem_tree_lookup32(tree, rand_int));
        wmem_tree_insert32(tree, rand_int, GINT_TO_POINTER(i));
        g_assert_true(wmem_tree_lookup32(tree, rand_int) == GINT_TO_POINTER(i));
    }
    g_assert_true(wmem_tree_count(tree) == CONTAINER_ITERS);
    wmem_free_all(allocator);

    /* test auto-reset functionality */
    tree = wmem_tree_new_autoreset(allocator, extra_allocator);
    for (i=0; i<CONTAINER_ITERS; i++) {
        g_assert_true(wmem_tree_lookup32(tree, i) == NULL);
        wmem_tree_insert32(tree, i, GINT_TO_POINTER(i));
        g_assert_true(wmem_tree_lookup32(tree, i) == GINT_TO_POINTER(i));
    }
    g_assert_true(wmem_tree_count(tree) == CONTAINER_ITERS);
    wmem_free_all(extra_allocator);
    g_assert_true(wmem_tree_count(tree) == 0);
    for (i=0; i<CONTAINER_ITERS; i++) {
        g_assert_true(wmem_tree_lookup32(tree, i) == NULL);
        g_assert_true(wmem_tree_lookup32_le(tree, i) == NULL);
    }
    wmem_free_all(allocator);

    /* test array key functionality */
    tree = wmem_tree_new(allocator);
    key_count = g_random_int_range(1, WMEM_TREE_MAX_KEY_COUNT);
    for (j=0; j<key_count; j++) {
        keys[j].length = g_random_int_range(1, WMEM_TREE_MAX_KEY_LEN);
    }
    keys[key_count].length = 0;
    for (i=0; i<CONTAINER_ITERS; i++) {
        for (j=0; j<key_count; j++) {
            keys[j].key    = (uint32_t*)wmem_test_rand_string(allocator,
                    (keys[j].length*4), (keys[j].length*4)+1);
        }
        wmem_tree_insert32_array(tree, keys, GINT_TO_POINTER(i));
        g_assert_true(wmem_tree_lookup32_array(tree, keys) == GINT_TO_POINTER(i));
    }
    wmem_free_all(allocator);

    tree = wmem_tree_new(allocator);
    keys[0].length = 1;
    keys[0].key    = wmem_new(allocator, uint32_t);
    *(keys[0].key) = 0;
    keys[1].length = 0;
    for (i=0; i<CONTAINER_ITERS; i++) {
        wmem_tree_insert32_array(tree, keys, GINT_TO_POINTER(i));
        *(keys[0].key) += 4;
    }
    *(keys[0].key) = 0;
    for (i=0; i<CONTAINER_ITERS; i++) {
        g_assert_true(wmem_tree_lookup32_array(tree, keys) == GINT_TO_POINTER(i));
        for (j=0; j<3; j++) {
            (*(keys[0].key)) += 1;
            g_assert_true(wmem_tree_lookup32_array_le(tree, keys) ==
                    GINT_TO_POINTER(i));
        }
        *(keys[0].key) += 1;
    }
    wmem_free_all(allocator);

    /* test string key functionality */
    tree = wmem_tree_new(allocator);
    for (i=0; i<CONTAINER_ITERS; i++) {
        str_key = wmem_test_rand_string(allocator, 1, 64);
        wmem_tree_insert_string(tree, str_key, GINT_TO_POINTER(i), 0);
        g_assert_true(wmem_tree_lookup_string(tree, str_key, 0) ==
                GINT_TO_POINTER(i));
    }
    wmem_free_all(allocator);

    tree = wmem_tree_new(allocator);
    for (i=0; i<CONTAINER_ITERS; i++) {
        str_key = wmem_test_rand_string(allocator, 1, 64);
        wmem_tree_insert_string(tree, str_key, GINT_TO_POINTER(i),
                WMEM_TREE_STRING_NOCASE);
        g_assert_true(wmem_tree_lookup_string(tree, str_key,
                    WMEM_TREE_STRING_NOCASE) == GINT_TO_POINTER(i));
    }
    wmem_free_all(allocator);

    /* test for-each functionality */
    tree = wmem_tree_new(allocator);
    expected_user_data = GINT_TO_POINTER(g_test_rand_int());
    for (i=0; i<CONTAINER_ITERS; i++) {
        int tmp;
        do {
            tmp = g_test_rand_int();
        } while (wmem_tree_lookup32(tree, tmp));
        value_seen[i] = false;
        wmem_tree_insert32(tree, tmp, GINT_TO_POINTER(i));
    }

    cb_called_count    = 0;
    cb_continue_count  = CONTAINER_ITERS;
    wmem_tree_foreach(tree, wmem_test_foreach_cb, expected_user_data);
    g_assert_true(cb_called_count   == CONTAINER_ITERS);
    g_assert_true(cb_continue_count == 0);

    for (i=0; i<CONTAINER_ITERS; i++) {
        g_assert_true(value_seen[i]);
        value_seen[i] = false;
    }

    cb_called_count    = 0;
    cb_continue_count  = 10;
    wmem_tree_foreach(tree, wmem_test_foreach_cb, expected_user_data);
    g_assert_true(cb_called_count   == 10);
    g_assert_true(cb_continue_count == 0);

    for (i=0; i<CONTAINER_ITERS; i++) {
        if (value_seen[i]) {
            seen_values++;
        }
    }
    g_assert_true(seen_values == 10);

    wmem_destroy_allocator(extra_allocator);
    wmem_destroy_allocator(allocator);
}


/* to be used as userdata in the callback wmem_test_itree_check_overlap_cb*/
typedef struct wmem_test_itree_user_data {
    wmem_range_t range;
    unsigned counter;
} wmem_test_itree_user_data_t;


/* increase userData counter in case the range match the userdata range */
static bool
wmem_test_itree_check_overlap_cb (const void *key, void *value _U_, void *userData)
{
    const wmem_range_t *ckey = (const wmem_range_t *)key;
    struct wmem_test_itree_user_data * d = (struct wmem_test_itree_user_data *)userData;
    g_assert_true(key);
    g_assert_true(d);

    if(wmem_itree_range_overlap(ckey, &d->range)) {
        d->counter++;
    }

    return false;
}


static bool
wmem_test_overlap(uint64_t low, uint64_t high, uint64_t lowbis, uint64_t highbis)
{
    wmem_range_t r1 = {low, high, 0};
    wmem_range_t r2 = {lowbis, highbis, 0};
    return wmem_itree_range_overlap(&r1, &r2);
}

static void
wmem_test_itree(void)
{
    wmem_allocator_t   *allocator, *extra_allocator;
    wmem_itree_t       *tree;
    int i = 0;
    int32_t max_rand = 0;
    wmem_test_itree_user_data_t userData;
    wmem_range_t range, r2;

    allocator       = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);
    extra_allocator = wmem_allocator_new(WMEM_ALLOCATOR_STRICT);

    tree = wmem_itree_new(allocator);
    g_assert_true(tree);
    g_assert_true(wmem_itree_is_empty(tree));

    wmem_free_all(allocator);

    /* make sure that wmem_test_overlap is correct (well it's no proof but...)*/
    g_assert_true(wmem_test_overlap(0, 10, 0, 4));
    g_assert_true(wmem_test_overlap(0, 10, 9, 14));
    g_assert_true(wmem_test_overlap(5, 10, 3, 8));
    g_assert_true(wmem_test_overlap(5, 10, 1, 12));
    g_assert_true(!wmem_test_overlap(0, 10, 11, 12));

    /* Generate a reference range, then fill an itree with random ranges,
    then we count greedily the number of overlapping ranges and compare
    the result with the optimized result
     */

    userData.counter = 0;

    tree = wmem_itree_new(allocator);

    /* even though keys are uint64_t, we use INT32_MAX as a max because of the type returned by
      g_test_rand_int_range.
     */
    max_rand = INT32_MAX;
    r2.max_edge = range.max_edge = 0;
    range.low = g_test_rand_int_range(0, max_rand);
    range.high = g_test_rand_int_range( (int32_t)range.low, (int32_t)max_rand);
    userData.range = range;

    for (i=0; i<CONTAINER_ITERS; i++) {

        wmem_list_t *results = NULL;

        /* reset the search */
        userData.counter = 0;
        r2.low = (uint64_t)g_test_rand_int_range(0, 100);
        r2.high = (uint64_t)g_test_rand_int_range( (int32_t)r2.low, 100);

        wmem_itree_insert(tree, r2.low, r2.high, GINT_TO_POINTER(i));

        /* greedy search */
        wmem_tree_foreach(tree, wmem_test_itree_check_overlap_cb, &userData);

        /* Optimized search */
        results = wmem_itree_find_intervals(tree, allocator, range.low, range.high);

        /* keep it as a loop instead of wmem_list_count in case one */
        g_assert_true(wmem_list_count(results) == userData.counter);
    }

    wmem_destroy_allocator(extra_allocator);
    wmem_destroy_allocator(allocator);
}


int
main(int argc, char **argv)
{
    int ret;

    wmem_init();

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/wmem/allocator/block",     wmem_test_allocator_block);
    g_test_add_func("/wmem/allocator/blk_fast",  wmem_test_allocator_block_fast);
    g_test_add_func("/wmem/allocator/simple",    wmem_test_allocator_simple);
    g_test_add_func("/wmem/allocator/strict",    wmem_test_allocator_strict);
    g_test_add_func("/wmem/allocator/callbacks", wmem_test_allocator_callbacks);

    g_test_add_func("/wmem/utils/misc",    wmem_test_miscutls);
    g_test_add_func("/wmem/utils/strings", wmem_test_strutls);

    if (g_test_perf()) {
        g_test_add_func("/wmem/utils/stringperf", wmem_test_stringperf);
    }

    g_test_add_func("/wmem/datastruct/array",  wmem_test_array);
    g_test_add_func("/wmem/datastruct/list",   wmem_test_list);
    g_test_add_func("/wmem/datastruct/map",    wmem_test_map);
    g_test_add_func("/wmem/datastruct/queue",  wmem_test_queue);
    g_test_add_func("/wmem/datastruct/stack",  wmem_test_stack);
    g_test_add_func("/wmem/datastruct/strbuf", wmem_test_strbuf);
    g_test_add_func("/wmem/datastruct/strbuf/validate", wmem_test_strbuf_validate);
    g_test_add_func("/wmem/datastruct/tree",   wmem_test_tree);
    g_test_add_func("/wmem/datastruct/itree",  wmem_test_itree);

    ret = g_test_run();

    wmem_cleanup();

    return ret;
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
