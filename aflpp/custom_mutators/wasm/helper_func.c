#include "mutator_func.h"
#include "safe_alloc.h"

/* Binaryen can represent multi-value types. In fuzzing scenarios, a malformed
   module might lead to unexpectedly large arities; avoid VLAs and guard
   allocations. */
static int wasm_alloc_type_vec(BinaryenIndex n, BinaryenType **out) {

    *out = NULL;
    if (n == 0) return 1;
    if ((size_t)n > SIZE_MAX / sizeof(BinaryenType)) return 0;
    *out = (BinaryenType *)malloc(sizeof(BinaryenType) * (size_t)n);
    return (*out != NULL);

}

static int wasm_alloc_expr_vec(BinaryenIndex n, BinaryenExpressionRef **out) {

    *out = NULL;
    if (n == 0) return 1;
    if ((size_t)n > SIZE_MAX / sizeof(BinaryenExpressionRef)) return 0;
    *out = (BinaryenExpressionRef *)malloc(sizeof(BinaryenExpressionRef) * (size_t)n);
    return (*out != NULL);

}

/*
    Given an integer array, perform a random shuffle using the Fisher-Yates
    algorithm.
    @param afl: global afl state.
    @param arr: original array waited for shuffling.
    @param n:   size of the array.
*/
void fisher_yates_shuffle(afl_state_t* afl, u32 arr[], size_t n) {

    /* Nothing to shuffle for arrays of size 0 or 1. Avoids n == 0 underflow. */
    if (n <= 1) return;

    for (u32 i = n - 1; i > 0; i--) {

        u32 j = rand_below(afl, i + 1);

        u32 temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;

    }

}


/*
    Given a ExpressionRef AST node, retrieve its ExpressionRef children list.
    @param parent: parent AST node.
    @param child:  children AST nodes, the caller function is responsible for 
                   passing in a pointer to BinaryenExpressionRef* type and free
                   memory allocated in this function.
    @return:       the numbder of children expression node.
*/
BinaryenIndex wasm_get_children_expr(BinaryenExpressionRef parent, BinaryenExpressionRef** child) {

    /* Defensive check: if parent is NULL, return 0 children to prevent
       crashes in the caller. This can happen when a function has no body
       or when child nodes are legitimately NULL (e.g., optional else branch). */
    if (!parent) {
        *child = NULL;
        return 0;
    }

    BinaryenIndex numChilds = 1;
    BinaryenExpressionId id = BinaryenExpressionGetId(parent);

    if (id == BinaryenBlockId()) {

        numChilds    = BinaryenBlockGetNumChildren(parent);
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child) && numChilds > 0) return 0;
        for (BinaryenIndex ind = 0; ind < numChilds; ind++)
            (*child)[ind] = BinaryenBlockGetChildAt(parent, ind);

    } else if (id == BinaryenIfId()) {

        numChilds    = 3;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenIfGetCondition(parent);
        (*child)[1]  = BinaryenIfGetIfTrue(parent);
        (*child)[2]  = BinaryenIfGetIfFalse(parent);

    } else if (id == BinaryenLoopId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenLoopGetBody(parent);

    } else if (id == BinaryenBreakId()) {

        numChilds    = 2;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenBreakGetValue(parent);
        (*child)[1]  = BinaryenBreakGetCondition(parent);

    } else if (id == BinaryenSwitchId()) {

        numChilds    = 2;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenSwitchGetValue(parent);
        (*child)[1]  = BinaryenSwitchGetCondition(parent);

    } else if (id == BinaryenCallId()) {

        numChilds    = BinaryenCallGetNumOperands(parent);
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child) && numChilds > 0) return 0;
        for (BinaryenIndex ind = 0; ind < numChilds; ind++)
            (*child)[ind] = BinaryenCallGetOperandAt(parent, ind);

    } else if (id == BinaryenCallIndirectId()) {

        numChilds    = BinaryenCallIndirectGetNumOperands(parent) + 1;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child) && numChilds > 0) return 0;
        for (BinaryenIndex ind = 0; ind < numChilds - 1; ind++)
            (*child)[ind] = BinaryenCallIndirectGetOperandAt(parent, ind);
        
        (*child)[numChilds - 1] = BinaryenCallIndirectGetTarget(parent);

    } else if (id == BinaryenLocalSetId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenLocalSetGetValue(parent);

    } else if (id == BinaryenGlobalSetId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenGlobalSetGetValue(parent);

    } else if (id == BinaryenLoadId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenLoadGetPtr(parent);

    } else if (id == BinaryenStoreId()) {

        numChilds    = 2;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenStoreGetPtr(parent);
        (*child)[1]  = BinaryenStoreGetValue(parent);

    } else if (id == BinaryenUnaryId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenUnaryGetValue(parent);

    } else if (id == BinaryenBinaryId()) {

        numChilds    = 2;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenBinaryGetLeft(parent);
        (*child)[1]  = BinaryenBinaryGetRight(parent);

    } else if (id == BinaryenSelectId()) {

        numChilds    = 3;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenSelectGetIfTrue(parent);
        (*child)[1]  = BinaryenSelectGetIfFalse(parent);
        (*child)[2]  = BinaryenSelectGetCondition(parent);

    } else if (id == BinaryenDropId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenDropGetValue(parent);

    } else if (id == BinaryenReturnId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenReturnGetValue(parent);

    } else if (id == BinaryenMemoryGrowId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenMemoryGrowGetDelta(parent);

#ifndef DISABLE_FIXED_WIDTH_SIMD

    } else if (id == BinaryenSIMDExtractId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenSIMDExtractGetVec(parent);

    } else if (id == BinaryenSIMDReplaceId()) {

        numChilds    = 2;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenSIMDReplaceGetVec(parent);
        (*child)[1]  = BinaryenSIMDReplaceGetValue(parent);

    } else if (id == BinaryenSIMDShuffleId()) {

        numChilds    = 2;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenSIMDShuffleGetLeft(parent);
        (*child)[1]  = BinaryenSIMDShuffleGetRight(parent);

    } else if (id == BinaryenSIMDTernaryId()) {

        numChilds    = 3;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenSIMDTernaryGetA(parent);
        (*child)[1]  = BinaryenSIMDTernaryGetB(parent);
        (*child)[2]  = BinaryenSIMDTernaryGetC(parent);

    } else if (id == BinaryenSIMDShiftId()) {

        numChilds    = 2;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenSIMDShiftGetVec(parent);
        (*child)[1]  = BinaryenSIMDShiftGetShift(parent);

    } else if (id == BinaryenSIMDLoadId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenSIMDLoadGetPtr(parent);

    } else if (id == BinaryenSIMDLoadStoreLaneId()) {

        numChilds    = 2;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenSIMDLoadStoreLaneGetPtr(parent);
        (*child)[1]  = BinaryenSIMDLoadStoreLaneGetVec(parent);

#endif

    } else if (id == BinaryenMemoryInitId()) {

        numChilds    = 3;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenMemoryInitGetDest(parent);
        (*child)[1]  = BinaryenMemoryInitGetOffset(parent);
        (*child)[2]  = BinaryenMemoryInitGetSize(parent);

    } else if (id == BinaryenMemoryCopyId()) {

        numChilds    = 3;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenMemoryCopyGetDest(parent);
        (*child)[1]  = BinaryenMemoryCopyGetSource(parent);
        (*child)[2]  = BinaryenMemoryCopyGetSize(parent);

    } else if (id == BinaryenMemoryFillId()) {

        numChilds    = 3;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenMemoryFillGetDest(parent);
        (*child)[1]  = BinaryenMemoryFillGetValue(parent);
        (*child)[2]  = BinaryenMemoryFillGetSize(parent);

    } else if (id == BinaryenRefIsNullId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenRefIsNullGetValue(parent);

    } else if (id == BinaryenTableGetId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenTableGetGetIndex(parent);

    } else if (id == BinaryenTableSetId()) {

        numChilds    = 2;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenTableSetGetIndex(parent);
        (*child)[1]  = BinaryenTableSetGetValue(parent);

    } else if (id == BinaryenTableGrowId()) {

        numChilds    = 2;
        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenTableGrowGetValue(parent);
        (*child)[1]  = BinaryenTableGrowGetDelta(parent);

    } else if (id == BinaryenTupleMakeId()) {

        numChilds   = BinaryenTupleMakeGetNumOperands(parent);
        (*child)    = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child) && numChilds > 0) return 0;
        for (BinaryenIndex ind = 0; ind < numChilds; ind++)
            (*child)[ind] = BinaryenTupleMakeGetOperandAt(parent, ind);

    } else if (id == BinaryenTupleExtractId()) {

        (*child)     = calloc(numChilds, sizeof(BinaryenExpressionRef));
        if (!(*child)) return 0;
        (*child)[0]  = BinaryenTupleExtractGetTuple(parent);

    } else {

        numChilds = 0;

    }

    return numChilds;

}

