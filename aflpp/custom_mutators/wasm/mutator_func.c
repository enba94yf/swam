#include "mutator_func.h"
#include "safe_alloc.h"

/*
   INTERESTING_VALUES Mutation:
   substitute one scalar value with one of the predefined corner values
   multiple times. 
*/
u32 wasm_interest_values_mutate(wasm_mutator_t* data, BinaryenModuleRef mod) {

    afl_state_t* afl = data->afl;
    u32 cur_cnt = 0, max_cnt = rand_below(afl, DEFAULT_MUTATION_MAX) + 1;
    list_t child_node_list;
    memset(&child_node_list, 0, sizeof(list_t));
    u32* func_shuffle = NULL;

    u32 numFuncs = BinaryenGetNumFunctions(mod);
    if (numFuncs > 0) {

        func_shuffle = malloc(sizeof(u32) * numFuncs);
        if (func_shuffle) {

            for (u32 i = 0; i < numFuncs; i++) func_shuffle[i] = i;
            fisher_yates_shuffle(afl, func_shuffle, numFuncs);

        }

    }

    for (BinaryenIndex i = 0; i < numFuncs; i++) {

        BinaryenIndex idx = func_shuffle ? func_shuffle[i] : i;
        BinaryenFunctionRef   func = BinaryenGetFunctionByIndex(mod, idx);
        BinaryenExpressionRef body = BinaryenFunctionGetBody(func);

        /* Skip functions with no body (e.g., imported functions) */
        if (!body) continue;

        // since we do not care about the order of function traversal, we
        // use list_t here.

        list_append(&child_node_list, body);
        LIST_FOREACH(&child_node_list, struct BinaryenExpression, {

            BinaryenIndex size = 0;
            BinaryenExpressionRef* child = NULL;

            // core mutation logic here
            if (BinaryenExpressionGetId(el) == BinaryenConstId() && rand_below(afl, 6) == 0) {

                BinaryenType type = BinaryenExpressionGetType(el);

                if (type == BinaryenTypeInt32()) {

                    // i32.const instruction
                    int32_t new_val = interesting_32[rand_below(afl, sizeof(interesting_32) / sizeof(int32_t))];
                    BinaryenConstSetValueI32(el, new_val);

                } else if (type == BinaryenTypeInt64()) {
                    
                    // i64.const instruction
                    int64_t new_val = interesting_64[rand_below(afl, sizeof(interesting_64) / sizeof(int64_t))];
                    BinaryenConstSetValueI64(el, new_val);

                } else if (type == BinaryenTypeFloat32()) {

                    // f32.const instruction
                    int32_t new_val = interesting_f32[rand_below(afl, sizeof(interesting_f32) / sizeof(int32_t))];
                    struct BinaryenLiteral new_val_lit = BinaryenLiteralFloat32Bits(new_val);
                    BinaryenConstSetValueF32(el, new_val_lit.f32);

                } else if (type == BinaryenTypeFloat64()) {

                    // f64.const instruction
                    int64_t new_val = interesting_f64[rand_below(afl, sizeof(interesting_f64) / sizeof(int64_t))];
                    struct BinaryenLiteral new_val_lit = BinaryenLiteralFloat64Bits(new_val);
                    BinaryenConstSetValueF64(el, new_val_lit.f64);

                }
#ifndef DISABLE_FIXED_WIDTH_SIMD
                else if (type == BinaryenTypeVec128()) {

                    // v128.const instruction
                    uint8_t new_val[16];                    
                    switch (rand_below(afl, 4)) {

                        case 0: /* INTERESTING_I64 + INTERESTING_F64 */
                        {
                            uint64_t* new_val_64 = (uint64_t*)new_val;
                            new_val_64[0] = (uint64_t)interesting_64[rand_below(afl, sizeof(interesting_64) / sizeof(int64_t))];
                            new_val_64[1] = (uint64_t)interesting_f64[rand_below(afl, sizeof(interesting_f64) / sizeof(int64_t))];
                            break;
                        }                        

                        case 1: /* INTERESTING_I32 + INTERESTING_F32 + INTERESTING_I32 + INTERESTING_F32 */
                        {
                            uint32_t* new_val_32 = (uint32_t*)new_val;
                            new_val_32[0] = (uint32_t)interesting_32[rand_below(afl, sizeof(interesting_32) / sizeof(int32_t))];
                            new_val_32[1] = (uint32_t)interesting_f32[rand_below(afl, sizeof(interesting_f32) / sizeof(int32_t))];
                            new_val_32[2] = (uint32_t)interesting_32[rand_below(afl, sizeof(interesting_32) / sizeof(int32_t))];
                            new_val_32[3] = (uint32_t)interesting_f32[rand_below(afl, sizeof(interesting_f32) / sizeof(int32_t))];
                            break;
                        }

                        case 2: /* INTERESTING_I16 * 8 */
                        {
                            uint16_t* new_val_16 = (uint16_t*)new_val;
                            for (int ind = 0; ind < 8; ind++)
                                new_val_16[ind] = (uint16_t)interesting_16[rand_below(afl, sizeof(interesting_16) / sizeof(int16_t))];
                            break;
                        }

                        case 3: /* INTERESTING_I8 * 16 */
                        {
                            for (int ind = 0; ind < 16; ind++)
                                new_val[ind] = (uint8_t)interesting_8[rand_below(afl, sizeof(interesting_8) / sizeof(int8_t))];
                            break;
                        }

                    }

                    BinaryenConstSetValueV128(el, new_val);

                }
#endif

                cur_cnt++;
                
            }

            if (cur_cnt >= max_cnt) goto final;
            
            size = wasm_get_children_expr(el, &child);
            for (BinaryenIndex ind = 0; ind < size; ind++)
                if (child[ind]) list_append(&child_node_list, child[ind]);
            
            if (el_box->next != next)
                next = el_box->next;
            
            LIST_REMOVE_CURRENT_EL_IN_FOREACH();
            free(child);

        })

    }

final:
    free(func_shuffle);
    /* Avoid LIST_FOREACH on an uninitialized list (e.g., module with 0 funcs). */
    if (child_node_list.element_prealloc_buf[0].next) {
        LIST_FOREACH_CLEAR(&child_node_list, struct BinaryenExpression, {});
    }
    return cur_cnt;
}


