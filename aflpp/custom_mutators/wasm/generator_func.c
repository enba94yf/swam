#include "mutator_func.h"
#include "safe_alloc.h"

/*
    Randomly generate an instruction sequence which produces `result` on top of stack, and
    this function is mainly intended for generating the control-flow skeleton of the inst-
    ruction sequence.
    NOTE that any input parameter type should be converted to local before CALLING TO THIS
    FUNCTION, just for simplicity.
    @param data:        global wasm mutator state.
    @param mod:         global wasm module representation.
    @param result:      the needed output result type.
    @param local_cxt:   the local variables that are maintained during generation.
    @param label_cxt:   the labels that are introduced by structured control instructions.
    @param depth:       maximum recursion depth.
    @return:            the generated instruction sequence.
*/
BinaryenExpressionRef wasm_generate_instr_seqs_top_level(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, 
                                                         list_t* local_cxt, list_t* label_cxt, s32 depth) {
    
    afl_state_t* afl = data->afl;
    result = wasm_simplify_type(result);

    // if we reach to the end of recursive generation, call `wasm_generate_instr_seqs_base_case`
    // function to generate the flesh and blood for us.
    if (depth <= 0) {
        return wasm_generate_instr_seqs_base_case(data, mod, result, local_cxt, label_cxt, 3);
    }

    // otherwise we generate a block to contain all sub-block(/loop/if)s that we gonna generate
    // in a recursive manner.
    
    // come up with a block label first and add it to the labelCxt.
    char* top_label_str = wasm_find_available_name(data);

    labelCxt* top_cxt = calloc(1, sizeof(labelCxt));
    if (!top_cxt) {
        /* Allocation failure: fall back to simpler non-recursive generation. */
        return wasm_generate_instr_seqs_base_case(data, mod, result, local_cxt, label_cxt, 3);
    }

    top_cxt->pre_added  = false;
    top_cxt->label_type = result;
    top_cxt->label_name = top_label_str;
    list_append(label_cxt, (void*)top_cxt);

    u32* tmp_locals = NULL;
    BinaryenType* tmp_local_types = NULL;
    BinaryenExpressionRef* block_expr = NULL;
    BinaryenExpressionRef top_blk = NULL;
    BinaryenType* results = NULL;
    u32 num_results = BinaryenTypeArity(result), num_subs = 0, num_locals = 0;
    if (num_results >= 1) {

        // if the result type consists of multiple value types, decompose it into subtypes
        // and each subtype will be produced by corresponding sub-block(/loop/if).
        results = malloc(sizeof(BinaryenType) * num_results);
        if (!results) {
            goto oom;
        }
        BinaryenTypeExpand(result, results);

        u32 cur_index = 0;
        while (cur_index < num_results) {

            u32 local_idx = (u32)-1;
            /* Pick at least one result type to ensure forward progress.
               A previous version allowed move_steps==0, which caused an
               infinite loop and unbounded allocations that could corrupt
               memory / crash afl-fuzz. */
            u32 remaining = num_results - cur_index;
            u32 move_steps = rand_below(afl, remaining) + 1;
            BinaryenType sub_type = BinaryenTypeCreate(results + cur_index, move_steps);

            if (move_steps > 0) {

                // if we produce values on top of stack, save produced values into local variables
                // since Binaryen cannot handle stacky code properly.
                local_idx = local_cxt->element_total_count;

                if (!SAFE_REALLOC_ARRAY(tmp_locals, num_locals, u32) ||
                    !SAFE_REALLOC_ARRAY(tmp_local_types, num_locals, BinaryenType)) {
                    goto oom;
                }
                tmp_locals[num_locals] = local_idx;
                tmp_local_types[num_locals] = sub_type;
                num_locals++;

                localCxt* cur_local = calloc(1, sizeof(localCxt));
                if (!cur_local) {
                    goto oom;
                }
                cur_local->pre_added = false;
                cur_local->local_type = sub_type;
                cur_local->local_idx = local_idx;
                list_append(local_cxt, (void*)cur_local);

            }

            // select one of the structured instructions as constructor.
            u32 selected = rand_below(afl, 3);
            BinaryenExpressionRef new_expr = NULL;
            if (selected == 0) {

                // use block as constructor.
                new_expr = wasm_generate_instr_seqs_top_level(data, mod, sub_type, local_cxt, 
                                                              label_cxt, depth - 1);

            } else if (selected == 1) {

                // use loop as constructor.
                char* sub_label_str = wasm_find_available_name(data);

                labelCxt* sub_cxt = calloc(1, sizeof(labelCxt));
                if (!sub_cxt) {
                    goto oom;
                }
                sub_cxt->pre_added = false;
                sub_cxt->label_name = sub_label_str;
                sub_cxt->label_type = BinaryenTypeNone();
                list_append(label_cxt, (void*)sub_cxt);

                BinaryenExpressionRef sub_expr = wasm_generate_instr_seqs_top_level(data, mod, sub_type, local_cxt, 
                                                                                    label_cxt, depth - 1);
                new_expr = BinaryenLoop(mod, sub_label_str, sub_expr);

                list_remove(label_cxt, (void*)sub_cxt);
                free(sub_cxt);

            } else if (selected == 2) {

                // use if-statement as constructor.
                BinaryenExpressionRef cond_expr = wasm_generate_instr_seqs_top_level(data, mod, BinaryenTypeInt32(),
                                                                                     local_cxt, label_cxt, depth - 1);
                BinaryenExpressionRef true_expr = wasm_generate_instr_seqs_top_level(data, mod, sub_type, local_cxt,
                                                                                     label_cxt, depth - 1);
                BinaryenExpressionRef false_expr = wasm_generate_instr_seqs_top_level(data, mod, sub_type, local_cxt,
                                                                                      label_cxt, depth - 1);
                
                new_expr = BinaryenIf(mod, cond_expr, true_expr, false_expr);
                
            }

            if (!SAFE_REALLOC_ARRAY(block_expr, num_subs, BinaryenExpressionRef)) {
                goto oom;
            }
            if (move_steps > 0) {
                BinaryenExpressionRef set_expr = BinaryenLocalSet(mod, local_idx, new_expr);
                block_expr[num_subs++] = set_expr;
            } else {
                block_expr[num_subs++] = new_expr;
            }

            cur_index += move_steps;

        }

    } else {

        // if the result type is BinaryenTypeNone, then try to make things easier
        // by generating only one structured instruction.
        if (!SAFE_REALLOC_ARRAY(block_expr, num_subs, BinaryenExpressionRef)) {
            goto oom;
        }

        u32 selected = rand_below(afl, 3);
        if (selected == 0) {

            // use block as constructor.
            BinaryenExpressionRef sub_expr = wasm_generate_instr_seqs_top_level(data, mod, BinaryenTypeNone(), local_cxt,
                                                                                label_cxt, depth - 1);
            block_expr[num_subs++] = sub_expr;

        } else if (selected == 1) {

            // use loop as constructor.
            char* sub_label_str = wasm_find_available_name(data);

            labelCxt* sub_cxt = calloc(1, sizeof(labelCxt));
            sub_cxt->pre_added = false;
            sub_cxt->label_name = sub_label_str;
            sub_cxt->label_type = BinaryenTypeNone();
            list_append(label_cxt, (void*)sub_cxt);

            BinaryenExpressionRef sub_expr = wasm_generate_instr_seqs_top_level(data, mod, BinaryenTypeNone(), local_cxt,
                                                                                label_cxt, depth - 1);
            block_expr[num_subs++] = BinaryenLoop(mod, sub_label_str, sub_expr);

            list_remove(label_cxt, (void*)sub_cxt);
            free(sub_cxt);

        } else if (selected == 2) {

            // use if-statement as constructor.
            BinaryenExpressionRef cond_expr = wasm_generate_instr_seqs_top_level(data, mod, BinaryenTypeInt32(), local_cxt,
                                                                                 label_cxt, depth - 1);
            BinaryenExpressionRef true_expr = wasm_generate_instr_seqs_top_level(data, mod, BinaryenTypeNone(), local_cxt,
                                                                                 label_cxt, depth - 1);
            BinaryenExpressionRef false_expr = wasm_generate_instr_seqs_top_level(data, mod, BinaryenTypeNone(), local_cxt,
                                                                                  label_cxt, depth - 1);

            block_expr[num_subs++] = BinaryenIf(mod, cond_expr, true_expr, false_expr);

        }

    }

    if (num_locals > 0) {

        u32 cur_idx = 0, expr_idx = 0;
            BinaryenExpressionRef* get_exprs = malloc(sizeof(BinaryenExpressionRef) * num_results);
            if (!get_exprs) {
                goto oom;
            }
            while (cur_idx < num_locals) {

            u32 cur = tmp_locals[cur_idx];
            BinaryenType cur_type = tmp_local_types[cur_idx];

                u32 num_types = BinaryenTypeArity(cur_type);
                if (num_types > 1) {
                    BinaryenType* cur_types = malloc(sizeof(BinaryenType) * num_types);
                    if (!cur_types) {
                        free(get_exprs);
                        goto oom;
                    }
                    BinaryenTypeExpand(cur_type, cur_types);

                    for (u32 ind = 0; ind < num_types; ind++) {
                        BinaryenExpressionRef get_local = BinaryenLocalGet(mod, cur, cur_type);
                        get_exprs[expr_idx++] = BinaryenTupleExtract(mod, get_local, ind);
                    }
                    free(cur_types);
                } else {
                    get_exprs[expr_idx++] = BinaryenLocalGet(mod, cur, cur_type);
                }

            cur_idx++;

        }

        if (!SAFE_REALLOC_ARRAY(block_expr, num_subs, BinaryenExpressionRef)) {
            goto oom;
        }
        if (num_results > 1) {
            block_expr[num_subs++] = BinaryenTupleMake(mod, get_exprs, num_results);
        } else {
            block_expr[num_subs++] = get_exprs[0];
        }
        free(get_exprs);

    }

    top_blk = BinaryenBlock(mod, top_label_str, block_expr, num_subs, result);
    goto cleanup;

oom:

    /* On allocation failure, fall back to a simpler base-case sequence that
       still produces the requested result type. */
    top_blk = wasm_generate_instr_seqs_base_case(data, mod, result, local_cxt, label_cxt, 3);

cleanup:
    list_remove(label_cxt, (void*)top_cxt);
    free(tmp_local_types);
    free(tmp_locals);
    free(block_expr);
    free(results);
    free(top_cxt);

    return top_blk;

}