/*
    Change the child node (specified by index) of parent node to new child node.
    @param parent:    the parent node whose child shall be replaced.
    @param new_child: new child node that are replacing in.
    @param index:     index specifies which child is substituted.
*/
void wasm_set_children_expr(BinaryenExpressionRef parent, BinaryenExpressionRef new_child, BinaryenIndex index) {

    BinaryenExpressionId id = BinaryenExpressionGetId(parent);

    if (id == BinaryenBlockId()) {

        BinaryenBlockSetChildAt(parent, index, new_child);

    } else if (id == BinaryenIfId()) {

        switch (index) {
            case 0: BinaryenIfSetCondition(parent, new_child); break;
            case 1: BinaryenIfSetIfTrue(parent, new_child);    break;
            case 2: BinaryenIfSetIfFalse(parent, new_child);   break;
        }

    } else if (id == BinaryenLoopId()) {

        BinaryenLoopSetBody(parent, new_child);

    } else if (id == BinaryenBreakId()) {

        switch (index) {
            case 0: BinaryenBreakSetValue(parent, new_child);     break;
            case 1: BinaryenBreakSetCondition(parent, new_child); break;
        }

    } else if (id == BinaryenSwitchId()) {

        switch (index) {
            case 0: BinaryenSwitchSetValue(parent, new_child);     break;
            case 1: BinaryenSwitchSetCondition(parent, new_child); break;
        }

    } else if (id == BinaryenCallId()) {

        BinaryenCallSetOperandAt(parent, index, new_child);

    } else if (id == BinaryenCallIndirectId()) {

        BinaryenIndex numOperands = BinaryenCallIndirectGetNumOperands(parent);

        if (index < numOperands) {
            BinaryenCallIndirectSetOperandAt(parent, index, new_child);
        } else {
            BinaryenCallIndirectSetTarget(parent, new_child);
        }

    } else if (id == BinaryenLocalSetId()) {

        BinaryenLocalSetSetValue(parent, new_child);

    } else if (id == BinaryenGlobalSetId()) {

        BinaryenGlobalSetSetValue(parent, new_child);

    } else if (id == BinaryenLoadId()) {

        BinaryenLoadSetPtr(parent, new_child);

    } else if (id == BinaryenStoreId()) {

        switch (index) {
            case 0: BinaryenStoreSetPtr(parent, new_child);   break;
            case 1: BinaryenStoreSetValue(parent, new_child); break;
        }

    } else if (id == BinaryenUnaryId()) {

        BinaryenUnarySetValue(parent, new_child);

    } else if (id == BinaryenBinaryId()) {

        switch (index) {
            case 0: BinaryenBinarySetLeft(parent, new_child);  break;
            case 1: BinaryenBinarySetRight(parent, new_child); break;
        }

    } else if (id == BinaryenSelectId()) {

        switch (index) {
            case 0: BinaryenSelectSetIfTrue(parent, new_child);    break;
            case 1: BinaryenSelectSetIfFalse(parent, new_child);   break;
            case 2: BinaryenSelectSetCondition(parent, new_child); break;
        }

    } else if (id == BinaryenDropId()) {

        BinaryenDropSetValue(parent, new_child);

    } else if (id == BinaryenReturnId()) {

        BinaryenReturnSetValue(parent, new_child);

    } else if (id == BinaryenMemoryGrowId()) {

        BinaryenMemoryGrowSetDelta(parent, new_child);

#ifndef DISABLE_FIXED_WIDTH_SIMD

    } else if (id == BinaryenSIMDExtractId()) {

        BinaryenSIMDExtractSetVec(parent, new_child);

    } else if (id == BinaryenSIMDReplaceId()) {

        switch (index) {
            case 0: BinaryenSIMDReplaceSetVec(parent, new_child);   break;
            case 1: BinaryenSIMDReplaceSetValue(parent, new_child); break;
        }

    } else if (id == BinaryenSIMDShuffleId()) {

        switch (index) {
            case 0: BinaryenSIMDShuffleSetLeft(parent, new_child);  break;
            case 1: BinaryenSIMDShuffleSetRight(parent, new_child); break;
        }

    } else if (id == BinaryenSIMDTernaryId()) {

        switch (index) {
            case 0: BinaryenSIMDTernarySetA(parent, new_child); break;
            case 1: BinaryenSIMDTernarySetB(parent, new_child); break;
            case 2: BinaryenSIMDTernarySetC(parent, new_child); break;
        }

    } else if (id == BinaryenSIMDShiftId()) {

        switch (index) {
            case 0: BinaryenSIMDShiftSetVec(parent, new_child);   break;
            case 1: BinaryenSIMDShiftSetShift(parent, new_child); break;
        }

    } else if (id == BinaryenSIMDLoadId()) {

        BinaryenSIMDLoadSetPtr(parent, new_child);

    } else if (id == BinaryenSIMDLoadStoreLaneId()) {

        switch (index) {
            case 0: BinaryenSIMDLoadStoreLaneSetPtr(parent, new_child); break;
            case 1: BinaryenSIMDLoadStoreLaneSetVec(parent, new_child); break;
        }

#endif

    } else if (id == BinaryenMemoryInitId()) {

        switch (index) {
            case 0: BinaryenMemoryInitSetDest(parent, new_child);   break;
            case 1: BinaryenMemoryInitSetOffset(parent, new_child); break;
            case 2: BinaryenMemoryInitSetSize(parent, new_child);   break;
        }

    } else if (id == BinaryenMemoryCopyId()) {

        switch (index) {
            case 0: BinaryenMemoryCopySetDest(parent, new_child);   break;
            case 1: BinaryenMemoryCopySetSource(parent, new_child); break;
            case 2: BinaryenMemoryCopySetSize(parent, new_child);   break;
        }

    } else if (id == BinaryenMemoryFillId()) {

        switch (index) {
            case 0: BinaryenMemoryFillSetDest(parent, new_child);  break;
            case 1: BinaryenMemoryFillSetValue(parent, new_child); break;
            case 2: BinaryenMemoryFillSetSize(parent, new_child);  break;
        }

    } else if (id == BinaryenRefIsNullId()) {

        BinaryenRefIsNullSetValue(parent, new_child);

    } else if (id == BinaryenTableGetId()) {

        BinaryenTableGetSetIndex(parent, new_child);

    } else if (id == BinaryenTableSetId()) {

        switch (index) {
            case 0: BinaryenTableSetSetIndex(parent, new_child); break;
            case 1: BinaryenTableSetSetValue(parent, new_child); break;
        }

    } else if (id == BinaryenTableGrowId()) {

        switch (index) {
            case 0: BinaryenTableGrowSetValue(parent, new_child); break;
            case 1: BinaryenTableGrowSetDelta(parent, new_child); break;
        }

    } else if (id == BinaryenTupleMakeId()) {

        BinaryenTupleMakeSetOperandAt(parent, index, new_child);

    } else if (id == BinaryenTupleExtractId()) {

        BinaryenTupleExtractSetTuple(parent, new_child);

    }

}