/*
    OPERATORS Mutation:
    substitue one operator with another which has the same type-checking
    rules, e.g., replace i32.add with i32.sub.
*/
u32 wasm_operators_mutate(wasm_mutator_t* data, BinaryenModuleRef mod) {

    afl_state_t* afl = data->afl;
    u32 cur_cnt = 0, max_cnt = rand_below(afl, OPERATOR_MUTATION_MAX) + 1;
    list_t child_node_list;
    memset(&child_node_list, 0, sizeof(list_t));
    u32* func_shuffle = NULL;

    u32 numFuncs = BinaryenGetNumFunctions(mod);
    if (numFuncs > 0) {

        func_shuffle = malloc(sizeof(u32) * numFuncs);
        if (func_shuffle) {

            for (u32 i = 0; i < numFuncs; i++) func_shuffle[i] = i;
            fisher_yates_shuffle(afl, func_shuffle, numFuncs);

        }

    }

    for (BinaryenIndex i = 0; i < numFuncs; i++) {

        BinaryenIndex idx = func_shuffle ? func_shuffle[i] : i;
        BinaryenFunctionRef   func = BinaryenGetFunctionByIndex(mod, idx);
        BinaryenExpressionRef body = BinaryenFunctionGetBody(func);

        /* Skip functions with no body (e.g., imported functions) */
        if (!body) continue;

        u8 whether_root_node = 1;
        list_append(&child_node_list, body);
        LIST_FOREACH(&child_node_list, struct BinaryenExpression, {

            BinaryenIndex size = 0;
            BinaryenExpressionRef* child = NULL;
            size = wasm_get_children_expr(el, &child);

            // Instead of determining whether we should replace one specific
            // instruction node with another, we determine whether we should
            // replace each child node of current node, and the actual effect
            // should be same.
            //
            // The reason for this is that Binaryen lacks function to find an
            // expression's parent expression.

            if (whether_root_node) {
                InstrTy instr_ty  = wasm_get_instr_type(el);
                s32     index_grp = wasm_find_instr_grps(data, instr_ty);

                if (index_grp != -1 && data->instr_grps[index_grp].num_inst > 1 &&
                    rand_below(afl, 10) == 0) {

                    u32      selected;
                    InstrTy  new_instr_ty;
                    InstrGrp instr_grp = data->instr_grps[index_grp];

                    /* Add max attempts to avoid infinite loop if all instructions have
                       the same hash. */
                    u32 max_attempts = instr_grp.num_inst * 2;
                    u32 attempts = 0;
                    do {
                        selected = rand_below(afl, instr_grp.num_inst);
                        new_instr_ty = instr_grp.groups[selected];
                        attempts++;
                    } while (new_instr_ty.hash_value == instr_ty.hash_value &&
                             attempts < max_attempts);

                    /* If we couldn't find a different instruction, skip this mutation
                       but still advance the LIST_FOREACH iterator. */
                    if (new_instr_ty.hash_value != instr_ty.hash_value) {

                        /* If the old and new instruction have same expression type, then
                           we can make a shortcut that does not require additional
                           generation. */
                        if (new_instr_ty.expr_id == instr_ty.expr_id) {

                            wasm_substitute_expr_fields(afl, el, new_instr_ty);

                        } else {

                            /* We can safely assume here that each child node produces
                               exactly one primitive type instead of multi-value type. */
                            BinaryenExpressionRef new_expr =
                                wasm_generate_one_simple_expr(afl, mod, new_instr_ty,
                                                              child, size);
                            BinaryenFunctionSetBody(func, new_expr);
                            el = new_expr;

                        }

                        cur_cnt++;

                    }

                }

                whether_root_node = 0;
            }

            for (BinaryenIndex ind = 0; ind < size; ind++) {
                if (child[ind]) {

                    InstrTy instr_ty  = wasm_get_instr_type(child[ind]);
                    s32     index_grp = wasm_find_instr_grps(data, instr_ty);

                    if (index_grp != -1 && data->instr_grps[index_grp].num_inst > 1 &&
                        rand_below(afl, 10) == 0) {

                        u32      selected;
                        InstrTy  new_instr_ty;
                        InstrGrp instr_grp = data->instr_grps[index_grp];

                        /* Add max attempts to avoid infinite loop if all instructions
                           have the same hash. */
                        u32 max_attempts = instr_grp.num_inst * 2;
                        u32 attempts = 0;
                        do {
                            selected = rand_below(afl, instr_grp.num_inst);
                            new_instr_ty = instr_grp.groups[selected];
                            attempts++;
                        } while (new_instr_ty.hash_value == instr_ty.hash_value &&
                                 attempts < max_attempts);

                        /* If we couldn't find a different instruction, skip this mutation
                           but still enqueue the child for further traversal. */
                        if (new_instr_ty.hash_value != instr_ty.hash_value) {

                            if (new_instr_ty.expr_id == instr_ty.expr_id) {

                                wasm_substitute_expr_fields(afl, child[ind],
                                                            new_instr_ty);

                            } else {

                                BinaryenIndex          size_desc = 0;
                                BinaryenExpressionRef *descendant = NULL;
                                size_desc =
                                    wasm_get_children_expr(child[ind], &descendant);

                                BinaryenExpressionRef new_expr =
                                    wasm_generate_one_simple_expr(afl, mod,
                                                                  new_instr_ty,
                                                                  descendant,
                                                                  size_desc);
                                wasm_set_children_expr(el, new_expr, ind);
                                child[ind] = new_expr;
                                free(descendant);

                            }

                            cur_cnt++;

                        }

                    }

                    /* Always push the (possibly updated) child to the worklist so the
                       traversal continues, regardless of whether a mutation happened.
                       However, if the mutation resulted in a NULL expression (which can
                       happen if wasm_generate_one_simple_expr fails to handle a specific
                       instruction type), skip adding it to avoid NULL dereferences. */
                    if (child[ind]) {
                        list_append(&child_node_list, child[ind]);
                    }
                }
            }

            if (el_box->next != next)
                next = el_box->next;
            
            LIST_REMOVE_CURRENT_EL_IN_FOREACH();
            free(child);

            if (cur_cnt >= max_cnt) goto final;

        })

    }

final:
    free(func_shuffle);
    if (child_node_list.element_prealloc_buf[0].next) {
        LIST_FOREACH_CLEAR(&child_node_list, struct BinaryenExpression, {});
    }
    return cur_cnt;

}