/*
    Randomly generate an instruction sequence which produces `result` on top of stack, and
    different from above function, this function is mainly intended for generating actual 
    instructions instead of control-flow skeleton.
    NOTE that any input parameter type should be converted to local before CALLING TO THIS
    FUNCTION, just for simplicity.
    @param data:        global wasm mutator state.
    @param mod:         global wasm module representation.
    @param result:      the needed output result type.
    @param local_cxt:   the local variables that are maintained during generation.
    @param label_cxt:   the labels that are introduced by structured control instructions.
    @param depth:       maximum recursion depth. (must be greater than 0)
    @return:            the generated instruction sequence.
*/
BinaryenExpressionRef wasm_generate_instr_seqs_base_case(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, 
                                                         list_t* local_cxt, list_t* label_cxt, s32 depth) {

    afl_state_t* afl = data->afl;
    result = wasm_simplify_type(result);
    BinaryenExpressionRef result_expr = NULL;
    u32 num_results = BinaryenTypeArity(result);

    if (depth <= 0) {

        if (num_results >= 1 && rand_below(afl, 2)) {
            // try local/global variable get instruction first.
            bool use_local = rand_below(afl, 2);
            result_expr = wasm_generate_one_local_global_get(afl, mod, result, local_cxt, label_cxt, use_local, depth);
            if (result_expr) return result_expr;
        }

        // otherwise, try instructions with stack-operating type () -> (result).
        u32 selected_grp_idx = (u32)-1;
        u32 num_grps = data->num_instr_grps;

        for (u32 ind = 0; ind < num_grps; ind++) {
            InstrGrp cur_grp = data->instr_grps[ind];

            if (cur_grp.num_req == 0 && cur_grp.num_pro == num_results) {
                if (num_results != 0) {
                    BinaryenType grp_type = cur_grp.produced[0];

                    if (grp_type == result && selected_grp_idx == (u32)-1) {
                        selected_grp_idx = ind;
                    } else if (grp_type == result && rand_below(afl, 2)) {
                        selected_grp_idx = ind;
                    }
                } else {
                    if (selected_grp_idx == (u32)-1 || rand_below(afl, 2)) {
                        selected_grp_idx = ind;
                    }
                }
            }
        }

        /* If no matching instruction group is found, bail out instead of
           indexing with (u32)-1, which would be out-of-bounds. */
        if (selected_grp_idx == (u32)-1) {
            return NULL;
        }

        InstrGrp selected_grp = data->instr_grps[selected_grp_idx];
        InstrTy selected_inst = selected_grp.groups[rand_below(afl, selected_grp.num_inst)];
        result_expr = wasm_generate_one_simple_expr(afl, mod, selected_inst, NULL, 0);

        return result_expr;

    }

    if (rand_below(afl, 2) == 0) {
        // try call instruction with 50% possibility.
        result_expr = wasm_generate_one_call(data, mod, result, local_cxt, label_cxt, depth);
        if (result_expr) return result_expr;
    }

    if (rand_below(afl, 5) == 0) {
        // try call_indirect instruction with 20% possibility.
        result_expr = wasm_generate_one_call_indirect(data, mod, result, local_cxt, label_cxt, depth);
        return result_expr;
    }

    if (rand_below(afl, 8) == 0) {
        // try control-flow transfer instruction with 12.5% possibility.
        result_expr = wasm_generate_one_control_transfer(data, mod, result, local_cxt, label_cxt, depth);
        if (result_expr) return result_expr;
    }

    if (num_results >= 1) {

        if (rand_below(afl, 5) == 0) {
            // try local/global get instruction with 20% possibility.
            bool use_local = rand_below(afl, 2);
            result_expr = wasm_generate_one_local_global_get(afl, mod, result, local_cxt, label_cxt, use_local, depth);
            if (result_expr) return result_expr;
        }

        if (rand_below(afl, 5) == 0) {
            // try local.tee instruction with 20% possibility.
            result_expr = wasm_generate_one_local_tee(data, mod, result, local_cxt, label_cxt, depth);
            if (result_expr) return result_expr;
        }

    } else {

        if (rand_below(afl, 5) == 0) {
            // try local/global variable set instruction with 20% possibility.
            bool use_local = rand_below(afl, 2);
            result_expr = wasm_generate_one_local_global_set(data, mod, local_cxt,
                                                             label_cxt, use_local,
                                                             depth);
            if (result_expr) return result_expr;
        }

    }

    // if all above methods fail, try default recursive tree generation.
    if (num_results >= 2) {

        BinaryenType* results_arr = malloc(sizeof(BinaryenType) * num_results);
        BinaryenExpressionRef* result_exprs =
            malloc(sizeof(BinaryenExpressionRef) * num_results);

        if (!results_arr || !result_exprs) {

            free(results_arr);
            free(result_exprs);
            return NULL;

        }

        BinaryenTypeExpand(result, results_arr);

        for (u32 ind = 0; ind < num_results; ind++) {
            result_exprs[ind] = wasm_generate_instr_seqs_base_case(
                data, mod, results_arr[ind], local_cxt, label_cxt, depth - 1);
        }

        result_expr = BinaryenTupleMake(mod, result_exprs, num_results);

        free(result_exprs);
        free(results_arr);

    } else {

        u32 selected_grp_idx = (u32)-1;
        u32 num_grps = data->num_instr_grps;

        for (u32 ind = 0; ind < num_grps; ind++) {
            InstrGrp cur_grp = data->instr_grps[ind];

            if (cur_grp.num_pro == num_results) {
                if (num_results != 0) {
                    BinaryenType grp_type = cur_grp.produced[0];

                    if (grp_type == result && selected_grp_idx == (u32)-1) {
                        selected_grp_idx = ind;
                    } else if (grp_type == result && rand_below(afl, 2)) {
                        selected_grp_idx = ind;
                    }
                } else {
                    if (selected_grp_idx == (u32)-1 || rand_below(afl, 2)) {
                        selected_grp_idx = ind;
                    }
                }
            }
        }

        /* If no matching instruction group is found, bail out instead of
           indexing with (u32)-1, which would be out-of-bounds. */
        if (selected_grp_idx == (u32)-1) {
            return NULL;
        }

        InstrGrp selected_grp = data->instr_grps[selected_grp_idx];
        InstrTy selected_inst = selected_grp.groups[rand_below(afl, selected_grp.num_inst)];

        u32 num_required = selected_grp.num_req;
        if (num_required > 0) {

            BinaryenExpressionRef* child_expr =
                malloc(sizeof(BinaryenExpressionRef) * num_required);
            if (!child_expr) {

                return NULL;

            }

            for (u32 ind = 0; ind < num_required; ind++) {
                child_expr[ind] = wasm_generate_instr_seqs_base_case(
                    data, mod, selected_grp.required[ind], local_cxt, label_cxt, depth - 1);
            }

            result_expr =
                wasm_generate_one_simple_expr(afl, mod, selected_inst, child_expr, num_required);

            free(child_expr);

        } else {

            result_expr = wasm_generate_one_simple_expr(afl, mod, selected_inst, NULL, 0);

        }

    }

    return result_expr;

}