/*
    Given a BinaryenExpressionRef, infer its corresponding instruction type.
    @param expr: the binaryen expression that we want to infer.
    @return:     inferred instruction type represented as InstrTy structure.  
*/
InstrTy wasm_get_instr_type(BinaryenExpressionRef expr) {

    InstrTy instr_ty;
    memset(&instr_ty, 0, sizeof(InstrTy));
    BinaryenType ty = BinaryenExpressionGetType(expr);
    BinaryenExpressionId id = BinaryenExpressionGetId(expr);
    
    /* need to handle each expression kind, a little bit tedious */
    instr_ty.expr_id = id;
    if (id == BinaryenLoadId()) {

        instr_ty.union_type = 0;
        instr_ty.opd_type   = ty;
        instr_ty.mem_bytes  = (uint8_t)BinaryenLoadGetBytes(expr);
        instr_ty.mem_signed = (uint8_t)BinaryenLoadIsSigned(expr);

    } else if (id == BinaryenStoreId()) {

        instr_ty.union_type = 0;
        instr_ty.opd_type   = BinaryenStoreGetValueType(expr);
        instr_ty.mem_bytes  = (uint8_t)BinaryenStoreGetBytes(expr);

    } else if (id == BinaryenUnaryId()) {

        instr_ty.union_type = 1;
        instr_ty.opt_type   = BinaryenUnaryGetOp(expr);

    } else if (id == BinaryenBinaryId()) {

        instr_ty.union_type = 1;
        instr_ty.opt_type   = BinaryenBinaryGetOp(expr);

    } else if (id == BinaryenDropId()) {

        instr_ty.union_type = 0;
        instr_ty.opd_type   = wasm_get_single_type(BinaryenDropGetValue(expr));

#ifndef DISABLE_FIXED_WIDTH_SIMD

    } else if (id == BinaryenSIMDExtractId()) {

        instr_ty.union_type = 1;
        instr_ty.opt_type   = BinaryenSIMDExtractGetOp(expr);

    } else if (id == BinaryenSIMDReplaceId()) {

        instr_ty.union_type = 1;
        instr_ty.opt_type   = BinaryenSIMDReplaceGetOp(expr);

    } else if (id == BinaryenSIMDTernaryId()) {

        instr_ty.union_type = 1;
        instr_ty.opt_type   = BinaryenSIMDTernaryGetOp(expr);

    } else if (id == BinaryenSIMDShiftId()) {

        instr_ty.union_type = 1;
        instr_ty.opt_type   = BinaryenSIMDShiftGetOp(expr);

    } else if (id == BinaryenSIMDLoadId()) {

        instr_ty.union_type = 1;
        instr_ty.opt_type   = BinaryenSIMDLoadGetOp(expr);

    } else if (id == BinaryenSIMDLoadStoreLaneId()) {

        instr_ty.union_type = 1;
        instr_ty.opt_type   = BinaryenSIMDLoadStoreLaneGetOp(expr);

#endif

    } else if (id == BinaryenRefFuncId()) {

        instr_ty.union_type = 0;
        instr_ty.opd_type   = BinaryenTypeFuncref();

    } else if (id == BinaryenRefIsNullId()) {

        instr_ty.union_type = 0;
        instr_ty.opd_type   = wasm_get_single_type(BinaryenRefIsNullGetValue(expr));

    } else if (id == BinaryenTableSetId()) {

        instr_ty.union_type = 0;
        instr_ty.opd_type   = wasm_get_single_type(BinaryenTableSetGetValue(expr));

    } else if (id == BinaryenTableGrowId()) {

        instr_ty.union_type = 0;
        instr_ty.opd_type   = wasm_get_single_type(BinaryenTableGrowGetValue(expr));

    } else if (id == BinaryenRefNullId()) {

        instr_ty.union_type = 0;
        instr_ty.opd_type   = (BinaryenExpressionGetType(expr) == BinaryenTypeNullFuncref() ?
                               BinaryenTypeFuncref() : BinaryenTypeExternref());

    } else {

        instr_ty.union_type = 0;
        instr_ty.opd_type   = BinaryenExpressionGetType(expr);

    }

    instr_ty.hash_value = djb2_hash((u8*)&instr_ty, sizeof(InstrTy));
    return instr_ty;

}

/*
    Given an instruction type, find out which instruction group it 
    belongs to.
    @param data:  where instruction groups reside.
    @param instr: target instruction type.
    @return:      the index of corresponding instruction group, if
                  there is no matching group then return -1.
*/
int32_t wasm_find_instr_grps(wasm_mutator_t* data, InstrTy instr) {

    InstrGrp* instr_grp = data->instr_grps;
    uint32_t  num_grps  = data->num_instr_grps;
    int32_t   index_grp = -1;

    for (int32_t i = 0; i < num_grps; i++) {

        InstrGrp cur_grp = instr_grp[i];
        for (int32_t j = 0; j < cur_grp.num_inst; j++) {

            InstrTy cur_inst = cur_grp.groups[j];
            if (instr.hash_value == cur_inst.hash_value) {
                index_grp = i;
                goto final;
            }

        }

    }

final:
    return index_grp;

}

/*
    Due to the extra complexity introduced by typed function references
    proposal, we have to manually figure out whether a reference type is
    funcref or not. This function only works for singular type, not for
    composite type (and is somewhat ad-hoc).
    @param expr: the Binaryen expression whose type info is needed
    @return:     the corresponding Binaryen type
*/
BinaryenType wasm_get_single_type(BinaryenExpressionRef expr) {

    if (expr == NULL) return BinaryenTypeNone();

    BinaryenType type = BinaryenExpressionGetType(expr);

    if (type == BinaryenTypeNone() || type == BinaryenTypeInt32() ||
        type == BinaryenTypeInt64() || type == BinaryenTypeFloat32() ||
        type == BinaryenTypeFloat64() || type == BinaryenTypeVec128() ||
        type == BinaryenTypeUnreachable())
        return type;

    if (type == BinaryenTypeExternref() || type == BinaryenTypeNullExternref())
        return BinaryenTypeExternref();
    
    return BinaryenTypeFuncref();

}

/*
    Substitute the corresponding fields in the original expression with
    infos from the new instruction type (may need adjustment)
    @param expr:      target expression whose fields need update.
    @param new_instr: instruction type which contains necessary infos
                      for the update.
*/
void wasm_substitute_expr_fields(afl_state_t* afl, BinaryenExpressionRef expr, InstrTy new_instr) {

    BinaryenExpressionId id = BinaryenExpressionGetId(expr);

    if (id == BinaryenUnaryId()) {

        BinaryenUnarySetOp(expr, new_instr.opt_type);

    } else if (id == BinaryenBinaryId()) {

        BinaryenBinarySetOp(expr, new_instr.opt_type);

#ifndef DISABLE_FIXED_WIDTH_SIMD

    } else if (id == BinaryenSIMDExtractId()) {

        uint8_t lane_index = 0;
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

        BinaryenSIMDExtractSetOp(expr, opt);
        BinaryenSIMDExtractSetIndex(expr, lane_index);

    } else if (id == BinaryenSIMDReplaceId()) {

        uint8_t lane_index = 0;
        BinaryenOp opt = new_instr.opt_type;

        if (opt == BinaryenReplaceLaneVecI8x16()) {
            lane_index = (uint8_t)rand_below(afl, 16);
        } else if (opt == BinaryenReplaceLaneVecI16x8()) {
            lane_index = (uint8_t)rand_below(afl, 8);
        } else if (opt == BinaryenReplaceLaneVecI32x4() || opt == BinaryenReplaceLaneVecF32x4()) {
            lane_index = (uint8_t)rand_below(afl, 4);
        } else if (opt == BinaryenReplaceLaneVecI64x2() || opt == BinaryenReplaceLaneVecF64x2()) {
            lane_index = (uint8_t)rand_below(afl, 2);
        }

        BinaryenSIMDReplaceSetOp(expr, opt);
        BinaryenSIMDReplaceSetIndex(expr, lane_index);

    } else if (id == BinaryenSIMDTernaryId()) {

        BinaryenSIMDTernarySetOp(expr, new_instr.opt_type);

    } else if (id == BinaryenSIMDShiftId()) {

        BinaryenSIMDShiftSetOp(expr, new_instr.opt_type);

    } else if (id == BinaryenSIMDLoadId()) {

        BinaryenIndex align_rand = 0;
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

        BinaryenSIMDLoadSetOp(expr, opt);
        BinaryenSIMDLoadSetAlign(expr, align_rand);

    } else if (id == BinaryenSIMDLoadStoreLaneId()) {

        uint8_t lane_index = 0;
        BinaryenIndex align_rand = 0;
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

        BinaryenSIMDLoadStoreLaneSetOp(expr, opt);
        BinaryenSIMDLoadStoreLaneSetAlign(expr, align_rand);
        BinaryenSIMDLoadStoreLaneSetIndex(expr, lane_index);

#endif

    } else if (id == BinaryenLoadId()) {

        BinaryenIndex align_rand = 1 << rand_below(afl, (u32)log2((double)new_instr.mem_bytes) + 1);

        BinaryenLoadSetAlign(expr, align_rand);
        BinaryenLoadSetSigned(expr, (bool)new_instr.mem_signed);
        BinaryenLoadSetBytes(expr, (uint32_t)new_instr.mem_bytes);

    } else if (id == BinaryenStoreId()) {

        BinaryenIndex align_rand = 1 << rand_below(afl, (u32)log2((double)new_instr.mem_bytes) + 1);

        BinaryenStoreSetAlign(expr, align_rand);
        BinaryenStoreSetBytes(expr, (uint32_t)new_instr.mem_bytes);

    }

}