/*
    CALL_TARGET Mutation:
    substitute the target function of `call` or `call_indirect` instruction with 
    function that has same function signature.
*/
u32 wasm_call_target_function_mutate(wasm_mutator_t* data, BinaryenModuleRef mod) {

    afl_state_t* afl = data->afl;
    u32 cur_cnt = 0, max_cnt = rand_below(afl, CALL_MUTATION_MAX) + 1;
    list_t child_node_list;
    memset(&child_node_list, 0, sizeof(list_t));
    u32* func_shuffle = NULL;

    u32 numFuncs = BinaryenGetNumFunctions(mod);
    if (numFuncs > 0) {

        func_shuffle = malloc(sizeof(u32) * numFuncs);
        if (func_shuffle) {

            for (u32 i = 0; i < numFuncs; i++) func_shuffle[i] = i;
            fisher_yates_shuffle(afl, func_shuffle, numFuncs);

        }

    }

    for (BinaryenIndex i = 0; i < numFuncs; i++) {

        BinaryenIndex idx = func_shuffle ? func_shuffle[i] : i;
        BinaryenFunctionRef   func = BinaryenGetFunctionByIndex(mod, idx);
        BinaryenExpressionRef body = BinaryenFunctionGetBody(func);

        /* Skip functions with no body (e.g., imported functions) */
        if (!body) continue;

        u8 whether_root_node = 1;
        list_append(&child_node_list, body);
        LIST_FOREACH(&child_node_list, struct BinaryenExpression, {

            BinaryenIndex size = 0;
            BinaryenExpressionRef* child = NULL;
            size = wasm_get_children_expr(el, &child);

            if (whether_root_node) {
                BinaryenType ty = BinaryenExpressionGetType(el);
                BinaryenExpressionId id = BinaryenExpressionGetId(el);
                
                if (id == BinaryenCallId() && rand_below(afl, 6) == 0) {

                    // For `call` instruction, we can directly change the call target
                    // without any further concern.
                    const char* cur_func_name = BinaryenCallGetTarget(el);
                    if (cur_func_name) {
                        BinaryenFunctionRef cur_func = BinaryenGetFunction(mod, cur_func_name);
                        if (cur_func) {
                            BinaryenType param_type  = wasm_simplify_type(BinaryenFunctionGetParams(cur_func));
                            BinaryenType result_type = wasm_simplify_type(BinaryenFunctionGetResults(cur_func));

                            BinaryenIndex matches = 0;
                            BinaryenFunctionRef* matched_funcs = NULL;
                            matches = wasm_find_matching_funcs(mod, param_type, result_type, &matched_funcs, false);

                            const char* new_func_name = NULL;
                            if (matches >= 2) {
                                // Add max attempts to avoid infinite loop
                                u32 max_attempts = matches * 2;
                                u32 attempts = 0;
                                do {
                                    BinaryenIndex selected = rand_below(afl, matches);
                                    new_func_name = BinaryenFunctionGetName(matched_funcs[selected]);
                                    attempts++;
                                } while (!strcmp(new_func_name, cur_func_name) && attempts < max_attempts);

                                // If we couldn't find a different function, generate a new one
                                if (!strcmp(new_func_name, cur_func_name)) {
                                    new_func_name = wasm_generate_one_func(data, mod, param_type, result_type);
                                }
                            } else {
                                new_func_name = wasm_generate_one_func(data, mod, param_type, result_type);
                            }

                            if (new_func_name) {
                                BinaryenCallSetTarget(el, new_func_name);
                                cur_cnt++;
                            }
                            free(matched_funcs);
                        }
                    }

                } else if (id == BinaryenCallIndirectId() && rand_below(afl, 6) == 0) {

                    // For `call_indirect` instruction, we have to instead set the cor-
                    // responding table entry with new target function but do not touch
                    // the `call_indirect` instruction itself.
                    
                    BinaryenType param_type  = wasm_simplify_type(BinaryenCallIndirectGetParams(el));
                    BinaryenType result_type = wasm_simplify_type(BinaryenCallIndirectGetResults(el));
                    
                    BinaryenIndex matches = 0;
                    BinaryenFunctionRef* matched_funcs = NULL;
                    matches = wasm_find_matching_funcs(mod, param_type, result_type, &matched_funcs, false);

                    const char* new_func_name = NULL;
                    if (matches >= 2) {
                        BinaryenIndex selected = rand_below(afl, matches);
                        new_func_name = BinaryenFunctionGetName(matched_funcs[selected]);
                    } else {
                        new_func_name = wasm_generate_one_func(data, mod, param_type, result_type);
                    }

                    if (!new_func_name) {
                        free(matched_funcs);
                        goto after_call_indirect_root;
                    }

                    // if target position expression contains block/if/loops with names,
                    // save this expression into a local variable.
                    BinaryenIndex new_local = BinaryenFunctionAddVar(func, BinaryenTypeInt32());
                    BinaryenExpressionRef local_set = BinaryenLocalSet(mod, new_local, BinaryenCallIndirectGetTarget(el));
                    BinaryenExpressionRef local_get = BinaryenLocalGet(mod, new_local, BinaryenTypeInt32());

                    BinaryenCallIndirectSetTarget(el, BinaryenExpressionCopy(local_get, mod));
                    BinaryenExpressionRef func_ref  = BinaryenRefFunc(mod, new_func_name, BinaryenTypeFuncref());
                    BinaryenExpressionRef table_set = BinaryenTableSet(mod, BinaryenCallIndirectGetTable(el), local_get, func_ref);
                    BinaryenExpressionFinalize(el);

                    BinaryenExpressionRef* new_body = calloc(3, sizeof(BinaryenExpressionRef));
                    new_body[0] = local_set; new_body[1] = table_set; new_body[2] = el;

                    BinaryenExpressionRef block = NULL;
                    if (ty == BinaryenTypeUnreachable())
                        block = BinaryenBlock(mod, NULL, new_body, 3, BinaryenTypeUnreachable());
                    else
                        block = BinaryenBlock(mod, NULL, new_body, 3, result_type);

                    BinaryenFunctionSetBody(func, block);
                    free(matched_funcs);
                    free(new_body);
                    cur_cnt++;

                }

after_call_indirect_root:
                whether_root_node = 0;
            }

            for (BinaryenIndex ind = 0; ind < size; ind++) {
                if (child[ind]) {
                    BinaryenType ty = BinaryenExpressionGetType(child[ind]);
                    BinaryenExpressionId id = BinaryenExpressionGetId(child[ind]);

                    if (id == BinaryenCallId() && rand_below(afl, 6) == 0) {

                        const char* cur_func_name = BinaryenCallGetTarget(child[ind]);
                        if (cur_func_name) {
                            BinaryenFunctionRef cur_func = BinaryenGetFunction(mod, cur_func_name);
                            if (cur_func) {
                                BinaryenType param_type  = wasm_simplify_type(BinaryenFunctionGetParams(cur_func));
                                BinaryenType result_type = wasm_simplify_type(BinaryenFunctionGetResults(cur_func));

                                BinaryenIndex matches = 0;
                                BinaryenFunctionRef* matched_funcs = NULL;
                                matches = wasm_find_matching_funcs(mod, param_type, result_type, &matched_funcs, false);

                                const char* new_func_name = NULL;
                                if (matches >= 2) {
                                    // Add max attempts to avoid infinite loop
                                    u32 max_attempts = matches * 2;
                                    u32 attempts = 0;
                                    do {
                                        BinaryenIndex selected = rand_below(afl, matches);
                                        new_func_name = BinaryenFunctionGetName(matched_funcs[selected]);
                                        attempts++;
                                    } while (!strcmp(new_func_name, cur_func_name) && attempts < max_attempts);

                                    // If we couldn't find a different function, generate a new one
                                    if (!strcmp(new_func_name, cur_func_name)) {
                                        new_func_name = wasm_generate_one_func(data, mod, param_type, result_type);
                                    }
                                } else {
                                    new_func_name = wasm_generate_one_func(data, mod, param_type, result_type);
                                }

                                if (new_func_name) {
                                    BinaryenCallSetTarget(child[ind], new_func_name);
                                    cur_cnt++;
                                }
                                free(matched_funcs);
                            }
                        }

                    } else if (id == BinaryenCallIndirectId() && rand_below(afl, 6) == 0) {

                        BinaryenType param_type  = wasm_simplify_type(BinaryenCallIndirectGetParams(child[ind]));
                        BinaryenType result_type = wasm_simplify_type(BinaryenCallIndirectGetResults(child[ind]));

                        BinaryenIndex matches = 0;
                        BinaryenFunctionRef* matched_funcs = NULL;
                        matches = wasm_find_matching_funcs(mod, param_type, result_type, &matched_funcs, false);

                        const char* new_func_name = NULL;
                        if (matches >= 2) {
                            BinaryenIndex selected = rand_below(afl, matches);
                            new_func_name = BinaryenFunctionGetName(matched_funcs[selected]);
                        } else {
                            new_func_name = wasm_generate_one_func(data, mod, param_type, result_type);
                        }

                        if (!new_func_name) {
                            free(matched_funcs);
                            goto after_call_indirect_child;
                        }

                        BinaryenIndex new_local = BinaryenFunctionAddVar(func, BinaryenTypeInt32());
                        BinaryenExpressionRef local_set = BinaryenLocalSet(mod, new_local, BinaryenCallIndirectGetTarget(child[ind]));
                        BinaryenExpressionRef local_get = BinaryenLocalGet(mod, new_local, BinaryenTypeInt32());

                        BinaryenCallIndirectSetTarget(child[ind], BinaryenExpressionCopy(local_get, mod));
                        BinaryenExpressionRef func_ref  = BinaryenRefFunc(mod, new_func_name, BinaryenTypeFuncref());
                        BinaryenExpressionRef table_set = BinaryenTableSet(mod, BinaryenCallIndirectGetTable(child[ind]), local_get, func_ref);
                        BinaryenExpressionFinalize(child[ind]);

                        BinaryenExpressionRef* new_body = calloc(3, sizeof(BinaryenExpressionRef));
                        new_body[0] = local_set; new_body[1] = table_set; new_body[2] = child[ind];

                        BinaryenExpressionRef block = NULL;
                        if (ty == BinaryenTypeUnreachable())
                            block = BinaryenBlock(mod, NULL, new_body, 3, BinaryenTypeUnreachable());
                        else
                            block = BinaryenBlock(mod, NULL, new_body, 3, result_type);

                        wasm_set_children_expr(el, block, ind);
                        free(matched_funcs);
                        free(new_body);
                        cur_cnt++;

                    }

after_call_indirect_child:
                    list_append(&child_node_list, child[ind]);
                }
            }

            if (el_box->next != next)
                next = el_box->next;
            
            LIST_REMOVE_CURRENT_EL_IN_FOREACH();
            free(child);

            if (cur_cnt >= max_cnt) goto final;

        })

    }

final:
    free(func_shuffle);
    if (child_node_list.element_prealloc_buf[0].next) {
        LIST_FOREACH_CLEAR(&child_node_list, struct BinaryenExpression, {});
    }
    return cur_cnt;

}