/*
    Construct one local/global variable set instruction, may or may not introduce a new
    local/global variable.
    @param data:        global wasm mutator state.
    @param mod:         global wasm module representation.
    @param local_cxt:   the local variables that are maintained during generation.
    @param label_cxt:   the labels that are introduced by structured control instructions.
    @param use_local:   whether use local variable or not.
    @param depth:       maximum recursion depth.
    @return:            the generated local/global.set instruction.
*/
BinaryenExpressionRef wasm_generate_one_local_global_set(wasm_mutator_t* data, BinaryenModuleRef mod, list_t* local_cxt, list_t* label_cxt,
                                                         bool use_local, s32 depth) {

    afl_state_t* afl = data->afl;
    BinaryenExpressionRef result_expr = NULL;

    if (use_local) {

        if ((local_cxt->element_total_count == 0) || (rand_below(afl, 5) == 0)) {

            // introduce a new local variable and add it to local context.
            u32 local_idx = local_cxt->element_total_count;
            u32 num_types = rand_below(afl, 3) + 1;
            
            BinaryenType* types = malloc(sizeof(BinaryenType) * num_types);
            BinaryenExpressionRef* type_exprs =
                malloc(sizeof(BinaryenExpressionRef) * num_types);

            if (!types || !type_exprs) {

                free(types);
                free(type_exprs);
                return NULL;

            }

            for (u32 ind = 0; ind < num_types; ind++) {

#ifndef DISABLE_FIXED_WIDTH_SIMD
                switch (rand_below(afl, 7)) {
#else
                switch (rand_below(afl, 6)) {
#endif
                    case 0: types[ind] = BinaryenTypeInt32();     break;
                    case 1: types[ind] = BinaryenTypeInt64();     break;
                    case 2: types[ind] = BinaryenTypeFloat32();   break;
                    case 3: types[ind] = BinaryenTypeFloat64();   break;
                    case 4: types[ind] = BinaryenTypeFuncref();   break;
                    case 5: types[ind] = BinaryenTypeExternref(); break;
#ifndef DISABLE_FIXED_WIDTH_SIMD
                    case 6: types[ind] = BinaryenTypeVec128();    break;
#endif
                }

            }

            localCxt* cur_local = calloc(1, sizeof(localCxt));
            if (!cur_local) {

                free(type_exprs);
                free(types);
                return NULL;

            }
            cur_local->pre_added  = false;
            cur_local->local_idx  = local_idx;
            cur_local->local_type = BinaryenTypeCreate(types, num_types);
            list_append(local_cxt, (void*)cur_local);

            for (u32 ind = 0; ind < num_types; ind++) {
                type_exprs[ind] = wasm_generate_instr_seqs_base_case(data, mod, types[ind], local_cxt,
                                                                     label_cxt, depth - 1);
            }

            if (num_types > 1) {
                BinaryenExpressionRef tuple_make = BinaryenTupleMake(mod, type_exprs, num_types);
                result_expr = BinaryenLocalSet(mod, local_idx, tuple_make);
            } else {
                result_expr = BinaryenLocalSet(mod, local_idx, type_exprs[0]);
            }
            
            free(type_exprs);
            free(types);
            
        } else {

            // randomly pick one local variable and set its value.
            u32 local_idx = (u32)-1;
            BinaryenType local_type = BinaryenTypeNone();
            LIST_FOREACH(local_cxt, localCxt, {
                if (local_idx == (u32)-1 || rand_below(afl, 2)) {
                    local_idx  = el->local_idx;
                    local_type = el->local_type; 
                }
            });

            u32 num_local_types = BinaryenTypeArity(local_type);
            if (num_local_types == 0) {

                return NULL;

            }

            BinaryenType* local_types = malloc(sizeof(BinaryenType) * num_local_types);
            BinaryenExpressionRef* local_type_exprs =
                malloc(sizeof(BinaryenExpressionRef) * num_local_types);

            if (!local_types || !local_type_exprs) {

                free(local_types);
                free(local_type_exprs);
                return NULL;

            }

            BinaryenTypeExpand(local_type, local_types);

            for (u32 ind = 0; ind < num_local_types; ind++) {
                local_type_exprs[ind] = wasm_generate_instr_seqs_base_case(
                    data, mod, local_types[ind], local_cxt, label_cxt, depth - 1);
            }

            if (num_local_types > 1) {
                BinaryenExpressionRef tuple_make =
                    BinaryenTupleMake(mod, local_type_exprs, num_local_types);
                result_expr = BinaryenLocalSet(mod, local_idx, tuple_make);
            } else {
                result_expr = BinaryenLocalSet(mod, local_idx, local_type_exprs[0]);
            }

            free(local_type_exprs);
            free(local_types);

        }

    } else {

        bool has_mutable = false;
        for (u32 ind = 0; ind < BinaryenGetNumGlobals(mod); ind++) {
            BinaryenGlobalRef cur = BinaryenGetGlobalByIndex(mod, ind);

            if (BinaryenGlobalIsMutable(cur)) {
                has_mutable = true;
                break;
            }
        }

        if (!has_mutable || rand_below(afl, 5) == 0) {

            // introduce a new global variable.
            u32 global_idx = BinaryenGetNumGlobals(mod);
            int len = snprintf(NULL, 0, "%u", global_idx);
            char* global_name = calloc(len + 1, sizeof(char));
            snprintf(global_name, len + 1, "%u", global_idx);

            u32 num_types = rand_below(afl, 3) + 1;
            BinaryenType* types = malloc(sizeof(BinaryenType) * num_types);
            BinaryenExpressionRef* type_exprs =
                malloc(sizeof(BinaryenExpressionRef) * num_types);
            BinaryenExpressionRef* init_exprs =
                malloc(sizeof(BinaryenExpressionRef) * num_types);

            if (!types || !type_exprs || !init_exprs) {

                free(types);
                free(type_exprs);
                free(init_exprs);
                free(global_name);
                return NULL;

            }
            for (u32 ind = 0; ind < num_types; ind++) {

#ifndef DISABLE_FIXED_WIDTH_SIMD
                switch (rand_below(afl, 7)) {
#else
                switch (rand_below(afl, 6)) {
#endif
                    case 0: types[ind] = BinaryenTypeInt32();     break;
                    case 1: types[ind] = BinaryenTypeInt64();     break;
                    case 2: types[ind] = BinaryenTypeFloat32();   break;
                    case 3: types[ind] = BinaryenTypeFloat64();   break;
                    case 4: types[ind] = BinaryenTypeFuncref();   break;
                    case 5: types[ind] = BinaryenTypeExternref(); break;
#ifndef DISABLE_FIXED_WIDTH_SIMD
                    case 6: types[ind] = BinaryenTypeVec128();    break;
#endif
                }

                type_exprs[ind] = wasm_generate_instr_seqs_base_case(data, mod, types[ind], local_cxt,
                                                                     label_cxt, depth - 1);
                
                InstrTy init_inst; memset(&init_inst, 0, sizeof(InstrTy));
                if (types[ind] == BinaryenTypeExternref()) {
                    FILL_INSTR_TYPE_OPD(init_inst, BinaryenRefNullId(), types[ind]);
                } else if (types[ind] == BinaryenTypeFuncref()) {
                    FILL_INSTR_TYPE_OPD(init_inst, rand_below(afl, 2) ? BinaryenRefNullId() : BinaryenRefFuncId(), types[ind]);
                } else {
                    FILL_INSTR_TYPE_OPD(init_inst, BinaryenConstId(), types[ind]);
                }
                init_exprs[ind] = wasm_generate_one_simple_expr(afl, mod, init_inst, NULL, 0);

            }


            if (num_types > 1) {
                BinaryenExpressionRef tuple_make = BinaryenTupleMake(mod, type_exprs, num_types);
                BinaryenExpressionRef init_tuple = BinaryenTupleMake(mod, init_exprs, num_types);

                BinaryenAddGlobal(mod, global_name, BinaryenTypeCreate(types, num_types), true,
                                  init_tuple);
                result_expr = BinaryenGlobalSet(mod, global_name, tuple_make);
            } else {
                BinaryenAddGlobal(mod, global_name, BinaryenTypeCreate(types, num_types), true,
                                  init_exprs[0]);
                result_expr = BinaryenGlobalSet(mod, global_name, type_exprs[0]);
            }

            free(global_name);
            free(init_exprs);
            free(type_exprs);
            free(types);

        } else {

            // randomly pick one global variable and set its value.
            u32 selected = 0;
            BinaryenGlobalRef cur_global = NULL;
            BinaryenType global_type = BinaryenTypeNone();

            // First, check if there's at least one mutable global to avoid infinite loop
            u32 num_globals = BinaryenGetNumGlobals(mod);
            bool has_mutable_global = false;
            for (u32 i = 0; i < num_globals; i++) {
                BinaryenGlobalRef g = BinaryenGetGlobalByIndex(mod, i);
                if (BinaryenGlobalIsMutable(g)) {
                    has_mutable_global = true;
                    break;
                }
            }

            // If no mutable globals exist, return NULL to skip this mutation
            if (!has_mutable_global || num_globals == 0) {
                return NULL;
            }

            do {
                selected = rand_below(afl, num_globals);
                cur_global = BinaryenGetGlobalByIndex(mod, selected);
                global_type = BinaryenGlobalGetType(cur_global);
            } while (!BinaryenGlobalIsMutable(cur_global));

            u32 num_global_types = BinaryenTypeArity(global_type);
            if (num_global_types == 0) {

                return NULL;

            }

            BinaryenType* global_types = malloc(sizeof(BinaryenType) * num_global_types);
            BinaryenExpressionRef* global_type_exprs =
                malloc(sizeof(BinaryenExpressionRef) * num_global_types);

            if (!global_types || !global_type_exprs) {

                free(global_types);
                free(global_type_exprs);
                return NULL;

            }

            BinaryenTypeExpand(global_type, global_types);

            for (u32 ind = 0; ind < num_global_types; ind++) {
                global_type_exprs[ind] = wasm_generate_instr_seqs_base_case(
                    data, mod, global_types[ind], local_cxt, label_cxt, depth - 1);
            }

            const char* global_name = BinaryenGlobalGetName(cur_global);
            if (num_global_types > 1) {
                BinaryenExpressionRef tuple_make = BinaryenTupleMake(mod, global_type_exprs, num_global_types);
                result_expr = BinaryenGlobalSet(mod, global_name, tuple_make);
            } else {
                result_expr = BinaryenGlobalSet(mod, global_name, global_type_exprs[0]);
            }

            free(global_type_exprs);
            free(global_types);

        }

    }

    return result_expr;

}

/*
    Construct one control flow transfer instruction, depending on the instruction type, we
    may or may not need to produce the result type.
    @param data:        global wasm mutator state.
    @param mod:         global wasm module representation.
    @param result:      the needed output result type.
    @param local_cxt:   the local variables that are maintained during generation.
    @param label_cxt:   the labels that are introduced by structured control instructions.
    @param depth:       maximum recursion depth.
    @return:            the generated control flow transfer instruction.
*/
BinaryenExpressionRef wasm_generate_one_control_transfer(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, list_t* local_cxt,
                                                         list_t* label_cxt, s32 depth) {

    afl_state_t* afl = data->afl;
    u32 selected = rand_below(afl, 4);
    BinaryenExpressionRef result_expr = NULL;

    /* 0:            br l
       1:         br_if l
       2: br_table l* l_n
       3:     unreachable */
    if (selected == 0) {

        const char*  label_name = NULL;
        BinaryenType label_type = BinaryenTypeNone();

        if (label_cxt->element_total_count) {
            LIST_FOREACH(label_cxt, labelCxt, {
                if (label_name == NULL || rand_below(afl, 2)) {
                    label_name = el->label_name;
                    label_type = el->label_type;
                }
            });
        }

        if (label_name) {

            u32 num_label_type = BinaryenTypeArity(label_type);
            if (num_label_type > 0) {

                BinaryenType* label_types = malloc(sizeof(BinaryenType) * num_label_type);
                BinaryenExpressionRef* label_type_exprs =
                    malloc(sizeof(BinaryenExpressionRef) * num_label_type);

                if (!label_types || !label_type_exprs) {

                    free(label_types);
                    free(label_type_exprs);
                    return NULL;

                }

                BinaryenTypeExpand(label_type, label_types);

                for (u32 ind = 0; ind < num_label_type; ind++) {
                    label_type_exprs[ind] = wasm_generate_instr_seqs_base_case(
                        data, mod, label_types[ind], local_cxt, label_cxt, depth - 1);
                }

                if (num_label_type > 1) {
                    BinaryenExpressionRef tuple_make =
                        BinaryenTupleMake(mod, label_type_exprs, num_label_type);
                    result_expr = BinaryenBreak(mod, label_name, NULL, tuple_make);
                } else {
                    result_expr = BinaryenBreak(mod, label_name, NULL, label_type_exprs[0]);
                }

                free(label_type_exprs);
                free(label_types);

            } else {

                result_expr = BinaryenBreak(mod, label_name, NULL, NULL);

            }

        }

    } else if (selected == 1) {

        const char* label_name = NULL;
        if (label_cxt->element_total_count) {
            LIST_FOREACH(label_cxt, labelCxt, {
                if (el->label_type == result && label_name == NULL) {
                    label_name = el->label_name;
                } else if (el->label_type == result && rand_below(afl, 2)) {
                    label_name = el->label_name;
                }
            });
        }

        if (label_name) {

            u32 num_results = BinaryenTypeArity(result);
            if (num_results > 0) {

                BinaryenType* results = malloc(sizeof(BinaryenType) * num_results);
                BinaryenExpressionRef* result_exprs =
                    malloc(sizeof(BinaryenExpressionRef) * num_results);

                if (!results || !result_exprs) {

                    free(results);
                    free(result_exprs);
                    return NULL;

                }

                BinaryenTypeExpand(result, results);

                for (u32 ind = 0; ind < num_results; ind++) {
                    result_exprs[ind] = wasm_generate_instr_seqs_base_case(
                        data, mod, results[ind], local_cxt, label_cxt, depth - 1);
                }

                // the branch condition expression should be generated after value expressions.
                BinaryenExpressionRef cond_expr = wasm_generate_instr_seqs_base_case(
                    data, mod, BinaryenTypeInt32(), local_cxt, label_cxt, depth - 1);

                if (num_results > 1) {
                    BinaryenExpressionRef tuple_make =
                        BinaryenTupleMake(mod, result_exprs, num_results);
                    result_expr = BinaryenBreak(mod, label_name, cond_expr, tuple_make);
                } else {
                    result_expr = BinaryenBreak(mod, label_name, cond_expr, result_exprs[0]);
                }

                free(result_exprs);
                free(results);

            } else {

                BinaryenExpressionRef cond_expr = wasm_generate_instr_seqs_base_case(
                    data, mod, BinaryenTypeInt32(), local_cxt, label_cxt, depth - 1);
                result_expr = BinaryenBreak(mod, label_name, cond_expr, NULL);

            }

        }

    } else if (selected == 2) {

        // come up with a default branch target label first, then find all labels having
        // the same result type as default label.
        const char*  default_label_name = NULL;
        BinaryenType default_label_type = BinaryenTypeNone();
        if (label_cxt->element_total_count) {
            LIST_FOREACH(label_cxt, labelCxt, {
                if (default_label_name == NULL || rand_below(afl, 2)) {
                    default_label_name = el->label_name;
                    default_label_type = el->label_type;
                }
            });
        }

        if (default_label_name) {

            u32 num_selected_label = 0;
            const char** selected_labels = NULL;

            if (label_cxt->element_total_count) {
                LIST_FOREACH(label_cxt, labelCxt, {
                    if (el->label_type == default_label_type && rand_below(afl, 2)) {
                        if (!SAFE_REALLOC_ARRAY(selected_labels, num_selected_label, const char*)) {
                            /* Out of memory – stop collecting more labels but keep the ones
                               we already gathered. */
                            break;
                        }
                        selected_labels[num_selected_label++] = el->label_name;
                    }
                });
            }

            u32 num_label_types = BinaryenTypeArity(default_label_type);
            if (num_label_types > 0) {

                BinaryenType* label_types = malloc(sizeof(BinaryenType) * num_label_types);
                BinaryenExpressionRef* label_type_exprs =
                    malloc(sizeof(BinaryenExpressionRef) * num_label_types);

                if (!label_types || !label_type_exprs) {

                    free(label_types);
                    free(label_type_exprs);
                    free(selected_labels);
                    return NULL;

                }

                BinaryenTypeExpand(default_label_type, label_types);

                for (u32 ind = 0; ind < num_label_types; ind++) {
                    label_type_exprs[ind] = wasm_generate_instr_seqs_base_case(
                        data, mod, label_types[ind], local_cxt, label_cxt, depth - 1);
                }

                // the branch condition expression should be generated after value expressions.
                BinaryenExpressionRef cond_expr = wasm_generate_instr_seqs_base_case(
                    data, mod, BinaryenTypeInt32(), local_cxt, label_cxt, depth - 1);

                if (num_label_types > 1) {
                    BinaryenExpressionRef tuple_make =
                        BinaryenTupleMake(mod, label_type_exprs, num_label_types);
                    result_expr = BinaryenSwitch(mod, selected_labels, num_selected_label,
                                                 default_label_name, cond_expr, tuple_make);
                } else {
                    result_expr = BinaryenSwitch(mod, selected_labels, num_selected_label,
                                                 default_label_name, cond_expr, label_type_exprs[0]);
                }

                free(label_type_exprs);
                free(label_types);

            } else {

                BinaryenExpressionRef cond_expr = wasm_generate_instr_seqs_base_case(
                    data, mod, BinaryenTypeInt32(), local_cxt, label_cxt, depth - 1);
                result_expr = BinaryenSwitch(mod, selected_labels, num_selected_label,
                                             default_label_name, cond_expr, NULL);
            
            }

            free(selected_labels);

        }

    } else {

        result_expr = BinaryenUnreachable(mod);

    }

    return result_expr;

}

/*
    Construct one local variable tee instruction with `result` result type. we require
    exact match for any result type (i.e. no `TupleExtract` involved). 
    @param data:        global wasm mutator state.
    @param mod:         global wasm module representation.
    @param result:      the needed output result type.
    @param local_cxt:   the local variables that are maintained during generation.
    @param label_cxt:   the labels that are introduced by structured control instructions.
    @param depth:       maximum recursion depth.
    @return:            the generated local.tee instruction.
*/
BinaryenExpressionRef wasm_generate_one_local_tee(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, list_t* local_cxt,
                                                  list_t* label_cxt, s32 depth) {

    u32 local_idx = (u32)-1;
    afl_state_t* afl = data->afl;
    BinaryenExpressionRef result_expr = NULL;

    if (local_cxt->element_total_count) {
        LIST_FOREACH(local_cxt, localCxt, {
            BinaryenType local_type = el->local_type;

            if (local_type == result && local_idx == (u32)-1) {
                local_idx = el->local_idx;
            } else if (local_type == result && rand_below(afl, 2)) {
                local_idx = el->local_idx;
            }

        });
    }

    if (local_idx != (u32)-1 || rand_below(afl, 2)) {

        u32 num_results = BinaryenTypeArity(result);
        BinaryenType* results = malloc(sizeof(BinaryenType) * num_results);
        BinaryenExpressionRef* results_expr =
            malloc(sizeof(BinaryenExpressionRef) * num_results);

        if (!results || !results_expr) {

            free(results);
            free(results_expr);
            return NULL;

        }

        BinaryenTypeExpand(result, results);

        if (local_idx == (u32)-1) {

            // could not find one matching local variable, create one and add it to local context.
            local_idx = local_cxt->element_total_count;

            localCxt* cur_local = calloc(1, sizeof(localCxt));
            cur_local->pre_added  = false;
            cur_local->local_type = result;
            cur_local->local_idx  = local_idx;
            list_append(local_cxt, (void*)cur_local);

        }

        for (u32 ind = 0; ind < num_results; ind++) {
            results_expr[ind] = wasm_generate_instr_seqs_base_case(data, mod, results[ind], local_cxt,
                                                                   label_cxt, depth - 1);
        }

        if (num_results > 1) {
            BinaryenExpressionRef tuple_make = BinaryenTupleMake(mod, results_expr, num_results);
            result_expr = BinaryenLocalTee(mod, local_idx, tuple_make, result);
        } else {
            result_expr = BinaryenLocalTee(mod, local_idx, results_expr[0], result);
        }

        free(results_expr);
        free(results);

    }

    return result_expr;

}

/*
    Construct one local/global variable get instruction with `result` result type. For
    result type that contains multiple value types, we require exact match. For result
    type that contains only one value type, we consider `TupleExtract` if needed. 
    @param afl:         global afl state.
    @param mod:         global wasm module representation.
    @param result:      the needed output result type.
    @param local_cxt:   the local variables that are maintained during generation.
    @param label_cxt:   the labels that are introduced by structured control instructions.
    @param use_local:   whether use local variable or not.
    @param depth:       maximum recursion depth.
    @return:            the generated local/global.get instruction.
*/
BinaryenExpressionRef wasm_generate_one_local_global_get(afl_state_t* afl, BinaryenModuleRef mod, BinaryenType result, list_t* local_cxt,
                                                         list_t* label_cxt, bool use_local, s32 depth) {

    BinaryenExpressionRef result_expr = NULL;

    u32 num_results = BinaryenTypeArity(result);
    if (use_local) {
        u32 local_idx = (u32)-1, sub_idx = (u32)-1;
        
        if (num_results > 1) {
            if (local_cxt->element_total_count) {
                LIST_FOREACH(local_cxt, localCxt, {
                    if (el->local_type == result && local_idx == (u32)-1) {
                        local_idx = el->local_idx;
                    } else if (el->local_type == result && rand_below(afl, 2)) {
                        local_idx = el->local_idx;
                    }
                });
            }

            if (local_idx != (u32)-1) result_expr = BinaryenLocalGet(mod, local_idx, result);
            return result_expr;
        }

        BinaryenType selected_type = BinaryenTypeNone();
        if (local_cxt->element_total_count) {
            LIST_FOREACH(local_cxt, localCxt, {
                BinaryenType local_type = el->local_type;
                u32 num_local_types = BinaryenTypeArity(local_type);
                
                if (num_local_types > 1) {
                    BinaryenType* local_types = malloc(sizeof(BinaryenType) * num_local_types);

                    if (!local_types) {

                        return NULL;

                    }

                    BinaryenTypeExpand(local_type, local_types);

                    for (u32 ind = 0; ind < num_local_types; ind++) {
                        if (local_types[ind] == result && local_idx == (u32)-1) {
                            local_idx = el->local_idx; sub_idx = ind;
                            selected_type = local_type;
                        } else if (local_types[ind] == result && rand_below(afl, 2)) {
                            local_idx = el->local_idx; sub_idx = ind;
                            selected_type = local_type;
                        }
                    }

                    free(local_types);
                } else if (num_local_types == 1) {
                    if (local_type == result && local_idx == (u32)-1) {
                        local_idx = el->local_idx; sub_idx = (u32)-1;
                    } else if (local_type == result && rand_below(afl, 2)) {
                        local_idx = el->local_idx; sub_idx = (u32)-1;
                    }
                }
            });
        }

        if (sub_idx != (u32)-1) {
            BinaryenExpressionRef local_get = BinaryenLocalGet(mod, local_idx, selected_type);
            result_expr = BinaryenTupleExtract(mod, local_get, sub_idx);
        } else if (local_idx != (u32)-1) {
            result_expr = BinaryenLocalGet(mod, local_idx, result);
        }

    } else {
        u32 global_idx = (u32)-1, sub_idx = (u32)-1;
        u32 numGlobals = BinaryenGetNumGlobals(mod);

        if (num_results > 1) {
            for (u32 ind = 0; ind < numGlobals; ind++) {
                BinaryenGlobalRef cur_global = BinaryenGetGlobalByIndex(mod, ind);
                BinaryenType cur_type = BinaryenGlobalGetType(cur_global);

                if (cur_type == result && global_idx == (u32)-1) {
                    global_idx = ind;
                } else if (cur_type == result && rand_below(afl, 2)) {
                    global_idx = ind;
                }
            }

            if (global_idx != (u32)-1) {
                BinaryenGlobalRef target_global = BinaryenGetGlobalByIndex(mod, global_idx);
                const char* global_name = BinaryenGlobalGetName(target_global);
                result_expr = BinaryenGlobalGet(mod, global_name, result);
            }
            return result_expr;
        }

        for (u32 ind = 0; ind < numGlobals; ind++) {
            BinaryenGlobalRef cur_global = BinaryenGetGlobalByIndex(mod, ind);
            BinaryenType cur_type = BinaryenGlobalGetType(cur_global);

            u32 num_cur_types = BinaryenTypeArity(cur_type);
            if (num_cur_types > 1) {
                BinaryenType* cur_types = malloc(sizeof(BinaryenType) * num_cur_types);

                if (!cur_types) {

                    return NULL;

                }

                BinaryenTypeExpand(cur_type, cur_types);

                for (u32 type_idx = 0; type_idx < num_cur_types; type_idx++) {
                    if (cur_types[type_idx] == result && global_idx == (u32)-1) {
                        global_idx = ind; sub_idx = type_idx;
                    } else if (cur_types[type_idx] == result && rand_below(afl, 2)) {
                        global_idx = ind; sub_idx = type_idx;
                    }
                }

                free(cur_types);
            } else if (num_cur_types == 1) {
                if (cur_type == result && global_idx == (u32)-1) {
                    global_idx = ind; sub_idx = (u32)-1;
                } else if (cur_type == result && rand_below(afl, 2)) {
                    global_idx = ind; sub_idx = (u32)-1;
                }
            }
        }

        if (global_idx != (u32)-1) {
            BinaryenGlobalRef target_global = BinaryenGetGlobalByIndex(mod, global_idx);
            const char*  global_name = BinaryenGlobalGetName(target_global);
            BinaryenType global_type = BinaryenGlobalGetType(target_global);

            BinaryenExpressionRef global_get = BinaryenGlobalGet(mod, global_name, global_type);
            if (sub_idx != (u32)-1) {
                result_expr = BinaryenTupleExtract(mod, global_get, sub_idx);
            } else {
                result_expr = global_get;
            }
        }

    }

    return result_expr;

}

/*
    Construct one function call instruction with `result` result type,
    if can not find one such function, return NULL.
    @param data:        global wasm mutator state.
    @param mod:         global wasm module representation.
    @param result:      the needed output result type.
    @param local_cxt:   the local variables that are maintained during generation.
    @param label_cxt:   the labels that are introduced by structured control instructions.
    @param depth:       maximum recursion depth.
    @return:            the generated call instruction.
*/
BinaryenExpressionRef wasm_generate_one_call(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, list_t* local_cxt,
                                             list_t* label_cxt, s32 depth) {
    
    afl_state_t* afl = data->afl;
    BinaryenExpressionRef result_expr = NULL;

    // see if we can use one direct function call to get things done.
    u32 matches = 0;
    BinaryenFunctionRef* matched_funcs = NULL;
    matches = wasm_find_matching_funcs(mod, BinaryenTypeNone(), result, &matched_funcs, true);

    if (matches > 0) {

        u32 selected = rand_below(afl, matches);
        BinaryenFunctionRef target = matched_funcs[selected];
        const char* target_name = BinaryenFunctionGetName(target);

        BinaryenType target_param = BinaryenFunctionGetParams(target);
        u32 num_target_param = BinaryenTypeArity(target_param);
        if (num_target_param > 0) {

            BinaryenType* target_params = malloc(sizeof(BinaryenType) * num_target_param);
            BinaryenExpressionRef* target_params_expr =
                malloc(sizeof(BinaryenExpressionRef) * num_target_param);

            if (!target_params || !target_params_expr) {

                free(target_params);
                free(target_params_expr);
                free(matched_funcs);
                return NULL;

            }

            BinaryenTypeExpand(target_param, target_params);

            for (u32 ind = 0; ind < num_target_param; ind++) {
                target_params_expr[ind] = wasm_generate_instr_seqs_base_case(
                    data, mod, target_params[ind], local_cxt, label_cxt, depth - 1);
            }

            result_expr =
                BinaryenCall(mod, target_name, target_params_expr, num_target_param, result);

            free(target_params_expr);
            free(target_params);

        } else {

            result_expr = BinaryenCall(mod, target_name, NULL, num_target_param, result);

        }

    }

    free(matched_funcs);
    return result_expr;

}

/*
    Construct one call_indirect instruction with `result` result type.
    @param data:        global wasm mutator state.
    @param mod:         global wasm module representation.
    @param result:      the needed output result type.
    @param local_cxt:   the local variables that are maintained during generation.
    @param label_cxt:   the labels that are introduced by structured control instructions.
    @param depth:       maximum recursion depth.
    @return:            the generated call_indirect instruction.
*/
BinaryenExpressionRef wasm_generate_one_call_indirect(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, list_t* local_cxt,
                                                      list_t* label_cxt, s32 depth) {
    
    afl_state_t* afl = data->afl;
    BinaryenExpressionRef result_expr = NULL;

    // generate a call_indirect instruction, this may introduce run-time
    // error since we know nothing about the actual function being called.
    u32 num_params = rand_below(afl, 5) + 1;
    BinaryenType* params = malloc(sizeof(BinaryenType) * num_params);
    BinaryenExpressionRef* params_expr =
        malloc(sizeof(BinaryenExpressionRef) * num_params);

    if (!params || !params_expr) {

        free(params);
        free(params_expr);
        return NULL;

    }

    for (u32 ind = 0; ind < num_params; ind++) {

#ifndef DISABLE_FIXED_WIDTH_SIMD
        switch (rand_below(afl, 7)) {
#else
        switch (rand_below(afl, 6)) {
#endif
            case 0: params[ind] = BinaryenTypeInt32();     break;
            case 1: params[ind] = BinaryenTypeInt64();     break;
            case 2: params[ind] = BinaryenTypeFloat32();   break;
            case 3: params[ind] = BinaryenTypeFloat64();   break;
            case 4: params[ind] = BinaryenTypeFuncref();   break;
            case 5: params[ind] = BinaryenTypeExternref(); break;
#ifndef DISABLE_FIXED_WIDTH_SIMD
            case 6: params[ind] = BinaryenTypeVec128();    break;
#endif
        }

        params_expr[ind] = wasm_generate_instr_seqs_base_case(data, mod, params[ind], local_cxt,
                                                              label_cxt, depth - 1);

    }

    const char* table_name = NULL;
    u32 num_tables = BinaryenGetNumTables(mod);
    for (u32 ind = 0; ind < num_tables; ind++) {
        BinaryenTableRef cur_table = BinaryenGetTableByIndex(mod, ind);

        if (BinaryenTableGetType(cur_table) == BinaryenTypeFuncref()) {
            if (table_name == NULL)
                table_name = BinaryenTableGetName(cur_table);
            else if (rand_below(afl, 2))
                table_name = BinaryenTableGetName(cur_table);
        }
    }

    if (table_name == NULL) {

        u32 table_name_int = num_tables;
        int len = snprintf(NULL, 0, "%u", table_name_int);
        char* new_table_name = calloc(len + 1, sizeof(char));
        snprintf(new_table_name, len + 1, "%u", table_name_int);

        BinaryenTableRef new_table = BinaryenAddTable(mod, new_table_name, 0x10,
                                                      (BinaryenIndex)-1, BinaryenTypeFuncref());
        free(new_table_name);
        table_name = BinaryenTableGetName(new_table);

    }

    BinaryenType param = BinaryenTypeCreate(params, num_params);
    BinaryenExpressionRef index_expr = wasm_generate_instr_seqs_base_case(
        data, mod, BinaryenTypeInt32(), local_cxt, label_cxt, depth - 1);
    result_expr =
        BinaryenCallIndirect(mod, table_name, index_expr, params_expr, num_params, param, result);

    free(params_expr);
    free(params);

    return result_expr;

}

/*
    Construct one brand-new instruction given the information in
    instruction type, the corresponding child nodes are supplied
    by the `child` parameter.
    @param afl:       global afl state.
    @param mod:       global wasm module representation.
    @param new_instr: stores necessary information for construct-
                      ing a new instruction.
    @param child:     the corresponding child nodes.
    @param size:      the number of child nodes.
    @return:          newly-created Wasm instruction.
*/
BinaryenExpressionRef wasm_generate_one_simple_expr(afl_state_t* afl, BinaryenModuleRef mod, InstrTy new_instr, 
                                                    BinaryenExpressionRef* child, BinaryenIndex size) {

    BinaryenExpressionRef new_expr = NULL;
    BinaryenExpressionId  id = new_instr.expr_id;
    if (id == BinaryenLoadId()) {

        // if current module doesn't have any memory inside it, create one.
        if (!BinaryenHasMemory(mod)) wasm_set_memory(afl, mod, false);
        
        BinaryenExpressionRef child_expr = NULL;
        BinaryenIndex align_rand = 1 << rand_below(afl, (u32)log2((double)new_instr.mem_bytes) + 1);
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (child[ind] != NULL) {
                child_expr = child[ind];
            }
        }

        new_expr = BinaryenLoad(mod, new_instr.mem_bytes, new_instr.mem_signed, rand_below(afl, UINT32_MAX),
                                align_rand, new_instr.opd_type, child_expr, NULL);

    } else if (id == BinaryenStoreId()) {

        // if current module doesn't have any memory inside it, create one.
        if (!BinaryenHasMemory(mod)) wasm_set_memory(afl, mod, false);
        
        BinaryenExpressionRef ptr_child = NULL, val_child = NULL;
        BinaryenIndex align_rand = 1 << rand_below(afl, (u32)log2((double)new_instr.mem_bytes) + 1);
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (ptr_child == NULL && child[ind] != NULL) {
                ptr_child = child[ind];
            } else if (child[ind] != NULL) {
                val_child = child[ind];
            }
        }

        new_expr = BinaryenStore(mod, new_instr.mem_bytes, rand_below(afl, UINT32_MAX), align_rand, 
                                 ptr_child, val_child, new_instr.opd_type, NULL);

    } else if (id == BinaryenConstId()) {

        struct BinaryenLiteral lit_value;
        BinaryenType type = new_instr.opd_type;
        
        if (type == BinaryenTypeInt32()) {
            
            lit_value = BinaryenLiteralInt32((int32_t)rand_below(afl, UINT32_MAX));

        } else if (type == BinaryenTypeInt64()) {

            uint64_t value = ((uint64_t)rand_below(afl, UINT32_MAX) << 32) + rand_below(afl, UINT32_MAX);
            lit_value = BinaryenLiteralInt64((int64_t)value);
        
        } else if (type == BinaryenTypeFloat32()) {
        
            lit_value = BinaryenLiteralFloat32Bits((int32_t)rand_below(afl, UINT32_MAX));
        
        } else if (type == BinaryenTypeFloat64()) {
        
            uint64_t value = ((uint64_t)rand_below(afl, UINT32_MAX) << 32) + rand_below(afl, UINT32_MAX);
            lit_value = BinaryenLiteralFloat64Bits((int64_t)value);
        
        }
#ifndef DISABLE_FIXED_WIDTH_SIMD
        else if (type == BinaryenTypeVec128()) {
        
            uint8_t value_array[16];
            for (int i = 0; i < 16; i++)
                value_array[i] = rand_below(afl, UINT8_MAX + 1);
            
            lit_value = BinaryenLiteralVec128(value_array);

        }
#endif

        new_expr = BinaryenConst(mod, lit_value);

    } else if (id == BinaryenUnaryId()) {

        BinaryenExpressionRef child_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (child[ind] != NULL) {
                child_expr = child[ind];
            }
        }

        new_expr = BinaryenUnary(mod, new_instr.opt_type, child_expr);

    } else if (id == BinaryenBinaryId()) {

        BinaryenExpressionRef left_expr = NULL, right_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (left_expr == NULL && child[ind] != NULL) {
                left_expr  = child[ind];
            } else if (child[ind] != NULL) {
                right_expr = child[ind];
            }
        }

        new_expr = BinaryenBinary(mod, new_instr.opt_type, left_expr, right_expr);

    } else if (id == BinaryenSelectId()) {

        BinaryenExpressionRef cond_expr = NULL, true_expr = NULL, false_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (true_expr == NULL && child[ind] != NULL) {
                true_expr  = child[ind];
            } else if (false_expr == NULL && child[ind] != NULL) {
                false_expr = child[ind];
            } else if (child[ind] != NULL) {
                cond_expr  = child[ind];
            }
        }

        new_expr = BinaryenSelect(mod, cond_expr, true_expr, false_expr, new_instr.opd_type);

    } else if (id == BinaryenDropId()) {

        BinaryenExpressionRef child_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (child[ind] != NULL) {
                child_expr = child[ind];
            }
        }

        new_expr = BinaryenDrop(mod, child_expr);

    } else if (id == BinaryenMemorySizeId()) {

        // if current module doesn't have any memory inside it, create one.
        if (!BinaryenHasMemory(mod)) wasm_set_memory(afl, mod, false);
        
        new_expr = BinaryenMemorySize(mod, NULL, false);

    } else if (id == BinaryenMemoryGrowId()) {

        // if current module doesn't have any memory inside it, create one.
        if (!BinaryenHasMemory(mod)) wasm_set_memory(afl, mod, false);

        BinaryenExpressionRef child_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (child[ind] != NULL) {
                child_expr = child[ind];
            }
        }

        new_expr = BinaryenMemoryGrow(mod, child_expr, NULL, false);

    } else if (id == BinaryenNopId()) {

        new_expr = BinaryenNop(mod);

#ifndef DISABLE_FIXED_WIDTH_SIMD

    } else if (id == BinaryenSIMDExtractId()) {

        uint8_t lane_index;
        BinaryenOp opt = new_instr.opt_type;

        if (opt == BinaryenExtractLaneSVecI8x16() || opt == BinaryenExtractLaneUVecI8x16()) {
            lane_index = (uint8_t)rand_below(afl, 16);
        } else if (opt == BinaryenExtractLaneSVecI16x8() || opt == BinaryenExtractLaneUVecI16x8()) {
            lane_index = (uint8_t)rand_below(afl, 8);
        } else if (opt == BinaryenExtractLaneVecI32x4() || opt == BinaryenExtractLaneVecF32x4()) {
            lane_index = (uint8_t)rand_below(afl, 4);
        } else if (opt == BinaryenExtractLaneVecI64x2() || opt == BinaryenExtractLaneVecF64x2()) {
            lane_index = (uint8_t)rand_below(afl, 2);
        }

        BinaryenExpressionRef child_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (child[ind] != NULL) {
                child_expr = child[ind];
            }
        }

        new_expr = BinaryenSIMDExtract(mod, opt, child_expr, lane_index);

    } else if (id == BinaryenSIMDReplaceId()) {

        BinaryenType opd;
        uint8_t lane_index;
        BinaryenOp opt = new_instr.opt_type;

        if (opt == BinaryenReplaceLaneVecI8x16()) {

            opd = BinaryenTypeInt32();
            lane_index = (uint8_t)rand_below(afl, 16);

        } else if (opt == BinaryenReplaceLaneVecI16x8()) {

            opd = BinaryenTypeInt32();
            lane_index = (uint8_t)rand_below(afl, 8);

        } else if (opt == BinaryenReplaceLaneVecI32x4()) {

            opd = BinaryenTypeInt32();
            lane_index = (uint8_t)rand_below(afl, 4);

        } else if (opt == BinaryenReplaceLaneVecI64x2()) {

            opd = BinaryenTypeInt64();
            lane_index = (uint8_t)rand_below(afl, 2);

        } else if (opt == BinaryenReplaceLaneVecF32x4()) {

            opd = BinaryenTypeFloat32();
            lane_index = (uint8_t)rand_below(afl, 4);

        } else if (opt == BinaryenReplaceLaneVecF64x2()) {

            opd = BinaryenTypeFloat64();
            lane_index = (uint8_t)rand_below(afl, 2);

        }

        BinaryenExpressionRef vec_child = NULL, val_child = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (vec_child == NULL && child[ind] != NULL) {
                vec_child = child[ind];
            } else if (child[ind] != NULL) {
                val_child = child[ind];
            }
        }

        new_expr = BinaryenSIMDReplace(mod, opt, vec_child, lane_index, val_child);

    } else if (id == BinaryenSIMDShuffleId()) {

        uint8_t mask[16];
        for (int i = 0; i < 16; i++)
            mask[i] = (uint8_t)rand_below(afl, 32);
        
        BinaryenExpressionRef left_expr = NULL, right_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (left_expr == NULL && child[ind] != NULL) {
                left_expr  = child[ind];
            } else if (child[ind] != NULL) {
                right_expr = child[ind];
            }
        }

        new_expr = BinaryenSIMDShuffle(mod, left_expr, right_expr, mask);

    } else if (id == BinaryenSIMDTernaryId()) {

        BinaryenExpressionRef a_expr = NULL, b_expr = NULL, c_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (a_expr == NULL && child[ind] != NULL) {
                a_expr = child[ind];
            } else if (b_expr == NULL && child[ind] != NULL) {
                b_expr = child[ind];
            } else if (child[ind] != NULL) {
                c_expr = child[ind];
            }
        }

        new_expr = BinaryenSIMDTernary(mod, new_instr.opt_type, a_expr, b_expr, c_expr);

    } else if (id == BinaryenSIMDShiftId()) {

        BinaryenExpressionRef vec_expr = NULL, shift_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (vec_expr == NULL && child[ind] != NULL) {
                vec_expr   = child[ind];
            } else if (child[ind] != NULL) {
                shift_expr = child[ind];
            }
        }

        new_expr = BinaryenSIMDShift(mod, new_instr.opt_type, vec_expr, shift_expr);

    } else if (id == BinaryenSIMDLoadId()) {

        // if current module doesn't have any memory inside it, create one.
        if (!BinaryenHasMemory(mod)) wasm_set_memory(afl, mod, false);

        BinaryenIndex align_rand;
        BinaryenOp opt = new_instr.opt_type;
        if (opt == BinaryenLoad8SplatVec128()) {
            align_rand = 1 << rand_below(afl, 1);
        } else if (opt == BinaryenLoad16SplatVec128()) {
            align_rand = 1 << rand_below(afl, 2);
        } else if (opt == BinaryenLoad32SplatVec128() || opt == BinaryenLoad32ZeroVec128()) {
            align_rand = 1 << rand_below(afl, 3);
        } else if (opt == BinaryenLoad8x8SVec128() || opt == BinaryenLoad8x8UVec128() ||
                   opt == BinaryenLoad16x4SVec128() || opt == BinaryenLoad16x4UVec128() ||
                   opt == BinaryenLoad32x2SVec128() || opt == BinaryenLoad32x2UVec128() ||
                   opt == BinaryenLoad64SplatVec128() || opt == BinaryenLoad64ZeroVec128()) {
            align_rand = 1 << rand_below(afl, 4);
        }

        BinaryenExpressionRef child_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (child[ind] != NULL) {
                child_expr = child[ind];
            }
        }

        new_expr = BinaryenSIMDLoad(mod, opt, rand_below(afl, UINT32_MAX), align_rand, child_expr, NULL);

    } else if (id == BinaryenSIMDLoadStoreLaneId()) {

        // if current module doesn't have any memory inside it, create one.
        if (!BinaryenHasMemory(mod)) wasm_set_memory(afl, mod, false);
        
        uint8_t lane_index;
        BinaryenIndex align_rand;
        BinaryenOp opt = new_instr.opt_type;
        if (opt == BinaryenLoad8LaneVec128() || opt == BinaryenStore8LaneVec128()) {

            align_rand = 1 << rand_below(afl, 1);
            lane_index = (uint8_t)rand_below(afl, 16);

        } else if (opt == BinaryenLoad16LaneVec128() || opt == BinaryenStore16LaneVec128()) {

            align_rand = 1 << rand_below(afl, 2);
            lane_index = (uint8_t)rand_below(afl, 8);

        } else if (opt == BinaryenLoad32LaneVec128() || opt == BinaryenStore32LaneVec128()) {

            align_rand = 1 << rand_below(afl, 3);
            lane_index = (uint8_t)rand_below(afl, 4);

        } else if (opt == BinaryenLoad64LaneVec128() || opt == BinaryenStore64LaneVec128()) {

            align_rand = 1 << rand_below(afl, 4);
            lane_index = (uint8_t)rand_below(afl, 2);

        }

        BinaryenExpressionRef ptr_expr = NULL, vec_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (ptr_expr == NULL && child[ind] != NULL) {
                ptr_expr = child[ind];
            } else if (child[ind] != NULL) {
                vec_expr = child[ind];
            }
        }

        new_expr = BinaryenSIMDLoadStoreLane(mod, opt, rand_below(afl, UINT32_MAX), align_rand, 
                                             lane_index, ptr_expr, vec_expr, NULL);