/*
    Find functions inside module that have function signature (param) -> (result), and
    save all such functions into `funcs`.
    @param mod:          global wasm module representation.
    @param param:        the needed input parameter type.
    @param result:       the needed output result type.
    @param funcs:        matching funcs, the caller function is responsible for passing in a
                         pointer to BinaryenFunctionRef* type and free memory allocated in this
                         function.
    @param ignore_param: whether ignore matching the parameter type.
    @return:       the number of matching functions.
*/
BinaryenIndex wasm_find_matching_funcs(BinaryenModuleRef mod, BinaryenType param, BinaryenType result,
                                       BinaryenFunctionRef** funcs, bool ignore_param) {

    BinaryenIndex matches = 0;

    BinaryenIndex numFuncs = BinaryenGetNumFunctions(mod);
    for (BinaryenIndex ind = 0; ind < numFuncs; ind++) {

        BinaryenFunctionRef func   = BinaryenGetFunctionByIndex(mod, ind);
        BinaryenType param_input   = wasm_simplify_type(BinaryenFunctionGetParams(func));
        BinaryenType result_output = wasm_simplify_type(BinaryenFunctionGetResults(func));

        if ((ignore_param || (param_input == param)) && (result_output == result)) {
            BinaryenIndex new_matches = matches + 1;
            if (!safe_realloc((void**)funcs, sizeof(BinaryenFunctionRef) * new_matches)) {
                /* Allocation failure: stop collecting further matches, but keep
                   the ones we already have. */
                break;
            }
            (*funcs)[matches] = func;
            matches = new_matches;
        }

    }

    return matches;

}

/*
    Construct one function satisfying the function signature (param) -> (result) and add it
    into current module.
    @param data:   global mutator state.
    @param mod:    global wasm module representation.
    @param param:  the needed input parameter type.
    @param result: the needed output result type.
    @return:       name of the newly-constructed function.

*/
const char* wasm_generate_one_func(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType param, BinaryenType result) {

    list_t local_cxt, label_cxt;
    memset(&local_cxt, 0, sizeof(list_t));
    memset(&label_cxt, 0, sizeof(list_t));

    // parameters are passed to function in the form of locals, so we maintain them
    // inside local variable context.
    u32 num_params = BinaryenTypeArity(param);
    if (num_params >= 1) {

        BinaryenType *params = NULL;
        if (!wasm_alloc_type_vec(num_params, &params)) {
            return NULL;
        }
        BinaryenTypeExpand(param, params);
    
        for (u32 ind = 0; ind < num_params; ind++) {

            localCxt* cur_local = calloc(1, sizeof(localCxt));
            if (!cur_local) {
                free(params);
                return NULL;
            }
            cur_local->local_idx  = ind;
            cur_local->pre_added  = true;
            cur_local->local_type = params[ind];

            list_append(&local_cxt, cur_local);

        }
        free(params);

    }

    // generate function body.
    BinaryenExpressionRef func_body = wasm_generate_instr_seqs_top_level(data, mod, result, 
                                                                         &local_cxt, &label_cxt, 3);

    // generate function name.
    u32 func_ind = BinaryenGetNumFunctions(mod);
    int len = snprintf(NULL, 0, "%u", func_ind);
    char* func_name = calloc(len + 1, sizeof(char));
    snprintf(func_name, len + 1, "%u", func_ind);

    // the type definition of extra locals are needed when calling `BinaryenAddFunction`.
    BinaryenFunctionRef new_func = NULL;
    s32 num_vars = local_cxt.element_total_count - num_params;
    if (num_vars >= 1) {

        BinaryenType *vars = NULL;
        if (!wasm_alloc_type_vec(num_vars, &vars)) {
            if (local_cxt.element_total_count) LIST_FOREACH_CLEAR(&local_cxt, localCxt, {free(el);});
            if (label_cxt.element_total_count) LIST_FOREACH_CLEAR(&label_cxt, labelCxt, {free(el);});
            free(func_name);
            return NULL;
        }
        LIST_FOREACH(&local_cxt, localCxt, {
            if (!el->pre_added) vars[el->local_idx - num_params] = el->local_type; });

        new_func = BinaryenAddFunction(mod, func_name, param, result, vars, num_vars, func_body);
        free(vars);

    } else {

        new_func = BinaryenAddFunction(mod, func_name, param, result, NULL, 0, func_body);

    }

    if (local_cxt.element_total_count) LIST_FOREACH_CLEAR(&local_cxt, localCxt, {free(el);});
    if (label_cxt.element_total_count) LIST_FOREACH_CLEAR(&label_cxt, labelCxt, {free(el);});
    free(func_name);

    return BinaryenFunctionGetName(new_func);

}

/*
    Simplify the concretized representation of funcref and externref type in Binaryen.
    @param original: rather specific type representation in Binaryen.
    @return:         simplified type representation.
*/
BinaryenType wasm_simplify_type(BinaryenType original) {

    if (original == BinaryenTypeUnreachable())
        return original;

    BinaryenIndex size = BinaryenTypeArity(original);
    if (size > 0) {
        BinaryenType *ori_types = NULL;
        BinaryenType *new_types = NULL;
        if (!wasm_alloc_type_vec(size, &ori_types) ||
            !wasm_alloc_type_vec(size, &new_types)) {
            free(ori_types);
            free(new_types);
            return BinaryenTypeNone();
        }
        BinaryenTypeExpand(original, ori_types);

        for (BinaryenIndex ind = 0; ind < size; ind++) {
            BinaryenType type = ori_types[ind];

            if (type == BinaryenTypeInt32() || type == BinaryenTypeInt64() || type == BinaryenTypeVec128() ||
                type == BinaryenTypeFloat32() || type == BinaryenTypeFloat64()) {
                new_types[ind] = type;
            } else if (type == BinaryenTypeExternref() || type == BinaryenTypeNullExternref()) {
                new_types[ind] = BinaryenTypeExternref();
            } else {
                new_types[ind] = BinaryenTypeFuncref();
            }
        }

        BinaryenType ret = BinaryenTypeCreate(new_types, size);
        free(ori_types);
        free(new_types);
        return ret;
    } else {
        return BinaryenTypeNone();
    }

}

/*
    Given a target sub-expression and the expression containing it, find all available
    branch targets and store them into list.
    @param container: high-level expression containing the target.
    @param target:    target sub-expression.
    @param label_cxt: all available branch targets.
    @return:          whether find the target expression.
*/
bool wasm_find_available_labels(BinaryenExpressionRef container, BinaryenExpressionRef target, list_t* label_cxt) {

    if (container == target) return true;

    labelCxt* cur_cxt = NULL;
    BinaryenType type = BinaryenExpressionGetType(container);
    BinaryenExpressionId id = BinaryenExpressionGetId(container);

    if (id == BinaryenBlockId() || id == BinaryenLoopId()) {

        const char* label_name;
        BinaryenType label_type;
        if (id == BinaryenLoopId()) {
            label_name = BinaryenLoopGetName(container);
            label_type = BinaryenTypeNone();
        } else {
            label_name = BinaryenBlockGetName(container);
            label_type = wasm_simplify_type(type);
        }
        
        if (label_name && label_type != BinaryenTypeUnreachable()) {
            cur_cxt = calloc(1, sizeof(labelCxt));
            cur_cxt->pre_added  = true;
            cur_cxt->label_name = label_name;
            cur_cxt->label_type = label_type;

            list_append(label_cxt, cur_cxt);
        }

    }
    
    bool found = false;
    BinaryenIndex size = 0;
    BinaryenExpressionRef* child = NULL;
    size = wasm_get_children_expr(container, &child);

    for (BinaryenIndex ind = 0; ind < size; ind++) {
        if (child[ind] && wasm_find_available_labels(child[ind], target, label_cxt)) {
            found = true;
            break;
        }
    }

    if (!found && cur_cxt) {
        list_remove(label_cxt, (void*)cur_cxt);
        free(cur_cxt);
    }

    free(child);
    return found;

}

/*
    Given a function, we consider all its local variables as available even if
    they may haven't been properly initialized (at the time of using them).
    @param func:      the function that local variables reside in.
    @param local_cxt: all available local variables.
*/
void wasm_find_available_locals(BinaryenFunctionRef func, list_t* local_cxt) {

    BinaryenType param = BinaryenFunctionGetParams(func);
    BinaryenIndex param_size = BinaryenTypeArity(param);
    if (param_size > 0) {
        BinaryenType *params = NULL;
        if (!wasm_alloc_type_vec(param_size, &params)) {
            return;
        }
        BinaryenTypeExpand(param, params);

        for (BinaryenIndex ind = 0; ind < param_size; ind++) {
            
            localCxt* cur_cxt = calloc(1, sizeof(localCxt));
            if (!cur_cxt) {
                free(params);
                return;
            }
            cur_cxt->pre_added  = true;
            cur_cxt->local_type = wasm_simplify_type(params[ind]);
            cur_cxt->local_idx  = local_cxt->element_total_count;
            
            list_append(local_cxt, cur_cxt);

        }
        free(params);
    }

    BinaryenIndex var_size = BinaryenFunctionGetNumVars(func);
    for (BinaryenIndex ind = 0; ind < var_size; ind++) {
        BinaryenType var_type = BinaryenFunctionGetVar(func, ind);

        localCxt* cur_cxt = calloc(1, sizeof(localCxt));
        cur_cxt->pre_added  = true;
        cur_cxt->local_type = wasm_simplify_type(var_type);
        cur_cxt->local_idx  = local_cxt->element_total_count;
        
        list_append(local_cxt, cur_cxt);
    }

}