/*
    BRANCH_TARGET Mutation:
    substitute the target label of `br`, `br_if` or `br_table` with another
    valid target label.
*/
u32 wasm_branch_target_mutate(wasm_mutator_t* data, BinaryenModuleRef mod) {

    afl_state_t* afl = data->afl;
    u32 cur_cnt = 0, max_cnt = rand_below(afl, BRANCH_MUTATION_MAX) + 1;
    list_t child_node_list;
    memset(&child_node_list, 0, sizeof(list_t));
    u32* func_shuffle = NULL;

    u32 numFuncs = BinaryenGetNumFunctions(mod);
    if (numFuncs > 0) {

        func_shuffle = malloc(sizeof(u32) * numFuncs);
        if (func_shuffle) {

            for (u32 i = 0; i < numFuncs; i++) func_shuffle[i] = i;
            fisher_yates_shuffle(afl, func_shuffle, numFuncs);

        }

    }

    for (BinaryenIndex i = 0; i < numFuncs; i++) {

        BinaryenIndex idx = func_shuffle ? func_shuffle[i] : i;
        BinaryenFunctionRef   func = BinaryenGetFunctionByIndex(mod, idx);
        BinaryenExpressionRef body = BinaryenFunctionGetBody(func);

        /* Skip functions with no body (e.g., imported functions) */
        if (!body) continue;

        list_append(&child_node_list, body);
        LIST_FOREACH(&child_node_list, struct BinaryenExpression, {

            BinaryenIndex size = 0;
            BinaryenExpressionRef* child = NULL;
            size = wasm_get_children_expr(el, &child);

            for (BinaryenIndex ind = 0; ind < size; ind++) {
                if (child[ind]) {
                    bool no_match = true;
                    BinaryenType ty = BinaryenExpressionGetType(child[ind]);
                    BinaryenExpressionId id = BinaryenExpressionGetId(child[ind]);

                    if ((id == BinaryenBreakId() || id == BinaryenSwitchId()) && rand_below(afl, 5) == 0) {

                        list_t label_cxt;
                        memset(&label_cxt, 0, sizeof(list_t));
                        wasm_find_available_labels(body, child[ind], &label_cxt);

                        list_t local_cxt;
                        memset(&local_cxt, 0, sizeof(list_t));
                        wasm_find_available_locals(func, &local_cxt);

                        if (id == BinaryenBreakId()) {

                            const char* new_label = NULL;
                            BinaryenType new_type = BinaryenTypeNone();
                            BinaryenType ori_type = BinaryenTypeNone();

                            LIST_FOREACH(&label_cxt, labelCxt, {
                                if (strcmp(el->label_name, BinaryenBreakGetName(child[ind]))) {
                                    if (!new_label || rand_below(afl, 2)) {
                                        new_label = el->label_name;
                                        new_type  = el->label_type;
                                    }
                                } else {
                                    ori_type = el->label_type;
                                }
                            });

                            if (new_label == NULL)
                                goto early_exit;
                            else
                                no_match = false;
 
                            // adjust the branch target and corresponding value expression.
                            BinaryenBreakSetName(child[ind], new_label);
                            if (BinaryenBreakGetCondition(child[ind])) {

                                // for br_if instruction whose type checking rule is [t*] -> [t*],
                                // if we branch to a new target which needs [t'*] on top of stack,
                                // we have to do following conversions:
                                //
                                //  1. save original value types into local variable which has the
                                //     effect of [t*] -> [];
                                //
                                //  2. generate value expressions whose result type is [t'*];
                                //
                                //  3. construct the new br_if instruction with type checking rule
                                //     [t'*] -> [t'*];
                                //
                                //  4. since Binaryen does not support the stacky code, we have to
                                //     save values into a new local whose effect is [t'*] -> [];
                                //
                                //  5. retrieve the original value types by local.get instruction 
                                //     which pushes value types [t*] back to stack;

                                u32 num_exprs = 0;
                                BinaryenExpressionRef* block_exprs = NULL;
                                
                                localCxt* ori_local = NULL;
                                BinaryenExpressionRef ori_local_set = NULL;
                                if (ori_type != BinaryenTypeNone()) {

                                    ori_local = calloc(1, sizeof(localCxt));
                                    if (!ori_local) {
                                        goto early_exit;
                                    }
                                    ori_local->pre_added  = false;
                                    ori_local->local_type = ori_type;
                                    ori_local->local_idx  = local_cxt.element_total_count;

                                    ori_local_set = BinaryenLocalSet(mod, ori_local->local_idx,
                                                                     BinaryenBreakGetValue(child[ind]));
                                    list_append(&local_cxt, (void*)ori_local);
                                    list_append(&child_node_list, ori_local_set);

                                    if (!SAFE_REALLOC_ARRAY(block_exprs, num_exprs, BinaryenExpressionRef)) {
                                        free(block_exprs);
                                        goto early_exit;
                                    }
                                    block_exprs[num_exprs++] = ori_local_set;

                                }

                                /*
                                    This is the one case we need to re-finalize expression's result
                                    type, since new branch target may have different stack type.
                                */
                                if (new_type == BinaryenTypeNone()) {
                                    BinaryenBreakSetValue(child[ind], NULL);
                                } else {
                                    BinaryenExpressionRef new_expr = wasm_generate_instr_seqs_base_case(data, mod, new_type,
                                                                                                        &local_cxt, &label_cxt, 1);
                                    BinaryenBreakSetValue(child[ind], new_expr);
                                }
                                BinaryenExpressionFinalize(child[ind]);

                                localCxt* new_local = NULL;
                                BinaryenExpressionRef new_local_set = NULL;
                                if (new_type != BinaryenTypeNone()) {

                                    new_local = calloc(1, sizeof(localCxt));
                                    if (!new_local) {
                                        free(block_exprs);
                                        goto early_exit;
                                    }
                                    new_local->pre_added  = false;
                                    new_local->local_type = new_type;
                                    new_local->local_idx  = local_cxt.element_total_count;
                                    
                                    list_append(&local_cxt, (void*)new_local);
                                    new_local_set = BinaryenLocalSet(mod, new_local->local_idx, child[ind]);

                                    if (!SAFE_REALLOC_ARRAY(block_exprs, num_exprs, BinaryenExpressionRef)) {
                                        free(block_exprs);
                                        goto early_exit;
                                    }
                                    block_exprs[num_exprs++] = new_local_set;

                                } else {

                                    if (!SAFE_REALLOC_ARRAY(block_exprs, num_exprs, BinaryenExpressionRef)) {
                                        free(block_exprs);
                                        goto early_exit;
                                    }
                                    block_exprs[num_exprs++] = child[ind];

                                }

                                if (ori_type != BinaryenTypeNone()) {

                                    if (!SAFE_REALLOC_ARRAY(block_exprs, num_exprs, BinaryenExpressionRef)) {
                                        free(block_exprs);
                                        goto early_exit;
                                    }
                                    block_exprs[num_exprs++] =
                                        BinaryenLocalGet(mod, ori_local->local_idx, ori_local->local_type);

                                }

                                char* blk_label_str = wasm_find_available_name(data);

                                if (ty == BinaryenTypeUnreachable())
                                    child[ind] = BinaryenBlock(mod, blk_label_str, block_exprs, num_exprs, BinaryenTypeUnreachable());
                                else
                                    child[ind] = BinaryenBlock(mod, blk_label_str, block_exprs, num_exprs, ori_type);
                                
                                wasm_set_children_expr(el, child[ind], ind);
                                free(block_exprs);

                            } else {

                                // br instruction has type checking rule [t1* t*] -> [t2*], where
                                // t1* and t2* can be literally anything, so it always type checks.

                                if (new_type == BinaryenTypeNone()) {
                                    BinaryenBreakSetValue(child[ind], NULL);
                                } else {
                                    BinaryenExpressionRef new_expr = wasm_generate_instr_seqs_base_case(data, mod, new_type,
                                                                                                        &local_cxt, &label_cxt, 1);
                                    BinaryenBreakSetValue(child[ind], new_expr);
                                }
                                
                            }

                        }

                        if (id == BinaryenSwitchId()) {

                            // there are many ways to mutate a `br_table` instruction, we consider 4
                            // possible mutation strategies:
                            //  1. insert a new target label at randomly selected index (with same 
                            //     type checking rule).
                            //  2. substitute a target label at randomly selected index with another
                            //     label that has same type checking rule.
                            //  3. remove a target label at randomly selected index.
                            //  4. construct a brand-new `br_table` instruction with different type
                            //     checking rule.

                            u32 strategy = rand_below(afl, 4) + 1;
                            if (strategy == 1 || strategy == 2) {

                                // fetch the original stack type of target labels.
                                const char*  dft_label = BinaryenSwitchGetDefaultName(child[ind]);
                                BinaryenType ori_type  = BinaryenTypeNone();
                                LIST_FOREACH(&label_cxt, labelCxt, {
                                    if (!strcmp(el->label_name, dft_label)) ori_type = el->label_type;
                                });

                                // find a new target label with same stack type.
                                const char*  new_label = NULL;
                                LIST_FOREACH(&label_cxt, labelCxt, {
                                    if (el->label_type == ori_type) {
                                        if (!new_label || rand_below(afl, 2)) new_label = el->label_name;
                                    }
                                });

                                BinaryenIndex num_names = BinaryenSwitchGetNumNames(child[ind]);
                                if (strategy == 1) {

                                    if (num_names == 0) {
                                        BinaryenSwitchAppendName(child[ind], new_label);
                                    } else {
                                        BinaryenIndex selected = rand_below(afl, num_names + 1);
                                        if (selected == num_names) {
                                            BinaryenSwitchAppendName(child[ind], new_label);
                                        } else {
                                            BinaryenSwitchInsertNameAt(child[ind], selected, new_label);
                                        }
                                    }

                                } else {

                                    if (num_names == 0) {
                                        BinaryenSwitchSetDefaultName(child[ind], new_label);
                                    } else {
                                        BinaryenIndex selected = rand_below(afl, num_names + 1);
                                        if (selected == num_names) {
                                            BinaryenSwitchSetDefaultName(child[ind], new_label);
                                        } else {
                                            BinaryenSwitchSetNameAt(child[ind], selected, new_label);
                                        }
                                    }

                                }

                                no_match = false;
                                list_append(&child_node_list, child[ind]);

                            } else if (strategy == 3) {

                                BinaryenIndex num_names = BinaryenSwitchGetNumNames(child[ind]);
                                if (num_names != 0) {
                                    BinaryenIndex selected = rand_below(afl, num_names);
                                    BinaryenSwitchRemoveNameAt(child[ind], selected);

                                    no_match = false;
                                    list_append(&child_node_list, child[ind]);
                                }

                            } else {

                                // fetch the original stack type of target labels.
                                const char*  dft_label = BinaryenSwitchGetDefaultName(child[ind]);
                                BinaryenType ori_type  = BinaryenTypeNone();
                                LIST_FOREACH(&label_cxt, labelCxt, {
                                    if (!strcmp(el->label_name, dft_label)) ori_type = el->label_type;
                                })

                                // pick one stack type that is different from current one.
                                BinaryenType new_type  = ori_type;
                                LIST_FOREACH(&label_cxt, labelCxt, {
                                    if (el->label_type != ori_type) {
                                        if (new_type == ori_type || rand_below(afl, 2)) new_type = el->label_type;
                                    }
                                })

                                if (new_type == ori_type)
                                    goto early_exit;
                                else
                                    no_match = false;

                                // find all target labels that have same type as the new stack type.
                                u32 num_new_labels = 0;
                                const char* *new_labels = NULL;
                                LIST_FOREACH(&label_cxt, labelCxt, {
                                    if (el->label_type == new_type) {
                                        if (!SAFE_REALLOC_ARRAY(new_labels, num_new_labels, const char*)) {
                                            free(new_labels);
                                            goto early_exit;
                                        }
                                        new_labels[num_new_labels++] = el->label_name;
                                    }
                                })

                                // construct a brand-new `br_table` instruction.
                                u32 num_cands = rand_below(afl, 10) + 1;
                                const char** candidates =
                                    malloc(sizeof(const char*) * num_cands);
                                if (!candidates) {

                                    free(new_labels);
                                    goto early_exit;

                                }
                                for (u32 i = 0; i < num_cands; i++) {
                                    u32 selected = rand_below(afl, num_new_labels);
                                    candidates[i] = new_labels[selected];
                                }
                                const char* defaults =
                                    new_labels[rand_below(afl, num_new_labels)];

                                // br_table instruction has type-checking rule [t1* t*] -> [t2*], where
                                // t1* and t2* can be literally anything, so it also always type checks.
                                BinaryenExpressionRef new_val  = NULL;
                                if (new_type != BinaryenTypeNone())
                                    new_val = wasm_generate_instr_seqs_base_case(
                                        data, mod, new_type, &local_cxt, &label_cxt, 1);
                                BinaryenExpressionRef ori_cond =
                                    BinaryenSwitchGetCondition(child[ind]);
                                child[ind] = BinaryenSwitch(
                                    mod, candidates, num_cands, defaults, ori_cond, new_val);
                                wasm_set_children_expr(el, child[ind], ind);

                                list_append(&child_node_list, ori_cond);
                                free(candidates);
                                free(new_labels);

                            }

                        }

                    early_exit:

                        if (local_cxt.element_total_count > 0) {
                            LIST_FOREACH(&local_cxt, localCxt, {
                                if (!el->pre_added) BinaryenFunctionAddVar(func, el->local_type);
                            })
                        }

                        if (!no_match) cur_cnt++;
                        if (local_cxt.element_total_count) LIST_FOREACH_CLEAR(&local_cxt, localCxt, {free(el);});
                        if (label_cxt.element_total_count) LIST_FOREACH_CLEAR(&label_cxt, labelCxt, {free(el);});

                    }

                    if (no_match) list_append(&child_node_list, child[ind]);

                }
            }

            if (el_box->next != next)
                next = el_box->next;
            
            LIST_REMOVE_CURRENT_EL_IN_FOREACH();
            free(child);

            if (cur_cnt >= max_cnt) goto final;

        });

    }

final:
    free(func_shuffle);
    if (child_node_list.element_prealloc_buf[0].next) {
        LIST_FOREACH_CLEAR(&child_node_list, struct BinaryenExpression, {});
    }
    return cur_cnt;

}

