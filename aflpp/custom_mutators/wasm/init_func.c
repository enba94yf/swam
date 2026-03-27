#include "mutator_func.h"

void wasm_init(wasm_mutator_t* data) {

    wasm_init_name_grp(data);
    wasm_init_instr_grps(data);

}

void wasm_deinit(wasm_mutator_t* data) {

    wasm_destroy_name_grp(data);
    wasm_destroy_instr_grps(data);

}

/*
    Binaryen uses name to identify blocks or loops, and it will intern name
    strings for better performance, which will cause excessive memory usage
    if we randomly generate names. Therefore, we use a name pool to maintain
    names used by each module.
    @param data: global mutator state.
*/
void wasm_init_name_grp(wasm_mutator_t* data) {

    nameGrp* name_grp = calloc(1, sizeof(nameGrp));

    // initially we insert 10 names into the pool.
    name_grp->name_cnt = 10;
    for (u32 i = 0; i < name_grp->name_cnt; i++) {

        int   name_len = snprintf(NULL, 0, "fuzz_%u", i);
        char* name_str = calloc(name_len + 1, sizeof(char));
        snprintf(name_str, name_len + 1, "fuzz_%u", i);

        list_append(&name_grp->free_names, name_str);

    }

    data->name_grp = name_grp;

}

void wasm_destroy_name_grp(wasm_mutator_t* data) {

    if (data->name_grp->free_names.element_total_count)
        LIST_FOREACH_CLEAR(&data->name_grp->free_names, char, {free(el);})
    if (data->name_grp->used_names.element_total_count)
        LIST_FOREACH_CLEAR(&data->name_grp->used_names, char, {free(el);})
    
    free(data->name_grp);

}