/*
    Given a Wasm module, decompose it into code fragments, where fragments
    with same stack type are grouped together.
    @param mod:           wasm module that needs fragmentation.
    @param fragment_pool: collected code fragments.
*/
void wasm_fragmentize_module(BinaryenModuleRef mod, list_t* fragment_pool) {

    list_t child_node_list;
    memset(&child_node_list, 0, sizeof(list_t));

    BinaryenIndex numFuncs = BinaryenGetNumFunctions(mod);
    for (BinaryenIndex i = 0; i < numFuncs; i++) {

        BinaryenFunctionRef   func = BinaryenGetFunctionByIndex(mod, i);
        BinaryenExpressionRef body = BinaryenFunctionGetBody(func);

        list_append(&child_node_list, body);
        LIST_FOREACH(&child_node_list, struct BinaryenExpression, {

            // TODO: add a blacklist that excludes expressions that
            // seems useless (e.g. local.get) from fragment pool.

            BinaryenType ty = BinaryenExpressionGetType(el);
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

            Frag fragment;
            fragment.func = func; fragment.expr = el;
            if (group != NULL) {

                // if we already have a fragment group with type `ty`, insert current
                // expression into this group.
                if (!SAFE_REALLOC_ARRAY(group->frags, group->num_frags, Frag)) {
                    /* Allocation failure: skip adding this fragment but keep
                       previously collected ones. */
                    continue;
                }
                group->frags[group->num_frags++] = fragment;

            } else {

                // otherwise create a new fragment group.
                group = calloc(1, sizeof(fragGrp));
                group->num_frags = 1;
                group->frag_type = ty;
                group->frags = calloc(1, sizeof(Frag));
                group->frags[0] = fragment;

                list_append(fragment_pool, group);

            }

            BinaryenIndex size = 0;
            BinaryenExpressionRef* child = NULL;
            size = wasm_get_children_expr(el, &child);

            for (BinaryenIndex ind = 0; ind < size; ind++)
                if (child[ind]) list_append(&child_node_list, child[ind]);
            
            if (el_box->next != next)
                next = el_box->next;
            
            LIST_REMOVE_CURRENT_EL_IN_FOREACH();
            free(child);

        });

    }

}