/*
    RECURSIVE Mutation:
    make structured instructions more nested by wrapping them with new
    structured instructions that have same stack types.
*/
u32 wasm_recursive_mutate(wasm_mutator_t* data, BinaryenModuleRef mod) {

    afl_state_t* afl = data->afl;
    u32 cur_cnt = 0, max_cnt = rand_below(afl, RECURSE_MUTATION_MAX) + 1;
    list_t child_node_list;
    memset(&child_node_list, 0, sizeof(list_t));
    u32* func_shuffle = NULL;

    u32 numFuncs = BinaryenGetNumFunctions(mod);
    if (numFuncs > 0) {

        func_shuffle = malloc(sizeof(u32) * numFuncs);
        if (func_shuffle) {

            for (u32 i = 0; i < numFuncs; i++) func_shuffle[i] = i;
            fisher_yates_shuffle(afl, func_shuffle, numFuncs);

        }

    }

    for (BinaryenIndex i = 0; i < numFuncs; i++) {

        BinaryenIndex idx = func_shuffle ? func_shuffle[i] : i;
        BinaryenFunctionRef   func = BinaryenGetFunctionByIndex(mod, idx);
        BinaryenExpressionRef body = BinaryenFunctionGetBody(func);

        /* Skip functions with no body (e.g., imported functions) */
        if (!body) continue;

        u8 whether_root_node = 1;
        list_append(&child_node_list, body);
        LIST_FOREACH(&child_node_list, struct BinaryenExpression, {

            BinaryenIndex size = 0;
            BinaryenExpressionRef* child = NULL;
            size = wasm_get_children_expr(el, &child);

            if (whether_root_node) {
                BinaryenType ty = BinaryenExpressionGetType(el);
                BinaryenExpressionId id = BinaryenExpressionGetId(el);

                if ((id == BinaryenBlockId() || id == BinaryenLoopId() || id == BinaryenIfId())
                    && rand_below(afl, 8) == 0) {

                    // the general idea is that we can wrap existing structured
                    // instruction into another one as long as their stack types
                    // match, e.g., suppose a block has stack type [t*]:
                    //
                    //  (block $bb (result [t*])
                    //    (...)
                    //  )
                    //
                    // recursive mutator wraps it into another structured instr-
                    // uction that has same type:
                    //
                    //  (loop $bb' (result [t*])
                    //    (block $bb (result [t*])
                    //      (...)
                    //    )
                    //  )

                    u32 wrapper = rand_below(afl, 3);
                    if (wrapper == 0 || wrapper == 1) {

                        char* label_str = wasm_find_available_name(data);

                        BinaryenExpressionRef new_expr = NULL;
                        if (wrapper == 0) {
                            // use `block` as wrapper instruction.
                            new_expr = BinaryenBlock(mod, label_str, &el, 1, ty);
                        } else {
                            // use `loop` as wrapper instruction.
                            new_expr = BinaryenLoop(mod, label_str, el);
                        }

                        BinaryenFunctionSetBody(func, new_expr);

                    } else {

                        // use `i32.const 1` as condition expression to ensure
                        // that original instruction is executed.
                        BinaryenExpressionRef cond_expr = BinaryenConst(mod, BinaryenLiteralInt32(1));

                        // depending on the original instruction's stack type,
                        // we generate a dumb false expression.
                        ty = wasm_simplify_type(ty);
                        BinaryenExpressionRef false_expr = NULL;
                        if (ty == BinaryenTypeUnreachable()) {
                            false_expr = BinaryenUnreachable(mod);
                        } else {
                            u32 num_types = BinaryenTypeArity(ty);

                            if (num_types == 0) {
                                false_expr = BinaryenNop(mod);
                            } else {
                                BinaryenType* types = malloc(sizeof(BinaryenType) * num_types);
                                BinaryenExpressionRef* type_exprs =
                                    malloc(sizeof(BinaryenExpressionRef) * num_types);

                                if (!types || !type_exprs) {

                                    free(types);
                                    free(type_exprs);
                                    goto final;

                                }

                                BinaryenTypeExpand(ty, types);

                                for (u32 ii = 0; ii < num_types; ii++) {
                                    InstrTy inst; memset(&inst, 0, sizeof(InstrTy));

                                    if (types[ii] == BinaryenTypeExternref()) {
                                        FILL_INSTR_TYPE_OPD(inst, BinaryenRefNullId(), types[ii]);
                                    } else if (types[ii] == BinaryenTypeFuncref()) {
                                        FILL_INSTR_TYPE_OPD(
                                            inst,
                                            rand_below(afl, 2) ? BinaryenRefNullId()
                                                               : BinaryenRefFuncId(),
                                            types[ii]);
                                    } else {
                                        FILL_INSTR_TYPE_OPD(inst, BinaryenConstId(), types[ii]);
                                    }

                                    type_exprs[ii] =
                                        wasm_generate_one_simple_expr(afl, mod, inst, NULL, 0);
                                }

                                if (num_types > 1) {
                                    false_expr = BinaryenTupleMake(mod, type_exprs, num_types);
                                } else {
                                    false_expr = type_exprs[0];
                                }

                                free(type_exprs);
                                free(types);

                            }
                        }

                        // use `if` as wrapper instruction.
                        BinaryenExpressionRef new_expr = BinaryenIf(mod, cond_expr, el, false_expr);
                        BinaryenFunctionSetBody(func, new_expr);

                    }

                    cur_cnt++;

                }

                whether_root_node = 0;
            }

            for (BinaryenIndex ind = 0; ind < size; ind++) {
                if (child[ind]) {
                    BinaryenType ty = BinaryenExpressionGetType(child[ind]);
                    BinaryenExpressionId id = BinaryenExpressionGetId(child[ind]);

                    if ((id == BinaryenBlockId() || id == BinaryenLoopId() || id == BinaryenIfId())
                        && rand_below(afl, 8) == 0) {

                        u32 wrapper = rand_below(afl, 3);
                        if (wrapper == 0 || wrapper == 1) {
                            
                            char* label_str = wasm_find_available_name(data);

                            BinaryenExpressionRef new_expr = NULL;
                            if (wrapper == 0) {
                                new_expr = BinaryenBlock(mod, label_str, &child[ind], 1, ty);
                            } else {
                                new_expr = BinaryenLoop(mod, label_str, child[ind]);
                            }

                            wasm_set_children_expr(el, new_expr, ind);

                        } else {

                            ty = wasm_simplify_type(ty);
                            BinaryenExpressionRef cond_expr = BinaryenConst(mod, BinaryenLiteralInt32(1));
                            
                            BinaryenExpressionRef false_expr = NULL;
                            if (ty == BinaryenTypeUnreachable()) {
                                false_expr = BinaryenUnreachable(mod);
                            } else {
                                u32 num_types = BinaryenTypeArity(ty);
                                
                                if (num_types == 0) {
                                    false_expr = BinaryenNop(mod);
                                } else {
                                    BinaryenType* types = malloc(sizeof(BinaryenType) * num_types);
                                    BinaryenExpressionRef* type_exprs =
                                        malloc(sizeof(BinaryenExpressionRef) * num_types);

                                    if (!types || !type_exprs) {

                                        free(types);
                                        free(type_exprs);
                                        goto final;

                                    }

                                    BinaryenTypeExpand(ty, types);

                                    for (u32 ii = 0; ii < num_types; ii++) {
                                        InstrTy inst; memset(&inst, 0, sizeof(InstrTy));

                                        if (types[ii] == BinaryenTypeExternref()) {
                                            FILL_INSTR_TYPE_OPD(inst, BinaryenRefNullId(), types[ii]);
                                        } else if (types[ii] == BinaryenTypeFuncref()) {
                                            FILL_INSTR_TYPE_OPD(
                                                inst,
                                                rand_below(afl, 2) ? BinaryenRefNullId()
                                                                   : BinaryenRefFuncId(),
                                                types[ii]);
                                        } else {
                                            FILL_INSTR_TYPE_OPD(inst, BinaryenConstId(), types[ii]);
                                        }
                                        
                                        type_exprs[ii] = wasm_generate_one_simple_expr(
                                            afl, mod, inst, NULL, 0);
                                    }

                                    if (num_types > 1) {
                                        false_expr = BinaryenTupleMake(mod, type_exprs, num_types);
                                    } else {
                                        false_expr = type_exprs[0];
                                    }

                                    free(type_exprs);
                                    free(types);

                                }
                            }

                            BinaryenExpressionRef new_expr = BinaryenIf(mod, cond_expr, child[ind], false_expr);
                            wasm_set_children_expr(el, new_expr, ind);

                        }

                        cur_cnt++;

                    }

                    list_append(&child_node_list, child[ind]);

                }
            }

            if (el_box->next != next)
                next = el_box->next;
            
            LIST_REMOVE_CURRENT_EL_IN_FOREACH();
            free(child);

            if (cur_cnt >= max_cnt) goto final;

        });

    }

final:
    free(func_shuffle);
    if (child_node_list.element_prealloc_buf[0].next) {
        LIST_FOREACH_CLEAR(&child_node_list, struct BinaryenExpression, {});
    }
    return cur_cnt;

}