#endif

    } else if (id == BinaryenMemoryInitId()) {

        // if current module doesn't have any memory or data segment, create one.
        if (!BinaryenHasMemory(mod) || !BinaryenGetNumMemorySegments(mod))
            wasm_set_memory(afl, mod, true);

        BinaryenIndex seg_ind = rand_below(afl, BinaryenGetNumMemorySegments(mod));
        int len = snprintf(NULL, 0, "%u", seg_ind);
        char* seg_name = calloc(len + 1, sizeof(char));
        snprintf(seg_name, len + 1, "%u", seg_ind);

        BinaryenExpressionRef dst_expr = NULL, off_expr = NULL, size_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (dst_expr == NULL && child[ind] != NULL) {
                dst_expr  = child[ind];
            } else if (off_expr == NULL && child[ind] != NULL) {
                off_expr  = child[ind];
            } else if (child[ind] != NULL) {
                size_expr = child[ind];
            }
        }

        new_expr = BinaryenMemoryInit(mod, seg_name, dst_expr, off_expr, size_expr, NULL);
        free(seg_name);

    } else if (id == BinaryenDataDropId()) {

        // if current module doesn't have any data segments, create some.
        if (!BinaryenGetNumMemorySegments(mod)) wasm_set_memory(afl, mod, true);

        BinaryenIndex seg_ind = rand_below(afl, BinaryenGetNumMemorySegments(mod));
        int len = snprintf(NULL, 0, "%u", seg_ind);
        char* seg_name = calloc(len + 1, sizeof(char));
        snprintf(seg_name, len + 1, "%u", seg_ind);

        new_expr = BinaryenDataDrop(mod, seg_name);
        free(seg_name);

    } else if (id == BinaryenMemoryCopyId()) {

        // if current module doesn't have any memory inside it, create one.
        if (!BinaryenHasMemory(mod)) wasm_set_memory(afl, mod, false);
        
        BinaryenExpressionRef dst_expr = NULL, src_expr = NULL, size_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (dst_expr == NULL && child[ind] != NULL) {
                dst_expr  = child[ind];
            } else if (src_expr == NULL && child[ind] != NULL) {
                src_expr  = child[ind];
            } else if (child[ind] != NULL) {
                size_expr = child[ind];
            }
        }

        new_expr = BinaryenMemoryCopy(mod, dst_expr, src_expr, size_expr, NULL, NULL);

    } else if (id == BinaryenMemoryFillId()) {

        // if current module doesn't have any memory inside it, create one.
        if (!BinaryenHasMemory(mod)) wasm_set_memory(afl, mod, false);

        BinaryenExpressionRef dst_expr = NULL, val_expr = NULL, size_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (dst_expr == NULL && child[ind] != NULL) {
                dst_expr  = child[ind];
            } else if (val_expr == NULL && child[ind] != NULL) {
                val_expr  = child[ind];
            } else if (child[ind] != NULL) {
                size_expr = child[ind];
            }
        }

        new_expr = BinaryenMemoryFill(mod, dst_expr, val_expr, size_expr, NULL);

    } else if (id == BinaryenRefNullId()) {

        BinaryenType type = new_instr.opd_type;
        if (type == BinaryenTypeFuncref()) {
            type = BinaryenTypeNullFuncref();
        } else if (type == BinaryenTypeExternref()) {
            type = BinaryenTypeNullExternref();
        }

        new_expr = BinaryenRefNull(mod, type);

    } else if (id == BinaryenRefIsNullId()) {

        BinaryenExpressionRef child_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (child[ind] != NULL) {
                child_expr = child[ind];
            }
        }

        new_expr = BinaryenRefIsNull(mod, child_expr);

    } else if (id == BinaryenRefFuncId()) {

        // Binaryen IR can be worked on without needing to think about 
        // declaring function references.
        const char* func_name = NULL;
        BinaryenIndex func_idx = rand_below(afl, BinaryenGetNumFunctions(mod));
        BinaryenFunctionRef func = BinaryenGetFunctionByIndex(mod, func_idx);
        
        func_name = BinaryenFunctionGetName(func);
        new_expr = BinaryenRefFunc(mod, func_name, BinaryenTypeFuncref());

    } else if (id == BinaryenTableGetId()) {

        BinaryenTableRef target_table = NULL;
        for (BinaryenIndex ind = 0; ind < BinaryenGetNumTables(mod); ind++) {
            
            BinaryenTableRef cur_table = BinaryenGetTableByIndex(mod, ind);
            BinaryenType table_type = BinaryenTableGetType(cur_table);

            if (target_table == NULL && table_type == new_instr.opd_type) {
                target_table = cur_table;
            } else if (table_type == new_instr.opd_type && rand_below(afl, 2)) {
                target_table = cur_table;
            }

        }

        // if we could not find a matching table, create one.
        if (target_table == NULL) {

            // come up with a table name.
            BinaryenIndex table_name_int = BinaryenGetNumTables(mod);
            int len = snprintf(NULL, 0, "%u", table_name_int);
            char* table_name = calloc(len + 1, sizeof(char));
            snprintf(table_name, len + 1, "%u", table_name_int);

            target_table = BinaryenAddTable(mod, table_name, 0x10, 
                                            (BinaryenIndex)-1, new_instr.opd_type);
            free(table_name);

        }

        BinaryenExpressionRef child_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (child[ind] != NULL) {
                child_expr = child[ind];
            }
        }

        new_expr = BinaryenTableGet(mod, BinaryenTableGetName(target_table), 
                                    child_expr, new_instr.opd_type);

    } else if (id == BinaryenTableSetId()) {

        BinaryenTableRef target_table = NULL;
        for (BinaryenIndex ind = 0; ind < BinaryenGetNumTables(mod); ind++) {
            
            BinaryenTableRef cur_table = BinaryenGetTableByIndex(mod, ind);
            BinaryenType table_type = BinaryenTableGetType(cur_table);

            if (target_table == NULL && table_type == new_instr.opd_type) {
                target_table = cur_table;
            } else if (table_type == new_instr.opd_type && rand_below(afl, 2)) {
                target_table = cur_table;
            }

        }

        // if we could not find a matching table, create one.
        if (target_table == NULL) {

            // come up with a table name.
            BinaryenIndex table_name_int = BinaryenGetNumTables(mod);
            int len = snprintf(NULL, 0, "%u", table_name_int);
            char* table_name = calloc(len + 1, sizeof(char));
            snprintf(table_name, len + 1, "%u", table_name_int);

            target_table = BinaryenAddTable(mod, table_name, 0x10, 
                                            (BinaryenIndex)-1, new_instr.opd_type);
            free(table_name);

        }

        BinaryenExpressionRef ind_expr = NULL, val_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (ind_expr == NULL && child[ind] != NULL) {
                ind_expr = child[ind];
            } else if (child[ind] != NULL) {
                val_expr = child[ind];
            }
        }

        new_expr = BinaryenTableSet(mod, BinaryenTableGetName(target_table), 
                                    ind_expr, val_expr);

    } else if (id == BinaryenTableSizeId()) {

        BinaryenTableRef target_table = NULL;
        if (!BinaryenGetNumTables(mod)) {

            // come up with a table name.
            BinaryenIndex table_name_int = BinaryenGetNumTables(mod);
            int len = snprintf(NULL, 0, "%u", table_name_int);
            char* table_name = calloc(len + 1, sizeof(char));
            snprintf(table_name, len + 1, "%u", table_name_int);

            BinaryenType table_type = rand_below(afl, 2) ? BinaryenTypeFuncref() : BinaryenTypeExternref();
            target_table = BinaryenAddTable(mod, table_name, 0x10, 
                                            (BinaryenIndex)-1, table_type);
            free(table_name);

        } else {

            BinaryenIndex table_idx = rand_below(afl, BinaryenGetNumTables(mod));
            target_table = BinaryenGetTableByIndex(mod, table_idx);

        }

        new_expr = BinaryenTableSize(mod, BinaryenTableGetName(target_table));

    } else if (id == BinaryenTableGrowId()) {

        BinaryenTableRef target_table = NULL;
        for (BinaryenIndex ind = 0; ind < BinaryenGetNumTables(mod); ind++) {
            
            BinaryenTableRef cur_table = BinaryenGetTableByIndex(mod, ind);
            BinaryenType table_type = BinaryenTableGetType(cur_table);

            if (target_table == NULL && table_type == new_instr.opd_type) {
                target_table = cur_table;
            } else if (table_type == new_instr.opd_type && rand_below(afl, 2)) {
                target_table = cur_table;
            }

        }

        // if we could not find a matching table, create one.
        if (target_table == NULL) {

            // come up with a table name.
            BinaryenIndex table_name_int = BinaryenGetNumTables(mod);
            int len = snprintf(NULL, 0, "%u", table_name_int);
            char* table_name = calloc(len + 1, sizeof(char));
            snprintf(table_name, len + 1, "%u", table_name_int);

            target_table = BinaryenAddTable(mod, table_name, 0x10, 
                                            (BinaryenIndex)-1, new_instr.opd_type);
            free(table_name);

        }

        BinaryenExpressionRef val_expr = NULL, del_expr = NULL;
        for (BinaryenIndex ind = 0; ind < size; ind++) {
            if (val_expr == NULL && child[ind] != NULL) {
                val_expr = child[ind];
            } else if (child[ind] != NULL) {
                del_expr = child[ind];
            }
        }

        new_expr = BinaryenTableGrow(mod, BinaryenTableGetName(target_table), 
                                     val_expr, del_expr);

    }

    return new_expr;

}