/*
    Cluster all instructions having the same type-checking rules into the same
    group and save all groups into global state. We exclude instructions related
    to local/global variables since they have special uses in Binaryen. Besides, 
    control-flow instructions are also not considered since their type-checking 
    rules are context-related.
    @param data: global state (represented as wasm_mutator_t), whose instr_grps
                 and num_instr_grps fields will be updated by this function.
*/
void wasm_init_instr_grps(wasm_mutator_t* data) {

    BinaryenType i32 = BinaryenTypeInt32();
    BinaryenType i64 = BinaryenTypeInt64();
    BinaryenType f32 = BinaryenTypeFloat32();
    BinaryenType f64 = BinaryenTypeFloat64();
    BinaryenType none = BinaryenTypeNone();
#ifndef DISABLE_FIXED_WIDTH_SIMD
    BinaryenType v128 = BinaryenTypeVec128();
#endif
    BinaryenType funcref = BinaryenTypeFuncref();
    BinaryenType externref = BinaryenTypeExternref();

    BinaryenExpressionId nopId = BinaryenNopId();
    BinaryenExpressionId dropId = BinaryenDropId();
    BinaryenExpressionId loadId = BinaryenLoadId();
    BinaryenExpressionId storeId = BinaryenStoreId();
    BinaryenExpressionId constId = BinaryenConstId();
    BinaryenExpressionId unaryId = BinaryenUnaryId();
    BinaryenExpressionId selectId = BinaryenSelectId();
    BinaryenExpressionId binaryId = BinaryenBinaryId();
    BinaryenExpressionId refFuncId = BinaryenRefFuncId();
    BinaryenExpressionId refNullId = BinaryenRefNullId();
    BinaryenExpressionId tableGetId = BinaryenTableGetId();
    BinaryenExpressionId tableSetId = BinaryenTableSetId();
    BinaryenExpressionId dataDropId = BinaryenDataDropId();
    BinaryenExpressionId tableSizeId = BinaryenTableSizeId();
    BinaryenExpressionId tableGrowId = BinaryenTableGrowId();
    BinaryenExpressionId refIsNullId = BinaryenRefIsNullId();
    BinaryenExpressionId memorySizeId = BinaryenMemorySizeId();
    BinaryenExpressionId memoryGrowId = BinaryenMemoryGrowId();
    BinaryenExpressionId memoryFillId = BinaryenMemoryFillId();
    BinaryenExpressionId memoryCopyId = BinaryenMemoryCopyId();
    BinaryenExpressionId memoryInitId = BinaryenMemoryInitId();
#ifndef DISABLE_FIXED_WIDTH_SIMD
    BinaryenExpressionId simdLoadId = BinaryenSIMDLoadId();
    BinaryenExpressionId simdShiftId = BinaryenSIMDShiftId();
    BinaryenExpressionId simdTernaryId = BinaryenSIMDTernaryId();
    BinaryenExpressionId simdShuffleId = BinaryenSIMDShuffleId();
    BinaryenExpressionId simdExtractId = BinaryenSIMDExtractId();
    BinaryenExpressionId simdReplaceId = BinaryenSIMDReplaceId();
    BinaryenExpressionId simdLoadStoreLaneId = BinaryenSIMDLoadStoreLaneId();
#endif

#ifndef DISABLE_FIXED_WIDTH_SIMD
    uint32_t num_inst_grps = 76;
#else
    uint32_t num_inst_grps = 55;
#endif
    InstrGrp* inst_grps = (InstrGrp*)calloc(num_inst_grps, sizeof(InstrGrp));

    /* stack-operating type: () -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[0], 0, 1, 3);
    FILL_GRP_TYPE(inst_grps[0].produced, i32);
    FILL_INSTR_TYPE_OPD(inst_grps[0].groups[0], constId, i32);
    FILL_INSTR_TYPE_OPD(inst_grps[0].groups[1], tableSizeId, i32);
    FILL_INSTR_TYPE_OPD(inst_grps[0].groups[2], memorySizeId, i32);

    /* stack-operating type: () -> (i64) */
    INITIALIZE_INSTR_GRP(inst_grps[1], 0, 1, 1);
    FILL_GRP_TYPE(inst_grps[1].produced, i64);
    FILL_INSTR_TYPE_OPD(inst_grps[1].groups[0], constId, i64);

    /* stack-operating type: () -> (f32) */
    INITIALIZE_INSTR_GRP(inst_grps[2], 0, 1, 1);
    FILL_GRP_TYPE(inst_grps[2].produced, f32);
    FILL_INSTR_TYPE_OPD(inst_grps[2].groups[0], constId, f32);

    /* stack-operating type: () -> (f64) */
    INITIALIZE_INSTR_GRP(inst_grps[3], 0, 1, 1);
    FILL_GRP_TYPE(inst_grps[3].produced, f64);
    FILL_INSTR_TYPE_OPD(inst_grps[3].groups[0], constId, f64);

    /* stack-operating type: (i32) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[4], 1, 1, 12);
    FILL_GRP_TYPE(inst_grps[4].required, i32);
    FILL_GRP_TYPE(inst_grps[4].produced, i32);
    FILL_INSTR_TYPE_OPT(inst_grps[4].groups[0], unaryId, BinaryenClzInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[4].groups[1], unaryId, BinaryenCtzInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[4].groups[2], unaryId, BinaryenPopcntInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[4].groups[3], unaryId, BinaryenExtendS8Int32());
    FILL_INSTR_TYPE_OPT(inst_grps[4].groups[4], unaryId, BinaryenExtendS16Int32());
    FILL_INSTR_TYPE_OPT(inst_grps[4].groups[5], unaryId, BinaryenEqZInt32());
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[4].groups[6], loadId, i32, 4, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[4].groups[7], loadId, i32, 1, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[4].groups[8], loadId, i32, 1, 1);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[4].groups[9], loadId, i32, 2, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[4].groups[10], loadId, i32, 2, 1);
    FILL_INSTR_TYPE_OPD(inst_grps[4].groups[11], memoryGrowId, i32);

    /* stack-operating type: (i64) -> (i64) */
    INITIALIZE_INSTR_GRP(inst_grps[5], 1, 1, 6);
    FILL_GRP_TYPE(inst_grps[5].required, i64);
    FILL_GRP_TYPE(inst_grps[5].produced, i64);
    FILL_INSTR_TYPE_OPT(inst_grps[5].groups[0], unaryId, BinaryenClzInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[5].groups[1], unaryId, BinaryenCtzInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[5].groups[2], unaryId, BinaryenPopcntInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[5].groups[3], unaryId, BinaryenExtendS8Int64());
    FILL_INSTR_TYPE_OPT(inst_grps[5].groups[4], unaryId, BinaryenExtendS16Int64());
    FILL_INSTR_TYPE_OPT(inst_grps[5].groups[5], unaryId, BinaryenExtendS32Int64());

    /* stack-operating type: (f32) -> (f32) */
    INITIALIZE_INSTR_GRP(inst_grps[6], 1, 1, 7);
    FILL_GRP_TYPE(inst_grps[6].required, f32);
    FILL_GRP_TYPE(inst_grps[6].produced, f32);
    FILL_INSTR_TYPE_OPT(inst_grps[6].groups[0], unaryId, BinaryenAbsFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[6].groups[1], unaryId, BinaryenNegFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[6].groups[2], unaryId, BinaryenSqrtFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[6].groups[3], unaryId, BinaryenCeilFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[6].groups[4], unaryId, BinaryenFloorFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[6].groups[5], unaryId, BinaryenTruncFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[6].groups[6], unaryId, BinaryenNearestFloat32());

    /* stack-operating type: (f64) -> (f64) */
    INITIALIZE_INSTR_GRP(inst_grps[7], 1, 1, 7);
    FILL_GRP_TYPE(inst_grps[7].required, f64);
    FILL_GRP_TYPE(inst_grps[7].produced, f64);
    FILL_INSTR_TYPE_OPT(inst_grps[7].groups[0], unaryId, BinaryenAbsFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[7].groups[1], unaryId, BinaryenNegFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[7].groups[2], unaryId, BinaryenSqrtFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[7].groups[3], unaryId, BinaryenCeilFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[7].groups[4], unaryId, BinaryenFloorFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[7].groups[5], unaryId, BinaryenTruncFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[7].groups[6], unaryId, BinaryenNearestFloat64());

    /* stack-operating type: (i32 i32) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[8], 2, 1, 25);
    FILL_GRP_TYPE(inst_grps[8].required, i32, i32);
    FILL_GRP_TYPE(inst_grps[8].produced, i32);
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[0], binaryId, BinaryenAddInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[1], binaryId, BinaryenSubInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[2], binaryId, BinaryenMulInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[3], binaryId, BinaryenDivSInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[4], binaryId, BinaryenDivUInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[5], binaryId, BinaryenRemSInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[6], binaryId, BinaryenRemUInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[7], binaryId, BinaryenAndInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[8], binaryId, BinaryenOrInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[9], binaryId, BinaryenXorInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[10], binaryId, BinaryenShlInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[11], binaryId, BinaryenShrUInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[12], binaryId, BinaryenShrSInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[13], binaryId, BinaryenRotLInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[14], binaryId, BinaryenRotRInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[15], binaryId, BinaryenEqInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[16], binaryId, BinaryenNeInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[17], binaryId, BinaryenLtSInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[18], binaryId, BinaryenLtUInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[19], binaryId, BinaryenLeSInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[20], binaryId, BinaryenLeUInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[21], binaryId, BinaryenGtSInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[22], binaryId, BinaryenGtUInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[23], binaryId, BinaryenGeSInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[8].groups[24], binaryId, BinaryenGeUInt32());

    /* stack-operating type: (i64 i64) -> (i64) */
    INITIALIZE_INSTR_GRP(inst_grps[9], 2, 1, 15);
    FILL_GRP_TYPE(inst_grps[9].required, i64, i64);
    FILL_GRP_TYPE(inst_grps[9].produced, i64);
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[0], binaryId, BinaryenAddInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[1], binaryId, BinaryenSubInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[2], binaryId, BinaryenMulInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[3], binaryId, BinaryenDivSInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[4], binaryId, BinaryenDivUInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[5], binaryId, BinaryenRemSInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[6], binaryId, BinaryenRemUInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[7], binaryId, BinaryenAndInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[8], binaryId, BinaryenOrInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[9], binaryId, BinaryenXorInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[10], binaryId, BinaryenShlInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[11], binaryId, BinaryenShrUInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[12], binaryId, BinaryenShrSInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[13], binaryId, BinaryenRotLInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[9].groups[14], binaryId, BinaryenRotRInt64());

    /* stack-operating type: (f32 f32) -> (f32) */
    INITIALIZE_INSTR_GRP(inst_grps[10], 2, 1, 7);
    FILL_GRP_TYPE(inst_grps[10].required, f32, f32);
    FILL_GRP_TYPE(inst_grps[10].produced, f32);
    FILL_INSTR_TYPE_OPT(inst_grps[10].groups[0], binaryId, BinaryenAddFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[10].groups[1], binaryId, BinaryenSubFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[10].groups[2], binaryId, BinaryenMulFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[10].groups[3], binaryId, BinaryenDivFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[10].groups[4], binaryId, BinaryenMinFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[10].groups[5], binaryId, BinaryenMaxFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[10].groups[6], binaryId, BinaryenCopySignFloat32());

    /* stack-operating type: (f64 f64) -> (f64) */
    INITIALIZE_INSTR_GRP(inst_grps[11], 2, 1, 7);
    FILL_GRP_TYPE(inst_grps[11].required, f64, f64);
    FILL_GRP_TYPE(inst_grps[11].produced, f64);
    FILL_INSTR_TYPE_OPT(inst_grps[11].groups[0], binaryId, BinaryenAddFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[11].groups[1], binaryId, BinaryenSubFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[11].groups[2], binaryId, BinaryenMulFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[11].groups[3], binaryId, BinaryenDivFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[11].groups[4], binaryId, BinaryenMinFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[11].groups[5], binaryId, BinaryenMaxFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[11].groups[6], binaryId, BinaryenCopySignFloat64());

    /* stack-operating type: (i64) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[12], 1, 1, 2);
    FILL_GRP_TYPE(inst_grps[12].required, i64);
    FILL_GRP_TYPE(inst_grps[12].produced, i32);
    FILL_INSTR_TYPE_OPT(inst_grps[12].groups[0], unaryId, BinaryenEqZInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[12].groups[1], unaryId, BinaryenWrapInt64());

    /* stack-operating type: (i64 i64) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[13], 2, 1, 10);
    FILL_GRP_TYPE(inst_grps[13].required, i64, i64);
    FILL_GRP_TYPE(inst_grps[13].produced, i32);
    FILL_INSTR_TYPE_OPT(inst_grps[13].groups[0], binaryId, BinaryenEqInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[13].groups[1], binaryId, BinaryenNeInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[13].groups[2], binaryId, BinaryenLtSInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[13].groups[3], binaryId, BinaryenLtUInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[13].groups[4], binaryId, BinaryenLeSInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[13].groups[5], binaryId, BinaryenLeUInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[13].groups[6], binaryId, BinaryenGtSInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[13].groups[7], binaryId, BinaryenGtUInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[13].groups[8], binaryId, BinaryenGeSInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[13].groups[9], binaryId, BinaryenGeUInt64());

    /* stack-operating type: (f32 f32) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[14], 2, 1, 6);
    FILL_GRP_TYPE(inst_grps[14].required, f32, f32);
    FILL_GRP_TYPE(inst_grps[14].produced, i32);
    FILL_INSTR_TYPE_OPT(inst_grps[14].groups[0], binaryId, BinaryenEqFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[14].groups[1], binaryId, BinaryenNeFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[14].groups[2], binaryId, BinaryenLtFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[14].groups[3], binaryId, BinaryenLeFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[14].groups[4], binaryId, BinaryenGtFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[14].groups[5], binaryId, BinaryenGeFloat32());

    /* stack-operating type: (f64 f64) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[15], 2, 1, 6);
    FILL_GRP_TYPE(inst_grps[15].required, f64, f64);
    FILL_GRP_TYPE(inst_grps[15].produced, i32);
    FILL_INSTR_TYPE_OPT(inst_grps[15].groups[0], binaryId, BinaryenEqFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[15].groups[1], binaryId, BinaryenNeFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[15].groups[2], binaryId, BinaryenLtFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[15].groups[3], binaryId, BinaryenLeFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[15].groups[4], binaryId, BinaryenGtFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[15].groups[5], binaryId, BinaryenGeFloat64());

    /* stack-operating type: (i32) -> (i64) */
    INITIALIZE_INSTR_GRP(inst_grps[16], 1, 1, 9);
    FILL_GRP_TYPE(inst_grps[16].required, i32);
    FILL_GRP_TYPE(inst_grps[16].produced, i64);
    FILL_INSTR_TYPE_OPT(inst_grps[16].groups[0], unaryId, BinaryenExtendSInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[16].groups[1], unaryId, BinaryenExtendUInt32());
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[16].groups[2], loadId, i64, 8, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[16].groups[3], loadId, i64, 1, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[16].groups[4], loadId, i64, 1, 1);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[16].groups[5], loadId, i64, 2, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[16].groups[6], loadId, i64, 2, 1);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[16].groups[7], loadId, i64, 4, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[16].groups[8], loadId, i64, 4, 1);

    /* stack-operating type: (f32) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[17], 1, 1, 5);
    FILL_GRP_TYPE(inst_grps[17].required, f32);
    FILL_GRP_TYPE(inst_grps[17].produced, i32);
    FILL_INSTR_TYPE_OPT(inst_grps[17].groups[0], unaryId, BinaryenTruncSFloat32ToInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[17].groups[1], unaryId, BinaryenTruncUFloat32ToInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[17].groups[2], unaryId, BinaryenReinterpretFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[17].groups[3], unaryId, BinaryenTruncSatSFloat32ToInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[17].groups[4], unaryId, BinaryenTruncSatUFloat32ToInt32());

    /* stack-operating type: (f32) -> (i64) */
    INITIALIZE_INSTR_GRP(inst_grps[18], 1, 1, 4);
    FILL_GRP_TYPE(inst_grps[18].required, f32);
    FILL_GRP_TYPE(inst_grps[18].produced, i64);
    FILL_INSTR_TYPE_OPT(inst_grps[18].groups[0], unaryId, BinaryenTruncSFloat32ToInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[18].groups[1], unaryId, BinaryenTruncUFloat32ToInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[18].groups[2], unaryId, BinaryenTruncSatSFloat32ToInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[18].groups[3], unaryId, BinaryenTruncSatUFloat32ToInt64());

    /* stack-operating type: (f64) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[19], 1, 1, 4);
    FILL_GRP_TYPE(inst_grps[19].required, f64);
    FILL_GRP_TYPE(inst_grps[19].produced, i32);
    FILL_INSTR_TYPE_OPT(inst_grps[19].groups[0], unaryId, BinaryenTruncSFloat64ToInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[19].groups[1], unaryId, BinaryenTruncUFloat64ToInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[19].groups[2], unaryId, BinaryenTruncSatSFloat64ToInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[19].groups[3], unaryId, BinaryenTruncSatUFloat64ToInt32());

    /* stack-operating type: (f64) -> (i64) */
    INITIALIZE_INSTR_GRP(inst_grps[20], 1, 1, 5);
    FILL_GRP_TYPE(inst_grps[20].required, f64);
    FILL_GRP_TYPE(inst_grps[20].produced, i64);
    FILL_INSTR_TYPE_OPT(inst_grps[20].groups[0], unaryId, BinaryenTruncSFloat64ToInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[20].groups[1], unaryId, BinaryenTruncUFloat64ToInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[20].groups[2], unaryId, BinaryenReinterpretFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[20].groups[3], unaryId, BinaryenTruncSatSFloat64ToInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[20].groups[4], unaryId, BinaryenTruncSatUFloat64ToInt64());

    /* stack-operating type: (i32) -> (f32) */
    INITIALIZE_INSTR_GRP(inst_grps[21], 1, 1, 4);
    FILL_GRP_TYPE(inst_grps[21].required, i32);
    FILL_GRP_TYPE(inst_grps[21].produced, f32);
    FILL_INSTR_TYPE_OPT(inst_grps[21].groups[0], unaryId, BinaryenReinterpretInt32());
    FILL_INSTR_TYPE_OPT(inst_grps[21].groups[1], unaryId, BinaryenConvertSInt32ToFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[21].groups[2], unaryId, BinaryenConvertUInt32ToFloat32());
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[21].groups[3], loadId, f32, 4, 0);

    /* stack-operating type: (i64) -> (f64) */
    INITIALIZE_INSTR_GRP(inst_grps[22], 1, 1, 3);
    FILL_GRP_TYPE(inst_grps[22].required, i64);
    FILL_GRP_TYPE(inst_grps[22].produced, f64);
    FILL_INSTR_TYPE_OPT(inst_grps[22].groups[0], unaryId, BinaryenReinterpretInt64());
    FILL_INSTR_TYPE_OPT(inst_grps[22].groups[1], unaryId, BinaryenConvertSInt64ToFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[22].groups[2], unaryId, BinaryenConvertUInt64ToFloat64());

    /* stack-operating type: (i32) -> (f64) */
    INITIALIZE_INSTR_GRP(inst_grps[23], 1, 1, 3);
    FILL_GRP_TYPE(inst_grps[23].required, i32);
    FILL_GRP_TYPE(inst_grps[23].produced, f64);
    FILL_INSTR_TYPE_OPT(inst_grps[23].groups[0], unaryId, BinaryenConvertSInt32ToFloat64());
    FILL_INSTR_TYPE_OPT(inst_grps[23].groups[1], unaryId, BinaryenConvertUInt32ToFloat64());
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[23].groups[2], loadId, f64, 8, 0);

    /* stack-operating type: (i64) -> (f32) */
    INITIALIZE_INSTR_GRP(inst_grps[24], 1, 1, 2);
    FILL_GRP_TYPE(inst_grps[24].required, i64);
    FILL_GRP_TYPE(inst_grps[24].produced, f32);
    FILL_INSTR_TYPE_OPT(inst_grps[24].groups[0], unaryId, BinaryenConvertSInt64ToFloat32());
    FILL_INSTR_TYPE_OPT(inst_grps[24].groups[1], unaryId, BinaryenConvertUInt64ToFloat32());

    /* stack-operating type: (f64) -> (f32) */
    INITIALIZE_INSTR_GRP(inst_grps[25], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[25].required, f64);
    FILL_GRP_TYPE(inst_grps[25].produced, f32);
    FILL_INSTR_TYPE_OPT(inst_grps[25].groups[0], unaryId, BinaryenDemoteFloat64());

    /* stack-operating type: (f32) -> (f64) */
    INITIALIZE_INSTR_GRP(inst_grps[26], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[26].required, f32);
    FILL_GRP_TYPE(inst_grps[26].produced, f64);
    FILL_INSTR_TYPE_OPT(inst_grps[26].groups[0], unaryId, BinaryenPromoteFloat32());

    /* stack-operating type: () -> (funcref) */
    /* reference-related instructions need extra care :P */
    INITIALIZE_INSTR_GRP(inst_grps[27], 0, 1, 2);
    FILL_GRP_TYPE(inst_grps[27].produced, funcref);
    FILL_INSTR_TYPE_OPD(inst_grps[27].groups[0], refNullId, funcref);
    FILL_INSTR_TYPE_OPD(inst_grps[27].groups[1], refFuncId, funcref);

    /* stack-operating type: () -> (externref) */
    INITIALIZE_INSTR_GRP(inst_grps[28], 0, 1, 1);
    FILL_GRP_TYPE(inst_grps[28].produced, externref);
    FILL_INSTR_TYPE_OPD(inst_grps[28].groups[0], refNullId, externref);

    /* stack-operating type: (funcref) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[29], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[29].required, funcref);
    FILL_GRP_TYPE(inst_grps[29].produced, i32);
    FILL_INSTR_TYPE_OPD(inst_grps[29].groups[0], refIsNullId, funcref);

    /* stack-operating type: (externref) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[30], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[30].required, externref);
    FILL_GRP_TYPE(inst_grps[30].produced, i32);
    FILL_INSTR_TYPE_OPD(inst_grps[30].groups[0], refIsNullId, externref);

    /* stack-operating type: (i32) -> () */
    /* value-polymorphic instructions need extra care :P */
    INITIALIZE_INSTR_GRP(inst_grps[31], 1, 0, 1);
    FILL_GRP_TYPE(inst_grps[31].required, i32);
    FILL_INSTR_TYPE_OPD(inst_grps[31].groups[0], dropId, i32);

    /* stack-operating type: (i64) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[32], 1, 0, 1);
    FILL_GRP_TYPE(inst_grps[32].required, i64);
    FILL_INSTR_TYPE_OPD(inst_grps[32].groups[0], dropId, i64);

    /* stack-operating type: (f32) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[33], 1, 0, 1);
    FILL_GRP_TYPE(inst_grps[33].required, f32);
    FILL_INSTR_TYPE_OPD(inst_grps[33].groups[0], dropId, f32);

    /* stack-operating type: (f64) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[34], 1, 0, 1);
    FILL_GRP_TYPE(inst_grps[34].required, f64);
    FILL_INSTR_TYPE_OPD(inst_grps[34].groups[0], dropId, f64);

    /* stack-operating type: (funcref) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[35], 1, 0, 1);
    FILL_GRP_TYPE(inst_grps[35].required, funcref);
    FILL_INSTR_TYPE_OPD(inst_grps[35].groups[0], dropId, funcref);

    /* stack-operating type: (externref) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[36], 1, 0, 1);
    FILL_GRP_TYPE(inst_grps[36].required, externref);
    FILL_INSTR_TYPE_OPD(inst_grps[36].groups[0], dropId, externref);

    /* stack-operating type: (i32 i32 i32) -> (i32) */
    /* value-polymorphic instructions need extra care :P */
    INITIALIZE_INSTR_GRP(inst_grps[37], 3, 1, 1);
    FILL_GRP_TYPE(inst_grps[37].required, i32, i32, i32);
    FILL_GRP_TYPE(inst_grps[37].produced, i32);
    FILL_INSTR_TYPE_OPD(inst_grps[37].groups[0], selectId, i32);

    /* stack-operating type: (i64 i64 i32) -> (i64) */
    INITIALIZE_INSTR_GRP(inst_grps[38], 3, 1, 1);
    FILL_GRP_TYPE(inst_grps[38].required, i64, i64, i32);
    FILL_GRP_TYPE(inst_grps[38].produced, i64);
    FILL_INSTR_TYPE_OPD(inst_grps[38].groups[0], selectId, i64);

    /* stack-operating type: (f32 f32 i32) -> (f32) */
    INITIALIZE_INSTR_GRP(inst_grps[39], 3, 1, 1);
    FILL_GRP_TYPE(inst_grps[39].required, f32, f32, i32);
    FILL_GRP_TYPE(inst_grps[39].produced, f32);
    FILL_INSTR_TYPE_OPD(inst_grps[39].groups[0], selectId, f32);

    /* stack-operating type: (f64 f64 i32) -> (f64) */
    INITIALIZE_INSTR_GRP(inst_grps[40], 3, 1, 1);
    FILL_GRP_TYPE(inst_grps[40].required, f64, f64, i32);
    FILL_GRP_TYPE(inst_grps[40].produced, f64);
    FILL_INSTR_TYPE_OPD(inst_grps[40].groups[0], selectId, f64);

    /* stack-operating type: (funcref funcref i32) -> (funcref) */
    INITIALIZE_INSTR_GRP(inst_grps[41], 3, 1, 1);
    FILL_GRP_TYPE(inst_grps[41].required, funcref, funcref, i32);
    FILL_GRP_TYPE(inst_grps[41].produced, funcref);
    FILL_INSTR_TYPE_OPD(inst_grps[41].groups[0], selectId, funcref);

    /* stack-operating type: (externref externref i32) -> (externref) */
    INITIALIZE_INSTR_GRP(inst_grps[42], 3, 1, 1);
    FILL_GRP_TYPE(inst_grps[42].required, externref, externref, i32);
    FILL_GRP_TYPE(inst_grps[42].produced, externref);
    FILL_INSTR_TYPE_OPD(inst_grps[42].groups[0], selectId, externref);

    /* stack-operating type: (i32) -> (funcref) */
    INITIALIZE_INSTR_GRP(inst_grps[43], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[43].required, i32);
    FILL_GRP_TYPE(inst_grps[43].produced, funcref);
    FILL_INSTR_TYPE_OPD(inst_grps[43].groups[0], tableGetId, funcref);

    /* stack-operating type: (i32) -> (externref) */
    INITIALIZE_INSTR_GRP(inst_grps[44], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[44].required, i32);
    FILL_GRP_TYPE(inst_grps[44].produced, externref);
    FILL_INSTR_TYPE_OPD(inst_grps[44].groups[0], tableGetId, externref);

    /* stack-operating type: (i32 funcref) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[45], 2, 0, 1);
    FILL_GRP_TYPE(inst_grps[45].required, i32, funcref);
    FILL_INSTR_TYPE_OPD(inst_grps[45].groups[0], tableSetId, funcref);

    /* stack-operating type: (i32 externref) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[46], 2, 0, 1);
    FILL_GRP_TYPE(inst_grps[46].required, i32, externref);
    FILL_INSTR_TYPE_OPD(inst_grps[46].groups[0], tableSetId, externref);

    /* stack-operating type: (funcref i32) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[47], 2, 1, 1);
    FILL_GRP_TYPE(inst_grps[47].required, funcref, i32);
    FILL_GRP_TYPE(inst_grps[47].produced, i32);
    FILL_INSTR_TYPE_OPD(inst_grps[47].groups[0], tableGrowId, funcref);

    /* stack-operating type: (externref i32) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[48], 2, 1, 1);
    FILL_GRP_TYPE(inst_grps[48].required, externref, i32);
    FILL_GRP_TYPE(inst_grps[48].produced, i32);
    FILL_INSTR_TYPE_OPD(inst_grps[48].groups[0], tableGrowId, externref);

    /* stack-operating type: (i32 i32) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[49], 2, 0, 3);
    FILL_GRP_TYPE(inst_grps[49].required, i32, i32);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[49].groups[0], storeId, i32, 4, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[49].groups[1], storeId, i32, 1, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[49].groups[2], storeId, i32, 2, 0);

    /* stack-operating type: (i32 i64) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[50], 2, 0, 4);
    FILL_GRP_TYPE(inst_grps[50].required, i32, i64);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[50].groups[0], storeId, i64, 8, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[50].groups[1], storeId, i64, 1, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[50].groups[2], storeId, i64, 2, 0);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[50].groups[3], storeId, i64, 4, 0);

    /* stack-operating type: (i32 f32) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[51], 2, 0, 1);
    FILL_GRP_TYPE(inst_grps[51].required, i32, f32);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[51].groups[0], storeId, f32, 4, 0);

    /* stack-operating type: (i32 f64) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[52], 2, 0, 1);
    FILL_GRP_TYPE(inst_grps[52].required, i32, f64);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[52].groups[0], storeId, f64, 8, 0);

    /* stack-operating type: (i32 i32 i32) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[53], 3, 0, 3);
    FILL_GRP_TYPE(inst_grps[53].required, i32, i32, i32);
    FILL_INSTR_TYPE_OPD(inst_grps[53].groups[0], memoryFillId, none);
    FILL_INSTR_TYPE_OPD(inst_grps[53].groups[1], memoryCopyId, none);
    FILL_INSTR_TYPE_OPD(inst_grps[53].groups[2], memoryInitId, none);

    /* stack-operating type: () -> () */
    INITIALIZE_INSTR_GRP(inst_grps[54], 0, 0, 2);
    FILL_INSTR_TYPE_OPD(inst_grps[54].groups[0], dataDropId, none);
    FILL_INSTR_TYPE_OPD(inst_grps[54].groups[1], nopId, none);