/*
    SPLICING Mutation:
    take two seed inputs from the queue (one in the form of fragment pool),
    combine or concatenate them together to construct a new testcase.
*/
u32 wasm_splicing_mutate(wasm_mutator_t* data, BinaryenModuleRef mod_1, BinaryenModuleRef mod_2,
                          list_t* fragment_pool) {

    afl_state_t* afl = data->afl;
    u32 cur_ins_cnt = 0, max_ins_cnt = rand_below(afl, SPLICE_INSERT_MUTATION_MAX) + 1;
    u32 cur_ovw_cnt = 0, max_ovw_cnt = rand_below(afl, SPLICE_OVERWRITE_MUTATION_MAX) + 1;
    list_t child_node_list;
    memset(&child_node_list, 0, sizeof(list_t));
    u32* func_shuffle = NULL;

    u32 numFuncs = BinaryenGetNumFunctions(mod_1);
    if (numFuncs > 0) {

        func_shuffle = malloc(sizeof(u32) * numFuncs);
        if (func_shuffle) {

            for (u32 i = 0; i < numFuncs; i++) func_shuffle[i] = i;
            fisher_yates_shuffle(afl, func_shuffle, numFuncs);

        }

    }

    for (BinaryenIndex i = 0; i < numFuncs; i++) {

        BinaryenIndex idx = func_shuffle ? func_shuffle[i] : i;
        BinaryenFunctionRef   func = BinaryenGetFunctionByIndex(mod_1, idx);
        BinaryenExpressionRef body = BinaryenFunctionGetBody(func);

        /* Skip functions with no body (e.g., imported functions) */
        if (!body) continue;

        list_append(&child_node_list, body);
        LIST_FOREACH(&child_node_list, struct BinaryenExpression, {

            BinaryenIndex size = 0;
            BinaryenExpressionRef* child = NULL;
            size = wasm_get_children_expr(el, &child);

            // overwrite sub-expressions with fragments from splicing module.
            for (BinaryenIndex ind = 0; ind < size; ind++) {
                if (child[ind]) {
                    bool substituted = false;

                    if (rand_below(afl, 10) == 0) {
                        BinaryenType ty = BinaryenExpressionGetType(child[ind]);
                        ty = wasm_simplify_type(ty);

                        fragGrp* group = NULL;
                        if (fragment_pool->element_total_count > 0) {
                            LIST_FOREACH(fragment_pool, fragGrp, {
                                if (el->frag_type == ty) {
                                    group = el;
                                    break;
                                }
                            });
                        }

                        if (group) {
                            u32 selected = rand_below(afl, group->num_frags);
                            Frag new_frag = group->frags[selected];

                            Frag old_frag;
                            old_frag.func = func; old_frag.expr = child[ind];
                            substituted = wasm_splicing_fixup(data, mod_1, mod_2, &old_frag, &new_frag);

                            if (substituted) {
                                wasm_set_children_expr(el, new_frag.expr, ind);
                                cur_ovw_cnt++;
                            }
                        }
                    }

                    if (!substituted) list_append(&child_node_list, child[ind]);
                }
            }

            // we only consider insertion for block expression.
            BinaryenExpressionId id = BinaryenExpressionGetId(el);
            if (id == BinaryenBlockId() && BinaryenBlockGetNumChildren(el) && rand_below(afl, 15) == 0) {
                fragGrp* group = NULL;
                if (fragment_pool->element_total_count > 0) {
                    LIST_FOREACH(fragment_pool, fragGrp, {
                        if (el->frag_type == BinaryenTypeNone()) {
                            group = el;
                            break;
                        }
                    })
                }

                if (group) {
                    u32 selected = rand_below(afl, group->num_frags);
                    Frag new_frag = group->frags[selected];

                    Frag old_frag;
                    u32 ins_idx = rand_below(afl, BinaryenBlockGetNumChildren(el));
                    old_frag.func = func; 
                    old_frag.expr = BinaryenBlockGetChildAt(el, ins_idx);

                    bool success = wasm_splicing_fixup(data, mod_1, mod_2, &old_frag, &new_frag);
                    if (success) {
                        BinaryenBlockInsertChildAt(el, ins_idx, new_frag.expr);
                        cur_ins_cnt++;
                    }
                }
            }

            if (el_box->next != next)
                next = el_box->next;
            
            LIST_REMOVE_CURRENT_EL_IN_FOREACH();
            free(child);

            if (cur_ins_cnt >= max_ins_cnt || cur_ovw_cnt >= max_ovw_cnt) goto final;

        });

    }

final:
    free(func_shuffle);
    if (child_node_list.element_prealloc_buf[0].next) {
        LIST_FOREACH_CLEAR(&child_node_list, struct BinaryenExpression, {});
    }
    return (cur_ins_cnt + cur_ovw_cnt);

}