/*
    In splicing mutation, the new expression that splices in may impose
    additional context requirements. This function is intended to fixup
    "holes" so that resulting module is still valid.
    @param data:     global mutator state.
    @param cur_mod:  current module being mutated.
    @param spl_mod:  the module that current one is splicing with.
    @param current:  the current fragment being mutated.
    @param target:   the new fragment that splices in.
    @return:         whether the fixup succeed or not.
*/
bool wasm_splicing_fixup(wasm_mutator_t* data, BinaryenModuleRef cur_mod, BinaryenModuleRef spl_mod,
                         Frag* current, Frag* target) {

    bool success = true;
    afl_state_t* afl = data->afl;
    bool skipped = (current->expr == target->expr && current->func == target->func) ? true : false;

    list_t child_node_list;
    memset(&child_node_list, 0, sizeof(list_t));

    // prepare information related to splicing module/function.
    BinaryenFunctionRef   spl_func = target->func;
    BinaryenExpressionRef spl_body = BinaryenFunctionGetBody(spl_func);
    BinaryenIndex   num_locals_spl = BinaryenFunctionGetNumLocals(spl_func);
    BinaryenIndex   num_params_spl = num_locals_spl - BinaryenFunctionGetNumVars(spl_func);

    BinaryenType* params_spl = NULL;
    if (num_params_spl) {
        params_spl = calloc(num_params_spl, sizeof(BinaryenType));
        BinaryenTypeExpand(BinaryenFunctionGetParams(spl_func), params_spl);
    }

    // prepare information related to current module/function.
    BinaryenFunctionRef   cur_func = current->func;
    BinaryenExpressionRef cur_expr = current->expr;
    
    list_t locals_cur;
    memset(&locals_cur, 0, sizeof(list_t));
    wasm_find_available_locals(cur_func, &locals_cur);

    list_t labels_cur;
    memset(&labels_cur, 0, sizeof(list_t));
    wasm_find_available_labels(BinaryenFunctionGetBody(cur_func), cur_expr, &labels_cur);

    // Binaryen uses name instead of index to refer branch tar-
    // gets. To prevent name conflicts, we re-write and update
    // branch target names.

    /* CRITICAL FIX (Bug #1): Copy target->expr BEFORE modifying it.
       target->expr belongs to spl_mod, and modifying it directly would
       corrupt spl_mod's internal state. We must copy it to cur_mod first,
       then modify only the copy. This prevents cross-module corruption. */
    BinaryenExpressionRef copied_expr = BinaryenExpressionCopy(target->expr, cur_mod);
    if (!copied_expr) {
        success = false;
        goto final;
    }
    /* From this point on, target->expr always refers to the cloned
       subtree that lives in cur_mod. We must never mutate the original
       spl_mod expression tree to avoid cross-module corruption. */
    target->expr = copied_expr;

    BinaryenIndex num_br = 0;
    BinaryenType* br_type = NULL;
    bool* br_type_valid = NULL;
    BinaryenExpressionRef* br_inst = NULL;
    list_append(&child_node_list, target->expr);
    LIST_FOREACH(&child_node_list, struct BinaryenExpression, {

        BinaryenExpressionId id = BinaryenExpressionGetId(el);

        if (id == BinaryenBreakId() || id == BinaryenSwitchId()) {
            if (!SAFE_REALLOC_ARRAY(br_inst, num_br, BinaryenExpressionRef) ||
                !SAFE_REALLOC_ARRAY(br_type, num_br, BinaryenType) ||
                !SAFE_REALLOC_ARRAY(br_type_valid, num_br, bool)) {
                success = false;
                goto final;
            }
            br_inst[num_br] = el;
            br_type_valid[num_br] = false;

            list_t labels_spl;
            memset(&labels_spl, 0, sizeof(list_t));
            wasm_find_available_labels(spl_body, el, &labels_spl);

            const char* label_name = (id == BinaryenBreakId()) ? BinaryenBreakGetName(el) :
                                                                 BinaryenSwitchGetDefaultName(el);
            LIST_FOREACH(&labels_spl, labelCxt, {
                if (!strcmp(el->label_name, label_name)) {
                    br_type[num_br] = el->label_type;
                    br_type_valid[num_br] = true;
                    break;
                }
            })

            LIST_FOREACH_CLEAR(&labels_spl, labelCxt, {free(el);});
            num_br++;
        }

        BinaryenIndex size = 0;
        BinaryenExpressionRef* child = NULL;
        size = wasm_get_children_expr(el, &child);

        for (BinaryenIndex ind = 0; ind < size; ind++)
            if (child[ind]) list_append(&child_node_list, child[ind]);
        
        if (el_box->next != next)
            next = el_box->next;
        
        LIST_REMOVE_CURRENT_EL_IN_FOREACH();
        free(child);

    })

    list_append(&child_node_list, target->expr);
    LIST_FOREACH(&child_node_list, struct BinaryenExpression, {

        BinaryenExpressionId id = BinaryenExpressionGetId(el);

        if (id == BinaryenBlockId() || id == BinaryenLoopId()) {
            const char* cur_name = (id == BinaryenBlockId()) ? BinaryenBlockGetName(el) :
                                                               BinaryenLoopGetName(el);
            char* new_name = wasm_find_available_name(data);

            if (cur_name != NULL) {
                for (BinaryenIndex i = 0; i < num_br; i++) {
                    BinaryenExpressionId br_id = BinaryenExpressionGetId(br_inst[i]);
                    
                    if (br_id == BinaryenBreakId()) {
                        const char* br_name = BinaryenBreakGetName(br_inst[i]);
                        if (!strcmp(br_name, cur_name)) BinaryenBreakSetName(br_inst[i], new_name);
                    } else {
                        for (BinaryenIndex ind = 0; ind < BinaryenSwitchGetNumNames(br_inst[i]); ind++) {
                            const char* br_name = BinaryenSwitchGetNameAt(br_inst[i], ind);
                            if (!strcmp(br_name, cur_name)) BinaryenSwitchSetNameAt(br_inst[i], ind, new_name);
                        }

                        if (!strcmp(BinaryenSwitchGetDefaultName(br_inst[i]), cur_name))
                            BinaryenSwitchSetDefaultName(br_inst[i], new_name);
                    }
                }
            }

            if (id == BinaryenBlockId())
                BinaryenBlockSetName(el, new_name);
            else
                BinaryenLoopSetName(el, new_name);
        }

        BinaryenIndex size = 0;
        BinaryenExpressionRef* child = NULL;
        size = wasm_get_children_expr(el, &child);

        for (BinaryenIndex ind = 0; ind < size; ind++)
            if (child[ind]) list_append(&child_node_list, child[ind]);
        
        if (el_box->next != next)
            next = el_box->next;
        
        LIST_REMOVE_CURRENT_EL_IN_FOREACH();
        free(child);

    })

    /* At this point target->expr already points to copied_expr in cur_mod. */
    if (skipped) goto skip_check;

    // we only consider one situation as fixup failure, that is
    // we cannot find a matching branch target for control-flow
    // transfer instructions.
    // therefore, we determine whether we can have a successful
    // fixup first.

    list_append(&child_node_list, target->expr);
    LIST_FOREACH(&child_node_list, struct BinaryenExpression, {

        BinaryenExpressionId id = BinaryenExpressionGetId(el);

        // fixup for br, br_if and br_table instruction.
        if (id == BinaryenBreakId() || id == BinaryenSwitchId()) {
            // find out target label's stack type.
            BinaryenType label_type;
            bool         label_type_found = false;
            const char*  label_name  = (id == BinaryenBreakId()) ?
                                       BinaryenBreakGetName(el) : BinaryenSwitchGetDefaultName(el);
            
            for (BinaryenIndex i = 0; i < num_br; i++) {
                /* Skip entries where we failed to recover a label type in the
                   first pass to avoid reading uninitialized br_type[i]. */
                if (!br_type_valid || !br_type_valid[i]) continue;

                BinaryenExpressionId br_id = BinaryenExpressionGetId(br_inst[i]);

                if (br_id == BinaryenBreakId()) {
                    if (!strcmp(BinaryenBreakGetName(br_inst[i]), label_name)) {
                        label_type = br_type[i];
                        label_type_found = true;
                        break;
                    }
                } else {
                    if (!strcmp(BinaryenSwitchGetDefaultName(br_inst[i]), label_name)) {
                        label_type = br_type[i];
                        label_type_found = true;
                        break;
                    }
                }
            }

            /* If we cannot recover the label's stack type, we cannot safely
               rewrite this branch. Treat this as a fixup failure to avoid
               producing an inconsistent module. */
            if (!label_type_found) {
                success = false;
                break;
            }

            // find out labels with matching stack type.
            u32 num_matches = 0;
            const char** match_names = NULL;

            list_t labels_spl;
            memset(&labels_spl, 0, sizeof(list_t));
            wasm_find_available_labels(target->expr, el, &labels_spl);

            if (labels_spl.element_total_count) {
                LIST_FOREACH(&labels_spl, labelCxt, {
                    if (el->label_type == label_type) {
                        if (!SAFE_REALLOC_ARRAY(match_names, num_matches, const char*)) {
                            /* Allocation failure: stop collecting more matches from
                               this list, but keep the ones we already have. */
                            break;
                        }
                        match_names[num_matches++] = el->label_name;
                    }
                });
                LIST_FOREACH_CLEAR(&labels_spl, labelCxt, {free(el);});
            }

            if (labels_cur.element_total_count) {
                LIST_FOREACH(&labels_cur, labelCxt, {
                    if (el->label_type == label_type) {
                        if (!SAFE_REALLOC_ARRAY(match_names, num_matches, const char*)) {
                            /* Allocation failure: stop collecting matches from the
                               current module, but keep existing ones. */
                            break;
                        }
                        match_names[num_matches++] = el->label_name;
                    }
                });
            }

            // if no matches are found, fixup fails and early exit.
            if (num_matches == 0) {
                success = false;
                break;
            }

            if (id == BinaryenBreakId()) {
                u32 selected = rand_below(afl, num_matches);
                BinaryenBreakSetName(el, match_names[selected]);
            } else {
                for (u32 ind = 0; ind < BinaryenSwitchGetNumNames(el); ind++) {
                    u32 selected = rand_below(afl, num_matches);
                    BinaryenSwitchSetNameAt(el, ind, match_names[selected]);
                }

                u32 selected = rand_below(afl, num_matches);
                BinaryenSwitchSetDefaultName(el, match_names[selected]);
            }

            free(match_names);
        }

        // fixup for return instruction.
        if (id == BinaryenReturnId()) {
            BinaryenType result_cur = wasm_simplify_type(BinaryenFunctionGetResults(cur_func));
            BinaryenType result_spl = wasm_simplify_type(BinaryenFunctionGetResults(spl_func));

            if (result_cur != result_spl) {
                success = false;
                break;
            }
        }

        BinaryenIndex size = 0;
        BinaryenExpressionRef* child = NULL;
        size = wasm_get_children_expr(el, &child);

        for (BinaryenIndex ind = 0; ind < size; ind++)
            if (child[ind]) list_append(&child_node_list, child[ind]);

        if (el_box->next != next)
            next = el_box->next;
        
        LIST_REMOVE_CURRENT_EL_IN_FOREACH();
        free(child);

    });

    // if fixup fails, early exit.
    if (child_node_list.element_prealloc_buf[0].next) {
        LIST_FOREACH_CLEAR(&child_node_list, struct BinaryenExpression, {});
    }
    if (!success) goto final; 

skip_check:
    // if above preliminary checks pass, perform further detailed fixup.
    list_append(&child_node_list, target->expr);
    LIST_FOREACH(&child_node_list, struct BinaryenExpression, {

        BinaryenExpressionId id = BinaryenExpressionGetId(el);

        // fixup for ref.func instruction.
        if (id == BinaryenRefFuncId()) {
            BinaryenIndex selected = rand_below(afl, BinaryenGetNumFunctions(cur_mod));

            BinaryenFunctionRef func = BinaryenGetFunctionByIndex(cur_mod, selected);
            BinaryenRefFuncSetFunc(el, BinaryenFunctionGetName(func));
        }

        // fixup for local.get and local.set instruction.
        if ((id == BinaryenLocalGetId() || id == BinaryenLocalSetId()) && !skipped) {
            BinaryenType local_type;
            BinaryenIndex local_idx = (id == BinaryenLocalGetId()) ? 
                                      BinaryenLocalGetGetIndex(el) : BinaryenLocalSetGetIndex(el);

            if (local_idx < num_params_spl)
                local_type = params_spl[local_idx];
            else
                local_type = BinaryenFunctionGetVar(spl_func, local_idx - num_params_spl);
            
            // find out if current function has a local variable with matching type.
            BinaryenIndex match_idx = (u32)-1;
            local_type = wasm_simplify_type(local_type);
            if (locals_cur.element_total_count > 0) {
                LIST_FOREACH(&locals_cur, localCxt, {
                    if (el->local_type == local_type && (match_idx == (u32)-1 || rand_below(afl, 2))) {
                        match_idx = el->local_idx;
                    }
                });
            }

            // if unfortunately we cannot find such variable, add one.
            if (match_idx == (u32)-1) {
                match_idx = BinaryenFunctionAddVar(cur_func, local_type);

                localCxt* new_local   = calloc(1, sizeof(localCxt));
                new_local->pre_added  = false;
                new_local->local_idx  = match_idx;
                new_local->local_type = local_type;

                list_append(&locals_cur, new_local);
            }

            if (id == BinaryenLocalGetId())
                BinaryenLocalGetSetIndex(el, match_idx);
            else
                BinaryenLocalSetSetIndex(el, match_idx);
        }

        // fixup for global.get and global.set instruction.
        if (id == BinaryenGlobalGetId() || id == BinaryenGlobalSetId()) {
            const char* global_name  = (id == BinaryenGlobalGetId()) ?
                                       BinaryenGlobalGetGetName(el) : BinaryenGlobalSetGetName(el);
            if (!global_name) {
                success = false;
                goto final;
            }
            
            BinaryenGlobalRef global = BinaryenGetGlobal(spl_mod, global_name);
            if (!global) {
                success = false;
                goto final;
            }
            BinaryenType global_type = BinaryenGlobalGetType(global);
            global_type = wasm_simplify_type(global_type);

            // find out if current module has a global variable with matching type.
            const char* match_name = NULL;
            for (BinaryenIndex i = 0; i < BinaryenGetNumGlobals(cur_mod); i++) {
                BinaryenGlobalRef cur = BinaryenGetGlobalByIndex(cur_mod, i);
                BinaryenType cur_type = BinaryenGlobalGetType(cur);

                cur_type = wasm_simplify_type(cur_type);
                if (cur_type == global_type && (id == BinaryenGlobalGetId() || BinaryenGlobalIsMutable(cur))
                                            && (match_name == NULL || rand_below(afl, 2))) {
                    match_name = BinaryenGlobalGetName(cur);
                }
            }

            // if unfortunately we cannot find such variable, add one.
            if (match_name == NULL) {
                u32 new_global_idx = BinaryenGetNumGlobals(cur_mod);
                int len = snprintf(NULL, 0, "%u", new_global_idx);
                char* new_global_name = calloc(len + 1, sizeof(char));
                snprintf(new_global_name, len + 1, "%u", new_global_idx);

                // randomly generate initialization expression.
                BinaryenIndex num_global_type = BinaryenTypeArity(global_type);
                BinaryenType *global_types = NULL;
                BinaryenExpressionRef *init_exprs = NULL;
                if (!wasm_alloc_type_vec(num_global_type, &global_types) ||
                    !wasm_alloc_expr_vec(num_global_type, &init_exprs)) {
                    free(global_types);
                    free(init_exprs);
                    free(new_global_name);
                    success = false;
                    goto final;
                }
                BinaryenTypeExpand(global_type, global_types);

                for (BinaryenIndex i = 0; i < num_global_type; i++) {
                    InstrTy init_inst; memset(&init_inst, 0, sizeof(InstrTy));

                    if (global_types[i] == BinaryenTypeExternref()) {
                        FILL_INSTR_TYPE_OPD(init_inst, BinaryenRefNullId(), global_types[i]);
                    } else if (global_types[i] == BinaryenTypeFuncref()) {
                        FILL_INSTR_TYPE_OPD(init_inst, rand_below(afl, 2) ?
                                            BinaryenRefNullId() : BinaryenRefFuncId(), global_types[i]);
                    } else {
                        FILL_INSTR_TYPE_OPD(init_inst, BinaryenConstId(), global_types[i]);
                    }

                    init_exprs[i] = wasm_generate_one_simple_expr(afl, cur_mod, init_inst, NULL, 0);
                }

                BinaryenExpressionRef init = NULL;
                if (num_global_type > 1)
                    init = BinaryenTupleMake(cur_mod, init_exprs, num_global_type);
                else
                    init = init_exprs[0];

                BinaryenGlobalRef new_global = BinaryenAddGlobal(cur_mod, new_global_name, global_type, true, init);

                match_name = BinaryenGlobalGetName(new_global);
                free(global_types);
                free(init_exprs);
                free(new_global_name);
            }

            if (id == BinaryenGlobalGetId())
                BinaryenGlobalGetSetName(el, match_name);
            else
                BinaryenGlobalSetSetName(el, match_name);
        }

        // fixup for table.get, table.set and table.grow instruction.
        if (id == BinaryenTableGetId() || id == BinaryenTableSetId() || id == BinaryenTableGrowId()) {
            const char* table_name = NULL;
            if (id == BinaryenTableGetId())
                table_name = BinaryenTableGetGetTable(el);
            else if (id == BinaryenTableSetId())
                table_name = BinaryenTableSetGetTable(el);
            else
                table_name = BinaryenTableGrowGetTable(el);
            if (!table_name) {
                success = false;
                goto final;
            }
            
            BinaryenTableRef  table = BinaryenGetTable(spl_mod, table_name);
            if (!table) {
                success = false;
                goto final;
            }
            BinaryenType table_type = BinaryenTableGetType(table);
            table_type = wasm_simplify_type(table_type);
            
            // find out if current module has a table with matching type.
            const char* match_name = NULL;
            for (BinaryenIndex i = 0; i < BinaryenGetNumTables(cur_mod); i++) {
                BinaryenTableRef  cur = BinaryenGetTableByIndex(cur_mod, i);
                BinaryenType cur_type = BinaryenTableGetType(cur); 
                cur_type = wasm_simplify_type(cur_type);

                if (cur_type == table_type && (match_name == NULL || rand_below(afl, 2))) {
                    match_name = BinaryenTableGetName(cur);
                }
            }

            // if unfortunately we cannot find such table, add one.
            if (match_name == NULL) {
                u32 new_table_int = BinaryenGetNumTables(cur_mod);
                int len = snprintf(NULL, 0, "%u", new_table_int);
                char* new_table_name = calloc(len + 1, sizeof(char));
                snprintf(new_table_name, len + 1, "%u", new_table_int);

                BinaryenTableRef new_table = BinaryenAddTable(cur_mod, new_table_name, 0x10,
                                                              (BinaryenIndex)-1, table_type);
                match_name = BinaryenTableGetName(new_table);
                free(new_table_name);
            }

            if (id == BinaryenTableGetId())
                BinaryenTableGetSetTable(el, match_name);
            else if (id == BinaryenTableSetId())
                BinaryenTableSetSetTable(el, match_name);
            else
                BinaryenTableGrowSetTable(el, match_name);
        }

        // fixup for table.size instruction.
        if (id == BinaryenTableSizeId()) {
            // if current module does not have any table, add one.
            if (!BinaryenGetNumTables(cur_mod)) {
                u32 new_table_int = 0;
                int len = snprintf(NULL, 0, "%u", new_table_int);
                char* new_table_name = calloc(len + 1, sizeof(char));
                snprintf(new_table_name, len + 1, "%u", new_table_int);

                BinaryenAddTable(cur_mod, new_table_name, 0x10, (BinaryenIndex)-1, BinaryenTypeFuncref());
                free(new_table_name);
            }

            u32 selected = rand_below(afl, BinaryenGetNumTables(cur_mod));
            BinaryenTableRef table = BinaryenGetTableByIndex(cur_mod, selected);
            BinaryenTableSizeSetTable(el, BinaryenTableGetName(table));
        }

        // fixup for t.load, t.loadN_sx, t.store, t.storeN, v128.loadNxM_sx, v128.loadN_splat,
        // v128.loadN_zero, v128.loadN_lane, v128.storeN_lane, memory.size, memory.grow,
        // memory.fill and memory.copy instruction.
        if (id == BinaryenLoadId() || id == BinaryenStoreId() || id == BinaryenSIMDLoadId() ||
            id == BinaryenSIMDLoadStoreLaneId() || id == BinaryenMemorySizeId() || id == BinaryenMemoryGrowId() ||
            id == BinaryenMemoryFillId() || id == BinaryenMemoryCopyId()) {

            if (!BinaryenHasMemory(cur_mod)) wasm_set_memory(afl, cur_mod, false);

        }

        // fixup for memory.init and data.drop instruction.
        if (id == BinaryenMemoryInitId() || id == BinaryenDataDropId()) {
            // if current module does not have any memory or data segment, create one.
            if ((id == BinaryenMemoryInitId() && !BinaryenHasMemory(cur_mod))
                                              || !BinaryenGetNumMemorySegments(cur_mod))
                wasm_set_memory(afl, cur_mod, true);

            BinaryenIndex seg_ind = rand_below(afl, BinaryenGetNumMemorySegments(cur_mod));
            int len = snprintf(NULL, 0, "%u", seg_ind);
            char* seg_name = calloc(len + 1, sizeof(char));
            snprintf(seg_name, len + 1, "%u", seg_ind);

            if (id == BinaryenMemoryInitId())
                BinaryenMemoryInitSetSegment(el, seg_name);
            else
                BinaryenDataDropSetSegment(el, seg_name);

            free(seg_name);
        }

        // fixup for call_indirect instruction.
        if (id == BinaryenCallIndirectId()) {
            const char* match_name = NULL;

            for (BinaryenIndex i = 0; i < BinaryenGetNumTables(cur_mod); i++) {
                BinaryenTableRef  cur = BinaryenGetTableByIndex(cur_mod, i);
                BinaryenType cur_type = BinaryenTableGetType(cur); 
                cur_type = wasm_simplify_type(cur_type);

                if (cur_type == BinaryenTypeFuncref() && (match_name == NULL || rand_below(afl, 2))) {
                    match_name = BinaryenTableGetName(cur);
                }
            }

            // if unfortunately we cannot find such table, add one.
            if (match_name == NULL) {
                u32 new_table_int = BinaryenGetNumTables(cur_mod);
                int len = snprintf(NULL, 0, "%u", new_table_int);
                char* new_table_name = calloc(len + 1, sizeof(char));
                snprintf(new_table_name, len + 1, "%u", new_table_int);

                BinaryenTableRef new_table = BinaryenAddTable(cur_mod, new_table_name, 0x10,
                                                              (BinaryenIndex)-1, BinaryenTypeFuncref());
                match_name = BinaryenTableGetName(new_table);
                free(new_table_name);
            }

            BinaryenCallIndirectSetTable(el, match_name);
        }

        // fixup for call instruction.
        if (id == BinaryenCallId()) {
            const char*    tar_func_name = BinaryenCallGetTarget(el);
            if (!tar_func_name) {
                success = false;
                goto final;
            }
            BinaryenFunctionRef tar_func = BinaryenGetFunction(spl_mod, tar_func_name);
            if (!tar_func) {
                success = false;
                goto final;
            }
            BinaryenType  param = wasm_simplify_type(BinaryenFunctionGetParams(tar_func));
            BinaryenType result = wasm_simplify_type(BinaryenFunctionGetResults(tar_func));

            // find out if current module has functions with matching signature.
            u32 num_matches = 0;
            BinaryenFunctionRef* match_funcs = NULL;
            num_matches = wasm_find_matching_funcs(cur_mod, param, result, &match_funcs, false);

            const char* match_name = NULL;
            if (num_matches == 0) {
                u32 func_ind = BinaryenGetNumFunctions(cur_mod);
                int len = snprintf(NULL, 0, "%u", func_ind);
                char* func_name = calloc(len + 1, sizeof(char));
                snprintf(func_name, len + 1, "%u", func_ind);

                BinaryenType* func_vars = NULL;
                u32 num_vars = BinaryenFunctionGetNumVars(tar_func);
                if (num_vars > 0) {
                    func_vars = calloc(num_vars, sizeof(BinaryenType));
                    
                    for (u32 ind = 0; ind < num_vars; ind++)
                        func_vars[ind] = BinaryenFunctionGetVar(tar_func, ind);
                }

                BinaryenExpressionRef new_func_body = BinaryenFunctionGetBody(tar_func);
                BinaryenFunctionRef new_func = BinaryenAddFunction(cur_mod, func_name, param,
                                                                   result, func_vars, num_vars, new_func_body);
                match_name = BinaryenFunctionGetName(new_func);

                // we need recursive fixup for newly-introduced function.
                Frag frag;
                frag.func = new_func;
                frag.expr = new_func_body;
                wasm_splicing_fixup(data, cur_mod, spl_mod, &frag, &frag);
                BinaryenFunctionSetBody(new_func, frag.expr);

                free(func_name);
                free(func_vars);
            } else {
                u32 selected = rand_below(afl, num_matches);
                match_name = BinaryenFunctionGetName(match_funcs[selected]);
            }

            BinaryenCallSetTarget(el, match_name);
            free(match_funcs);
        }

        BinaryenIndex size = 0;
        BinaryenExpressionRef* child = NULL;
        size = wasm_get_children_expr(el, &child);

        for (BinaryenIndex ind = 0; ind < size; ind++)
            if (child[ind]) list_append(&child_node_list, child[ind]);
        
        if (el_box->next != next)
            next = el_box->next;
        
        LIST_REMOVE_CURRENT_EL_IN_FOREACH();
        free(child);

    });

final:
    if (locals_cur.element_total_count) LIST_FOREACH_CLEAR(&locals_cur, localCxt, {free(el);});
    if (labels_cur.element_total_count) LIST_FOREACH_CLEAR(&labels_cur, labelCxt, {free(el);});
    free(params_spl);
    free(br_inst);
    free(br_type);
    free(br_type_valid);
    return success;
}