#ifndef DISABLE_FIXED_WIDTH_SIMD

    /* stack-operating type: () -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[55], 0, 1, 1);
    FILL_GRP_TYPE(inst_grps[55].produced, v128);
    FILL_INSTR_TYPE_OPD(inst_grps[55].groups[0], constId, v128);

    /* stack-operating type: (v128) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[56], 1, 1, 50);
    FILL_GRP_TYPE(inst_grps[56].required, v128);
    FILL_GRP_TYPE(inst_grps[56].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[0], unaryId, BinaryenNotVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[1], unaryId, BinaryenAbsVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[2], unaryId, BinaryenNegVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[3], unaryId, BinaryenAbsVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[4], unaryId, BinaryenNegVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[5], unaryId, BinaryenAbsVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[6], unaryId, BinaryenNegVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[7], unaryId, BinaryenAbsVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[8], unaryId, BinaryenNegVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[9], unaryId, BinaryenAbsVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[10], unaryId, BinaryenNegVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[11], unaryId, BinaryenSqrtVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[12], unaryId, BinaryenCeilVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[13], unaryId, BinaryenFloorVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[14], unaryId, BinaryenTruncVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[15], unaryId, BinaryenNearestVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[16], unaryId, BinaryenAbsVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[17], unaryId, BinaryenNegVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[18], unaryId, BinaryenSqrtVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[19], unaryId, BinaryenCeilVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[20], unaryId, BinaryenFloorVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[21], unaryId, BinaryenTruncVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[22], unaryId, BinaryenNearestVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[23], unaryId, BinaryenPopcntVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[24], unaryId, BinaryenExtendLowSVecI8x16ToVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[25], unaryId, BinaryenExtendLowUVecI8x16ToVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[26], unaryId, BinaryenExtendHighSVecI8x16ToVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[27], unaryId, BinaryenExtendHighUVecI8x16ToVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[28], unaryId, BinaryenExtendLowSVecI16x8ToVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[29], unaryId, BinaryenExtendLowUVecI16x8ToVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[30], unaryId, BinaryenExtendHighSVecI16x8ToVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[31], unaryId, BinaryenExtendHighUVecI16x8ToVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[32], unaryId, BinaryenExtendLowSVecI32x4ToVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[33], unaryId, BinaryenExtendLowUVecI32x4ToVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[34], unaryId, BinaryenExtendHighSVecI32x4ToVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[35], unaryId, BinaryenExtendHighUVecI32x4ToVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[36], unaryId, BinaryenTruncSatSVecF32x4ToVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[37], unaryId, BinaryenTruncSatUVecF32x4ToVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[38], unaryId, BinaryenTruncSatZeroSVecF64x2ToVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[39], unaryId, BinaryenTruncSatZeroUVecF64x2ToVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[40], unaryId, BinaryenConvertSVecI32x4ToVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[41], unaryId, BinaryenConvertUVecI32x4ToVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[42], unaryId, BinaryenConvertLowSVecI32x4ToVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[43], unaryId, BinaryenConvertLowUVecI32x4ToVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[44], unaryId, BinaryenDemoteZeroVecF64x2ToVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[45], unaryId, BinaryenPromoteLowVecF32x4ToVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[46], unaryId, BinaryenExtAddPairwiseSVecI8x16ToI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[47], unaryId, BinaryenExtAddPairwiseUVecI8x16ToI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[48], unaryId, BinaryenExtAddPairwiseSVecI16x8ToI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[56].groups[49], unaryId, BinaryenExtAddPairwiseUVecI16x8ToI32x4());

    /* stack-operating type: (v128 v128) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[57], 2, 1, 121);
    FILL_GRP_TYPE(inst_grps[57].required, v128, v128);
    FILL_GRP_TYPE(inst_grps[57].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[0], binaryId, BinaryenAndVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[1], binaryId, BinaryenOrVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[2], binaryId, BinaryenXorVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[3], binaryId, BinaryenAndNotVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[4], binaryId, BinaryenSwizzleVecI8x16());
    FILL_INSTR_TYPE_OPD(inst_grps[57].groups[5], simdShuffleId, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[6], binaryId, BinaryenAddVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[7], binaryId, BinaryenSubVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[8], binaryId, BinaryenAddVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[9], binaryId, BinaryenSubVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[10], binaryId, BinaryenAddVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[11], binaryId, BinaryenSubVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[12], binaryId, BinaryenAddVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[13], binaryId, BinaryenSubVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[14], binaryId, BinaryenAddVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[15], binaryId, BinaryenSubVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[16], binaryId, BinaryenMulVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[17], binaryId, BinaryenDivVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[18], binaryId, BinaryenMinVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[19], binaryId, BinaryenMaxVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[20], binaryId, BinaryenPMinVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[21], binaryId, BinaryenPMaxVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[22], binaryId, BinaryenAddVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[23], binaryId, BinaryenSubVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[24], binaryId, BinaryenMulVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[25], binaryId, BinaryenDivVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[26], binaryId, BinaryenMinVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[27], binaryId, BinaryenMaxVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[28], binaryId, BinaryenPMinVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[29], binaryId, BinaryenPMaxVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[30], binaryId, BinaryenMinSVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[31], binaryId, BinaryenMinUVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[32], binaryId, BinaryenMaxSVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[33], binaryId, BinaryenMaxUVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[34], binaryId, BinaryenMinSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[35], binaryId, BinaryenMinUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[36], binaryId, BinaryenMaxSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[37], binaryId, BinaryenMaxUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[38], binaryId, BinaryenMinSVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[39], binaryId, BinaryenMinUVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[40], binaryId, BinaryenMaxSVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[41], binaryId, BinaryenMaxUVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[42], binaryId, BinaryenAddSatSVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[43], binaryId, BinaryenAddSatUVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[44], binaryId, BinaryenSubSatSVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[45], binaryId, BinaryenSubSatUVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[46], binaryId, BinaryenAddSatSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[47], binaryId, BinaryenAddSatUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[48], binaryId, BinaryenSubSatSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[49], binaryId, BinaryenSubSatUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[50], binaryId, BinaryenMulVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[51], binaryId, BinaryenMulVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[52], binaryId, BinaryenMulVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[53], binaryId, BinaryenAvgrUVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[54], binaryId, BinaryenAvgrUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[55], binaryId, BinaryenQ15MulrSatSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[56], binaryId, BinaryenEqVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[57], binaryId, BinaryenNeVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[58], binaryId, BinaryenLtSVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[59], binaryId, BinaryenLtUVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[60], binaryId, BinaryenGtSVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[61], binaryId, BinaryenGtUVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[62], binaryId, BinaryenLeSVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[63], binaryId, BinaryenLeUVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[64], binaryId, BinaryenGeSVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[65], binaryId, BinaryenGeUVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[66], binaryId, BinaryenEqVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[67], binaryId, BinaryenNeVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[68], binaryId, BinaryenLtSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[69], binaryId, BinaryenLtUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[70], binaryId, BinaryenGtSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[71], binaryId, BinaryenGtUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[72], binaryId, BinaryenLeSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[73], binaryId, BinaryenLeUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[74], binaryId, BinaryenGeSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[75], binaryId, BinaryenGeUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[76], binaryId, BinaryenEqVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[77], binaryId, BinaryenNeVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[78], binaryId, BinaryenLtSVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[79], binaryId, BinaryenLtUVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[80], binaryId, BinaryenGtSVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[81], binaryId, BinaryenGtUVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[82], binaryId, BinaryenLeSVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[83], binaryId, BinaryenLeUVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[84], binaryId, BinaryenGeSVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[85], binaryId, BinaryenGeUVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[86], binaryId, BinaryenEqVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[87], binaryId, BinaryenNeVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[88], binaryId, BinaryenLtSVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[89], binaryId, BinaryenGtSVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[90], binaryId, BinaryenLeSVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[91], binaryId, BinaryenGeSVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[92], binaryId, BinaryenEqVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[93], binaryId, BinaryenNeVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[94], binaryId, BinaryenLtVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[95], binaryId, BinaryenGtVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[96], binaryId, BinaryenLeVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[97], binaryId, BinaryenGeVecF32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[98], binaryId, BinaryenEqVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[99], binaryId, BinaryenNeVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[100], binaryId, BinaryenLtVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[101], binaryId, BinaryenGtVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[102], binaryId, BinaryenLeVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[103], binaryId, BinaryenGeVecF64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[104], binaryId, BinaryenNarrowSVecI16x8ToVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[105], binaryId, BinaryenNarrowUVecI16x8ToVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[106], binaryId, BinaryenNarrowSVecI32x4ToVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[107], binaryId, BinaryenNarrowUVecI32x4ToVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[108], binaryId, BinaryenDotSVecI16x8ToVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[109], binaryId, BinaryenExtMulLowSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[110], binaryId, BinaryenExtMulHighSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[111], binaryId, BinaryenExtMulLowUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[112], binaryId, BinaryenExtMulHighUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[113], binaryId, BinaryenExtMulLowSVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[114], binaryId, BinaryenExtMulHighSVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[115], binaryId, BinaryenExtMulLowUVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[116], binaryId, BinaryenExtMulHighUVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[117], binaryId, BinaryenExtMulLowSVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[118], binaryId, BinaryenExtMulHighSVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[119], binaryId, BinaryenExtMulLowUVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[57].groups[120], binaryId, BinaryenExtMulHighUVecI64x2());

    /* stack-operating type: (v128 v128 v128) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[58], 3, 1, 1);
    FILL_GRP_TYPE(inst_grps[58].required, v128, v128, v128);
    FILL_GRP_TYPE(inst_grps[58].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[58].groups[0], simdTernaryId, BinaryenBitselectVec128());

    /* stack-operating type: (v128) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[59], 1, 1, 5);
    FILL_GRP_TYPE(inst_grps[59].required, v128);
    FILL_GRP_TYPE(inst_grps[59].produced, i32);
    FILL_INSTR_TYPE_OPT(inst_grps[59].groups[0], unaryId, BinaryenAnyTrueVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[59].groups[1], unaryId, BinaryenBitmaskVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[59].groups[2], unaryId, BinaryenBitmaskVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[59].groups[3], unaryId, BinaryenBitmaskVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[59].groups[4], unaryId, BinaryenBitmaskVecI64x2());

    /* stack-operating type: (i32) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[60], 1, 1, 16);
    FILL_GRP_TYPE(inst_grps[60].required, i32);
    FILL_GRP_TYPE(inst_grps[60].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[0], unaryId, BinaryenSplatVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[1], unaryId, BinaryenSplatVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[2], unaryId, BinaryenSplatVecI32x4());
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[60].groups[3], loadId, v128, 16, 0);
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[4], simdLoadId, BinaryenLoad8x8SVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[5], simdLoadId, BinaryenLoad8x8UVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[6], simdLoadId, BinaryenLoad16x4SVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[7], simdLoadId, BinaryenLoad16x4UVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[8], simdLoadId, BinaryenLoad32x2SVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[9], simdLoadId, BinaryenLoad32x2UVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[10], simdLoadId, BinaryenLoad8SplatVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[11], simdLoadId, BinaryenLoad16SplatVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[12], simdLoadId, BinaryenLoad32SplatVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[13], simdLoadId, BinaryenLoad64SplatVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[14], simdLoadId, BinaryenLoad32ZeroVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[60].groups[15], simdLoadId, BinaryenLoad64ZeroVec128());

    /* stack-operating type: (i64) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[61], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[61].required, i64);
    FILL_GRP_TYPE(inst_grps[61].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[61].groups[0], unaryId, BinaryenSplatVecI64x2());

    /* stack-operating type: (f32) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[62], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[62].required, f32);
    FILL_GRP_TYPE(inst_grps[62].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[62].groups[0], unaryId, BinaryenSplatVecF32x4());

    /* stack-operating type: (f64) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[63], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[63].required, f64);
    FILL_GRP_TYPE(inst_grps[63].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[63].groups[0], unaryId, BinaryenSplatVecF64x2());

    /* stack-operating type: (v128) -> (i32) */
    INITIALIZE_INSTR_GRP(inst_grps[64], 1, 1, 9);
    FILL_GRP_TYPE(inst_grps[64].required, v128);
    FILL_GRP_TYPE(inst_grps[64].produced, i32);
    FILL_INSTR_TYPE_OPT(inst_grps[64].groups[0], simdExtractId, BinaryenExtractLaneSVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[64].groups[1], simdExtractId, BinaryenExtractLaneUVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[64].groups[2], simdExtractId, BinaryenExtractLaneSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[64].groups[3], simdExtractId, BinaryenExtractLaneUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[64].groups[4], simdExtractId, BinaryenExtractLaneVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[64].groups[5], unaryId, BinaryenAllTrueVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[64].groups[6], unaryId, BinaryenAllTrueVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[64].groups[7], unaryId, BinaryenAllTrueVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[64].groups[8], unaryId, BinaryenAllTrueVecI64x2());

    /* stack-operating type: (v128) -> (i64) */
    INITIALIZE_INSTR_GRP(inst_grps[65], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[65].required, v128);
    FILL_GRP_TYPE(inst_grps[65].produced, i64);
    FILL_INSTR_TYPE_OPT(inst_grps[65].groups[0], simdExtractId, BinaryenExtractLaneVecI64x2());

    /* stack-operating type: (v128) -> (f32) */
    INITIALIZE_INSTR_GRP(inst_grps[66], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[66].required, v128);
    FILL_GRP_TYPE(inst_grps[66].produced, f32);
    FILL_INSTR_TYPE_OPT(inst_grps[66].groups[0], simdExtractId, BinaryenExtractLaneVecF32x4());

    /* stack-operating type: (v128) -> (f64) */
    INITIALIZE_INSTR_GRP(inst_grps[67], 1, 1, 1);
    FILL_GRP_TYPE(inst_grps[67].required, v128);
    FILL_GRP_TYPE(inst_grps[67].produced, f64);
    FILL_INSTR_TYPE_OPT(inst_grps[67].groups[0], simdExtractId, BinaryenExtractLaneVecF64x2());

    /* stack-operating type: (v128 i32) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[68], 2, 1, 15);
    FILL_GRP_TYPE(inst_grps[68].required, v128, i32);
    FILL_GRP_TYPE(inst_grps[68].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[0], simdReplaceId, BinaryenReplaceLaneVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[1], simdReplaceId, BinaryenReplaceLaneVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[2], simdReplaceId, BinaryenReplaceLaneVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[3], simdShiftId, BinaryenShlVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[4], simdShiftId, BinaryenShrSVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[5], simdShiftId, BinaryenShrUVecI8x16());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[6], simdShiftId, BinaryenShlVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[7], simdShiftId, BinaryenShrSVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[8], simdShiftId, BinaryenShrUVecI16x8());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[9], simdShiftId, BinaryenShlVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[10], simdShiftId, BinaryenShrSVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[11], simdShiftId, BinaryenShrUVecI32x4());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[12], simdShiftId, BinaryenShlVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[13], simdShiftId, BinaryenShrSVecI64x2());
    FILL_INSTR_TYPE_OPT(inst_grps[68].groups[14], simdShiftId, BinaryenShrUVecI64x2());

    /* stack-operating type: (v128 i64) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[69], 2, 1, 1);
    FILL_GRP_TYPE(inst_grps[69].required, v128, i64);
    FILL_GRP_TYPE(inst_grps[69].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[69].groups[0], simdReplaceId, BinaryenReplaceLaneVecI64x2());

    /* stack-operating type: (v128 f32) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[70], 2, 1, 1);
    FILL_GRP_TYPE(inst_grps[70].required, v128, f32);
    FILL_GRP_TYPE(inst_grps[70].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[70].groups[0], simdReplaceId, BinaryenReplaceLaneVecF32x4());

    /* stack-operating type: (v128 f64) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[71], 2, 1, 1);
    FILL_GRP_TYPE(inst_grps[71].required, v128, f64);
    FILL_GRP_TYPE(inst_grps[71].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[71].groups[0], simdReplaceId, BinaryenReplaceLaneVecF64x2());

    /* stack-operating type: (v128) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[72], 1, 0, 1);
    FILL_GRP_TYPE(inst_grps[72].required, v128);
    FILL_INSTR_TYPE_OPD(inst_grps[72].groups[0], dropId, v128);

    /* stack-operating type: (v128 v128 i32) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[73], 3, 1, 1);
    FILL_GRP_TYPE(inst_grps[73].required, v128, v128, i32);
    FILL_GRP_TYPE(inst_grps[73].produced, v128);
    FILL_INSTR_TYPE_OPD(inst_grps[73].groups[0], selectId, v128);

    /* stack-operating type: (i32 v128) -> () */
    INITIALIZE_INSTR_GRP(inst_grps[74], 2, 0, 5);
    FILL_GRP_TYPE(inst_grps[74].required, i32, v128);
    FILL_INSTR_TYPE_OPD_MEM(inst_grps[74].groups[0], storeId, v128, 16, 0);
    FILL_INSTR_TYPE_OPT(inst_grps[74].groups[1], simdLoadStoreLaneId, BinaryenStore8LaneVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[74].groups[2], simdLoadStoreLaneId, BinaryenStore16LaneVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[74].groups[3], simdLoadStoreLaneId, BinaryenStore32LaneVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[74].groups[4], simdLoadStoreLaneId, BinaryenStore64LaneVec128());

    /* stack-operating type: (i32 v128) -> (v128) */
    INITIALIZE_INSTR_GRP(inst_grps[75], 2, 1, 4);
    FILL_GRP_TYPE(inst_grps[75].required, i32, v128);
    FILL_GRP_TYPE(inst_grps[75].produced, v128);
    FILL_INSTR_TYPE_OPT(inst_grps[75].groups[0], simdLoadStoreLaneId, BinaryenLoad8LaneVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[75].groups[1], simdLoadStoreLaneId, BinaryenLoad16LaneVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[75].groups[2], simdLoadStoreLaneId, BinaryenLoad32LaneVec128());
    FILL_INSTR_TYPE_OPT(inst_grps[75].groups[3], simdLoadStoreLaneId, BinaryenLoad64LaneVec128());

#endif

    data->instr_grps = inst_grps;
    data->num_instr_grps = num_inst_grps;

}

void wasm_destroy_instr_grps(wasm_mutator_t* data) {
    
    for (int i = 0; i < data->num_instr_grps; i++) {

        InstrGrp cur_grp = data->instr_grps[i];
        free(cur_grp.required);
        free(cur_grp.produced);
        free(cur_grp.groups);

    }

    free(data->instr_grps);

}
