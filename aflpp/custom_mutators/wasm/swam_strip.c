#include "swam_strip.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define SWAM_MARKER_BEFORE "__swam_marker_before"
#define SWAM_MARKER_AFTER  "__swam_marker_after"

/* Reuse the mutator's existing Binaryen child access helpers so stripping
   remains correct even if a previously injected probe block is moved around. */
BinaryenIndex wasm_get_children_expr(BinaryenExpressionRef parent,
                                     BinaryenExpressionRef** child);
void wasm_set_children_expr(BinaryenExpressionRef parent,
                            BinaryenExpressionRef new_child,
                            BinaryenIndex index);

static bool swam_is_marker_name(const char* name) {

    if (name == NULL) return false;

    return strcmp(name, SWAM_MARKER_BEFORE) == 0 ||
           strcmp(name, SWAM_MARKER_AFTER) == 0;

}

static bool swam_is_marker_set(BinaryenExpressionRef expr) {

    if (expr == NULL) return false;
    if (BinaryenExpressionGetId(expr) != BinaryenGlobalSetId()) return false;

    return swam_is_marker_name(BinaryenGlobalSetGetName(expr));

}

static void swam_strip_expr(BinaryenModuleRef mod, BinaryenExpressionRef expr) {

    BinaryenExpressionRef* children = NULL;
    BinaryenIndex num_children = 0;

    if (mod == NULL || expr == NULL) return;

    num_children = wasm_get_children_expr(expr, &children);

    for (BinaryenIndex i = 0; i < num_children; i++) {

        BinaryenExpressionRef child = children[i];

        if (swam_is_marker_set(child)) {
            wasm_set_children_expr(expr, BinaryenNop(mod), i);
        } else {
            swam_strip_expr(mod, child);
        }

    }

    free(children);
    BinaryenExpressionFinalize(expr);

}

void swam_strip_markers(BinaryenModuleRef mod) {

    if (mod == NULL) return;

    BinaryenIndex num_funcs = BinaryenGetNumFunctions(mod);
    for (BinaryenIndex i = 0; i < num_funcs; i++) {

        BinaryenFunctionRef func = BinaryenGetFunctionByIndex(mod, i);
        BinaryenExpressionRef body = NULL;

        if (func == NULL) continue;

        body = BinaryenFunctionGetBody(func);
        if (body == NULL) continue;

        if (swam_is_marker_set(body)) {
            body = BinaryenNop(mod);
            BinaryenFunctionSetBody(func, body);
        } else {
            swam_strip_expr(mod, body);
        }

        BinaryenExpressionFinalize(body);

    }

}