/*
    Find an available name in the name pool, if not found,
    create a new one.
    @param data: global mutator state.
    @return:     a name string in the name pool.
*/
char* wasm_find_available_name(wasm_mutator_t* data) {

    char* candidate = NULL;
    list_t* free_names = &data->name_grp->free_names;
    list_t* used_names = &data->name_grp->used_names;

    if (free_names->element_total_count <= 0) {

        u32   name_int = data->name_grp->name_cnt;
        int   name_len = snprintf(NULL, 0, "fuzz_%u", name_int);
        char* name_str = calloc(name_len + 1, sizeof(char));
        snprintf(name_str, name_len + 1, "fuzz_%u", name_int);

        list_append(used_names, name_str);
        data->name_grp->name_cnt++;
        candidate = name_str;

    } else {

        LIST_FOREACH(free_names, char, {
            candidate = el;
            break;
        });

        list_remove(free_names, candidate);
        list_append(used_names, candidate);

    }

    return candidate;

}

/*
    Reset all used names in the name pool to the free state.
    @param data: global mutator state.
*/
void wasm_reset_name_grp(wasm_mutator_t* data) {

    list_t* free_names = &data->name_grp->free_names;
    list_t* used_names = &data->name_grp->used_names;

    if (used_names->element_total_count >= 1) {
        LIST_FOREACH(used_names, char, {list_append(free_names, el);});
        LIST_FOREACH_CLEAR(used_names, char, {});
    }

}

/*
    Binaryen lacks support for handling memory and data seg-
    ments separately, thus we need extra care when creating
    memory.
    @param afl:     global afl state.
    @param mod:     current wasm module.
    @param add_seg: whether adding data segments.
*/
void wasm_set_memory(afl_state_t* afl, BinaryenModuleRef mod, bool add_seg) {

    bool shared = false, memory64 = false;
    BinaryenIndex initial = 1, maximum = (BinaryenIndex)-1;

    if (BinaryenHasMemory(mod)) {
        memory64 = BinaryenMemoryIs64(mod, NULL);
        shared   = BinaryenMemoryIsShared(mod, NULL);
        initial  = BinaryenMemoryGetInitial(mod, NULL);

        if (BinaryenMemoryHasMax(mod, NULL))
            maximum = BinaryenMemoryGetMax(mod, NULL);
    }

    char** segments = NULL;
    bool* segmentPassives = NULL;
    BinaryenIndex* segmentSizes = NULL;
    BinaryenExpressionRef* segmentOffsets = NULL;

    BinaryenIndex num_seg = BinaryenGetNumMemorySegments(mod);
    if (num_seg > 0) {
        
        segmentPassives = calloc(num_seg, sizeof(bool));
        segments = calloc(num_seg, sizeof(const char*));
        segmentSizes = calloc(num_seg, sizeof(BinaryenIndex));
        segmentOffsets = calloc(num_seg, sizeof(BinaryenExpressionRef));

        for (BinaryenIndex ind = 0; ind < num_seg; ind++) {

            int len = snprintf(NULL, 0, "%u", ind);
            char* seg_name = calloc(len + 1, sizeof(char));
            snprintf(seg_name, len + 1, "%u", ind);

            segmentPassives[ind] = BinaryenGetMemorySegmentPassive(mod, seg_name);
            segmentSizes[ind] = BinaryenGetMemorySegmentByteLength(mod, seg_name);
            segments[ind] = calloc(segmentSizes[ind] + 1, sizeof(char));
            BinaryenCopyMemorySegmentData(mod, seg_name, segments[ind]);

            if (!segmentPassives[ind]) {
                int32_t offset = BinaryenGetMemorySegmentByteOffset(mod, seg_name);
                segmentOffsets[ind] = BinaryenConst(mod, BinaryenLiteralInt32(offset));
            }

            free(seg_name);

        }

        BinaryenSetMemory(mod, initial, maximum, NULL, NULL, (const char**)segments, segmentPassives,
                          segmentOffsets, segmentSizes, num_seg, shared, memory64, "0");

        free(segmentSizes);
        free(segmentOffsets);
        free(segmentPassives);

        for (BinaryenIndex ind = 0; ind < num_seg; ind++) free(segments[ind]);
        free(segments);

    } else {

        if (add_seg) {
        
            num_seg = rand_below(afl, 5) + 1;
            segmentPassives = calloc(num_seg, sizeof(bool));
            segments = calloc(num_seg, sizeof(const char*));
            segmentSizes = calloc(num_seg, sizeof(BinaryenIndex));
            segmentOffsets = calloc(num_seg, sizeof(BinaryenExpressionRef));

            for (BinaryenIndex ind = 0; ind < num_seg; ind++) {

                segmentSizes[ind] = 20;
                segments[ind] = "WeMustKnowWeWillKnow";
                segmentPassives[ind] = (bool)rand_below(afl, 2);

                if (!segmentPassives[ind]) {
                    int32_t offset = (int32_t)rand_below(afl, UINT32_MAX);
                    segmentOffsets[ind] = BinaryenConst(mod, BinaryenLiteralInt32(offset));
                }

            }
        
        }

        BinaryenSetMemory(mod, initial, maximum, NULL, NULL, (const char**)segments, segmentPassives,
                          segmentOffsets, segmentSizes, num_seg, shared, memory64, "0");

        free(segments);
        free(segmentSizes);
        free(segmentOffsets);
        free(segmentPassives);

    }

}
