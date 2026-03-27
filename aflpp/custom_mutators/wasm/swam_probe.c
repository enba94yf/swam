#include "swam_probe.h"

#define SWAM_MARKER_BEFORE "__swam_marker_before"
#define SWAM_MARKER_AFTER "__swam_marker_after"
#define SWAM_BULK_SEGMENT_NAME "__swam_bulk_seg"
#define SWAM_PAGE_SIZE_BYTES 65536

static BinaryenExpressionRef swam_make_i32_const(BinaryenModuleRef mod, int32_t value) {

    return BinaryenConst(mod, BinaryenLiteralInt32(value));

}

static BinaryenExpressionRef swam_make_index_const(BinaryenModuleRef mod,
                                                   int64_t value,
                                                   bool is_memory64) {

    if (is_memory64) {
        return BinaryenConst(mod, BinaryenLiteralInt64(value));
    }

    return swam_make_i32_const(mod, (int32_t)value);

}

static BinaryenType swam_valtype_to_binaryen(SwamValType vt) {

    switch (vt) {
        case SWAM_VALTYPE_I32: return BinaryenTypeInt32();
        case SWAM_VALTYPE_I64: return BinaryenTypeInt64();
        case SWAM_VALTYPE_F32: return BinaryenTypeFloat32();
        case SWAM_VALTYPE_F64: return BinaryenTypeFloat64();
        case SWAM_VALTYPE_V128: return BinaryenTypeVec128();
        default: return BinaryenTypeInt32();
    }

}

static BinaryenExpressionRef swam_make_v128_const(BinaryenModuleRef mod) {

    uint8_t bytes[16] = {0};

    return BinaryenConst(mod, BinaryenLiteralVec128(bytes));

}

static BinaryenExpressionRef swam_make_dummy_value(BinaryenModuleRef mod, SwamValType vt) {

    switch (vt) {
        case SWAM_VALTYPE_I32: return BinaryenConst(mod, BinaryenLiteralInt32(0x42));
        case SWAM_VALTYPE_I64: return BinaryenConst(mod, BinaryenLiteralInt64(0x42));
        case SWAM_VALTYPE_F32: return BinaryenConst(mod, BinaryenLiteralFloat32(1.0f));
        case SWAM_VALTYPE_F64: return BinaryenConst(mod, BinaryenLiteralFloat64(1.0));
        case SWAM_VALTYPE_V128: return swam_make_v128_const(mod);
        default: return BinaryenConst(mod, BinaryenLiteralInt32(0x42));
    }

}

static BinaryenOp swam_simd_load_op(SwamOpKind kind) {

    switch (kind) {
        case SWAM_OP_V128_LOAD8X8_S: return BinaryenLoad8x8SVec128();
        case SWAM_OP_V128_LOAD8X8_U: return BinaryenLoad8x8UVec128();
        case SWAM_OP_V128_LOAD16X4_S: return BinaryenLoad16x4SVec128();
        case SWAM_OP_V128_LOAD16X4_U: return BinaryenLoad16x4UVec128();
        case SWAM_OP_V128_LOAD32X2_S: return BinaryenLoad32x2SVec128();
        case SWAM_OP_V128_LOAD32X2_U: return BinaryenLoad32x2UVec128();
        case SWAM_OP_V128_LOAD8_SPLAT: return BinaryenLoad8SplatVec128();
        case SWAM_OP_V128_LOAD16_SPLAT: return BinaryenLoad16SplatVec128();
        case SWAM_OP_V128_LOAD32_SPLAT: return BinaryenLoad32SplatVec128();
        case SWAM_OP_V128_LOAD64_SPLAT: return BinaryenLoad64SplatVec128();
        case SWAM_OP_V128_LOAD32_ZERO: return BinaryenLoad32ZeroVec128();
        case SWAM_OP_V128_LOAD64_ZERO: return BinaryenLoad64ZeroVec128();
        default: return BinaryenLoad8SplatVec128();
    }

}

static BinaryenOp swam_simd_lane_op(SwamOpKind kind) {

    switch (kind) {
        case SWAM_OP_V128_LOAD8_LANE: return BinaryenLoad8LaneVec128();
        case SWAM_OP_V128_LOAD16_LANE: return BinaryenLoad16LaneVec128();
        case SWAM_OP_V128_LOAD32_LANE: return BinaryenLoad32LaneVec128();
        case SWAM_OP_V128_LOAD64_LANE: return BinaryenLoad64LaneVec128();
        case SWAM_OP_V128_STORE8_LANE: return BinaryenStore8LaneVec128();
        case SWAM_OP_V128_STORE16_LANE: return BinaryenStore16LaneVec128();
        case SWAM_OP_V128_STORE32_LANE: return BinaryenStore32LaneVec128();
        case SWAM_OP_V128_STORE64_LANE: return BinaryenStore64LaneVec128();
        default: return BinaryenLoad8LaneVec128();
    }

}

static BinaryenOp swam_rmw_binaryen_op(SwamOpKind kind) {

    switch (kind) {
        case SWAM_OP_I32_ATOMIC_RMW_ADD:
        case SWAM_OP_I64_ATOMIC_RMW_ADD:
        case SWAM_OP_I32_ATOMIC_RMW8_ADD_U:
        case SWAM_OP_I32_ATOMIC_RMW16_ADD_U:
        case SWAM_OP_I64_ATOMIC_RMW8_ADD_U:
        case SWAM_OP_I64_ATOMIC_RMW16_ADD_U:
        case SWAM_OP_I64_ATOMIC_RMW32_ADD_U:
            return BinaryenAtomicRMWAdd();
        case SWAM_OP_I32_ATOMIC_RMW_SUB:
        case SWAM_OP_I64_ATOMIC_RMW_SUB:
        case SWAM_OP_I32_ATOMIC_RMW8_SUB_U:
        case SWAM_OP_I32_ATOMIC_RMW16_SUB_U:
        case SWAM_OP_I64_ATOMIC_RMW8_SUB_U:
        case SWAM_OP_I64_ATOMIC_RMW16_SUB_U:
        case SWAM_OP_I64_ATOMIC_RMW32_SUB_U:
            return BinaryenAtomicRMWSub();
        case SWAM_OP_I32_ATOMIC_RMW_AND:
        case SWAM_OP_I64_ATOMIC_RMW_AND:
        case SWAM_OP_I32_ATOMIC_RMW8_AND_U:
        case SWAM_OP_I32_ATOMIC_RMW16_AND_U:
        case SWAM_OP_I64_ATOMIC_RMW8_AND_U:
        case SWAM_OP_I64_ATOMIC_RMW16_AND_U:
        case SWAM_OP_I64_ATOMIC_RMW32_AND_U:
            return BinaryenAtomicRMWAnd();
        case SWAM_OP_I32_ATOMIC_RMW_OR:
        case SWAM_OP_I64_ATOMIC_RMW_OR:
        case SWAM_OP_I32_ATOMIC_RMW8_OR_U:
        case SWAM_OP_I32_ATOMIC_RMW16_OR_U:
        case SWAM_OP_I64_ATOMIC_RMW8_OR_U:
        case SWAM_OP_I64_ATOMIC_RMW16_OR_U:
        case SWAM_OP_I64_ATOMIC_RMW32_OR_U:
            return BinaryenAtomicRMWOr();
        case SWAM_OP_I32_ATOMIC_RMW_XOR:
        case SWAM_OP_I64_ATOMIC_RMW_XOR:
        case SWAM_OP_I32_ATOMIC_RMW8_XOR_U:
        case SWAM_OP_I32_ATOMIC_RMW16_XOR_U:
        case SWAM_OP_I64_ATOMIC_RMW8_XOR_U:
        case SWAM_OP_I64_ATOMIC_RMW16_XOR_U:
        case SWAM_OP_I64_ATOMIC_RMW32_XOR_U:
            return BinaryenAtomicRMWXor();
        case SWAM_OP_I32_ATOMIC_RMW_XCHG:
        case SWAM_OP_I64_ATOMIC_RMW_XCHG:
        case SWAM_OP_I32_ATOMIC_RMW8_XCHG_U:
        case SWAM_OP_I32_ATOMIC_RMW16_XCHG_U:
        case SWAM_OP_I64_ATOMIC_RMW8_XCHG_U:
        case SWAM_OP_I64_ATOMIC_RMW16_XCHG_U:
        case SWAM_OP_I64_ATOMIC_RMW32_XCHG_U:
            return BinaryenAtomicRMWXchg();
        default:
            return BinaryenAtomicRMWAdd();
    }

}

static BinaryenExpressionRef swam_build_memory_bytes(BinaryenModuleRef mod,
                                                     const char* mem_name,
                                                     bool is_memory64) {

    BinaryenOp mul_op = is_memory64 ? BinaryenMulInt64() : BinaryenMulInt32();

    return BinaryenBinary(mod,
                          mul_op,
                          BinaryenMemorySize(mod, mem_name, is_memory64),
                          swam_make_index_const(mod,
                                                SWAM_PAGE_SIZE_BYTES,
                                                is_memory64));

}

static BinaryenExpressionRef swam_build_base_address(BinaryenModuleRef mod,
                                                     const SwamScenario* scenario,
                                                     const char* mem_name,
                                                     bool is_memory64) {

    BinaryenOp add_op = is_memory64 ? BinaryenAddInt64() : BinaryenAddInt32();
    BinaryenOp div_u_op = is_memory64 ? BinaryenDivUInt64() : BinaryenDivUInt32();
    BinaryenOp sub_op = is_memory64 ? BinaryenSubInt64() : BinaryenSubInt32();

    if (scenario == NULL || scenario->op_class == SWAM_OPCLASS_MEMORY_GROW ||
        scenario->access_width == 0U) {
        return swam_make_index_const(mod, 0, is_memory64);
    }

    /* OVERFLOW: base = 2^N - W, represented as a negative signed constant.
       With a static offset of W, the mathematical effective address is 2^N
       while a buggy modular computation wraps to 0. */
    if (scenario->wrap == SWAM_WRAP_OVERFLOW) {
        if (is_memory64) {
            return swam_make_index_const(mod,
                                         -(int64_t)scenario->access_width,
                                         true);
        }

        return swam_make_i32_const(mod, -(int32_t)scenario->access_width);
    }

    if (scenario->align == SWAM_ALIGN_ALIGNED &&
        scenario->layout == SWAM_LAYOUT_OOB) {
        return swam_build_memory_bytes(mod, mem_name, is_memory64);
    }

    if (scenario->align == SWAM_ALIGN_MISALIGNED) {
        switch (scenario->layout) {
            case SWAM_LAYOUT_INTERIOR:
                return BinaryenBinary(mod,
                                      add_op,
                                      BinaryenBinary(mod,
                                                     div_u_op,
                                                     swam_build_memory_bytes(mod,
                                                                             mem_name,
                                                                             is_memory64),
                                                     swam_make_index_const(mod,
                                                                           2,
                                                                           is_memory64)),
                                      swam_make_index_const(mod, 1, is_memory64));
            case SWAM_LAYOUT_BOUNDARY:
                return BinaryenBinary(
                  mod,
                  sub_op,
                  BinaryenBinary(mod,
                                 sub_op,
                                 swam_build_memory_bytes(mod, mem_name, is_memory64),
                                 swam_make_index_const(mod,
                                                       (int64_t)scenario->access_width,
                                                       is_memory64)),
                  swam_make_index_const(mod, 1, is_memory64));
            case SWAM_LAYOUT_OOB:
            default:
                return BinaryenBinary(
                  mod,
                  add_op,
                  BinaryenBinary(mod,
                                 sub_op,
                                 swam_build_memory_bytes(mod, mem_name, is_memory64),
                                 swam_make_index_const(mod,
                                                       (int64_t)scenario->access_width,
                                                       is_memory64)),
                  swam_make_index_const(mod, 1, is_memory64));
        }
    }

    switch (scenario->layout) {
        case SWAM_LAYOUT_BOUNDARY:
            return BinaryenBinary(mod,
                                  sub_op,
                                  swam_build_memory_bytes(mod, mem_name, is_memory64),
                                  swam_make_index_const(mod,
                                                        (int64_t)scenario->access_width,
                                                        is_memory64));
        case SWAM_LAYOUT_OOB:
            return BinaryenBinary(
              mod,
              add_op,
              BinaryenBinary(mod,
                             sub_op,
                             swam_build_memory_bytes(mod, mem_name, is_memory64),
                             swam_make_index_const(mod,
                                                   (int64_t)scenario->access_width,
                                                   is_memory64)),
              swam_make_index_const(mod, 1, is_memory64));
        case SWAM_LAYOUT_INTERIOR:
        default:
            return BinaryenBinary(mod,
                                  div_u_op,
                                  swam_build_memory_bytes(mod, mem_name, is_memory64),
                                  swam_make_index_const(mod, 2, is_memory64));
    }

}

static BinaryenExpressionRef swam_build_marker_set(BinaryenModuleRef mod,
                                                   const char* marker_name,
                                                   uint32_t scenario_id) {

    return BinaryenGlobalSet(mod, marker_name, swam_make_i32_const(mod, (int32_t)scenario_id));

}

static void swam_ensure_marker_global(BinaryenModuleRef mod, const char* marker_name) {

    if (BinaryenGetGlobal(mod, marker_name) == NULL) {
        BinaryenAddGlobal(mod,
                          marker_name,
                          BinaryenTypeInt32(),
                          true,
                          swam_make_i32_const(mod, 0));
    }

    if (BinaryenGetExport(mod, marker_name) == NULL) {
        BinaryenAddGlobalExport(mod, marker_name, marker_name);
    }

}

static BinaryenExpressionRef swam_build_target_op(BinaryenModuleRef mod,
                                                  const SwamScenario* scenario,
                                                  BinaryenIndex addr_local_idx,
                                                  const char* mem_name,
                                                  bool is_memory64) {

    BinaryenExpressionRef ptr = NULL;
    BinaryenExpressionRef probe_instr = NULL;
    BinaryenType ptr_type = is_memory64 ? BinaryenTypeInt64() : BinaryenTypeInt32();
    BinaryenType value_type = BinaryenTypeInt32();
    uint32_t static_offset = 0U;

    if (scenario == NULL) return BinaryenNop(mod);

    value_type = swam_valtype_to_binaryen(scenario->value_type);
    static_offset =
      (scenario->wrap == SWAM_WRAP_OVERFLOW) ? scenario->access_width : 0U;

    switch (scenario->op_class) {
        case SWAM_OPCLASS_LOAD:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenDrop(mod,
                                BinaryenLoad(mod,
                                             scenario->access_width,
                                             scenario->is_signed,
                                             static_offset,
                                             0,
                                             value_type,
                                             ptr,
                                             mem_name));
        case SWAM_OPCLASS_STORE:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenStore(mod,
                                 scenario->access_width,
                                 static_offset,
                                 0,
                                 ptr,
                                 swam_make_dummy_value(mod, scenario->value_type),
                                 value_type,
                                 mem_name);
        case SWAM_OPCLASS_ATOMIC_LOAD:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenDrop(mod,
                                BinaryenAtomicLoad(mod,
                                                   scenario->access_width,
                                                   static_offset,
                                                   value_type,
                                                   ptr,
                                                   mem_name));
        case SWAM_OPCLASS_ATOMIC_STORE:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenAtomicStore(mod,
                                       scenario->access_width,
                                       static_offset,
                                       ptr,
                                       swam_make_dummy_value(mod, scenario->value_type),
                                       value_type,
                                       mem_name);
        case SWAM_OPCLASS_ATOMIC_RMW:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenDrop(mod,
                                BinaryenAtomicRMW(mod,
                                                  swam_rmw_binaryen_op(scenario->op_kind),
                                                  scenario->access_width,
                                                  static_offset,
                                                  ptr,
                                                  swam_make_dummy_value(mod, scenario->value_type),
                                                  value_type,
                                                  mem_name));
        case SWAM_OPCLASS_ATOMIC_CMPXCHG:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenDrop(mod,
                                BinaryenAtomicCmpxchg(mod,
                                                      scenario->access_width,
                                                      static_offset,
                                                      ptr,
                                                      swam_make_dummy_value(mod, scenario->value_type),
                                                      swam_make_dummy_value(mod, scenario->value_type),
                                                      value_type,
                                                      mem_name));
        case SWAM_OPCLASS_ATOMIC_WAIT:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            probe_instr = BinaryenAtomicWait(mod,
                                             ptr,
                                             swam_make_dummy_value(mod, scenario->value_type),
                                             BinaryenConst(mod, BinaryenLiteralInt64(0)),
                                             value_type,
                                             mem_name);
            BinaryenAtomicWaitSetOffset(probe_instr, static_offset);
            return BinaryenDrop(mod, probe_instr);
        case SWAM_OPCLASS_ATOMIC_NOTIFY:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            probe_instr = BinaryenAtomicNotify(mod,
                                               ptr,
                                               swam_make_i32_const(mod, 0),
                                               mem_name);
            BinaryenAtomicNotifySetOffset(probe_instr, static_offset);
            return BinaryenDrop(mod, probe_instr);
        case SWAM_OPCLASS_MEMORY_GROW:
            return BinaryenDrop(
              mod,
              BinaryenMemoryGrow(mod,
                                 swam_make_index_const(mod, 1, is_memory64),
                                 mem_name,
                                 is_memory64));
        case SWAM_OPCLASS_SIMD_LOAD:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            if (scenario->op_kind == SWAM_OP_V128_LOAD) {
                return BinaryenDrop(mod,
                                    BinaryenLoad(mod,
                                                 16U,
                                                 false,
                                                 static_offset,
                                                 0,
                                                 BinaryenTypeVec128(),
                                                 ptr,
                                                 mem_name));
            }
            return BinaryenDrop(mod,
                                BinaryenSIMDLoad(mod,
                                                 swam_simd_load_op(scenario->op_kind),
                                                 static_offset,
                                                 scenario->access_width,
                                                 ptr,
                                                 mem_name));
        case SWAM_OPCLASS_SIMD_LOAD_LANE:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenDrop(mod,
                                BinaryenSIMDLoadStoreLane(mod,
                                                          swam_simd_lane_op(scenario->op_kind),
                                                          static_offset,
                                                          scenario->access_width,
                                                          0,
                                                          ptr,
                                                          swam_make_v128_const(mod),
                                                          mem_name));
        case SWAM_OPCLASS_SIMD_STORE:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenStore(mod,
                                 16U,
                                 static_offset,
                                 0,
                                 ptr,
                                 swam_make_v128_const(mod),
                                 BinaryenTypeVec128(),
                                 mem_name);
        case SWAM_OPCLASS_SIMD_STORE_LANE:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenSIMDLoadStoreLane(mod,
                                             swam_simd_lane_op(scenario->op_kind),
                                             static_offset,
                                             scenario->access_width,
                                             0,
                                             ptr,
                                             swam_make_v128_const(mod),
                                             mem_name);
        case SWAM_OPCLASS_BULK_COPY:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenMemoryCopy(mod,
                                      ptr,
                                      swam_make_index_const(mod, 0, is_memory64),
                                      swam_make_index_const(mod, 1, is_memory64),
                                      mem_name,
                                      mem_name);
        case SWAM_OPCLASS_BULK_FILL:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenMemoryFill(mod,
                                      ptr,
                                      swam_make_i32_const(mod, 0x42),
                                      swam_make_index_const(mod, 1, is_memory64),
                                      mem_name);
        case SWAM_OPCLASS_BULK_INIT:
            ptr = BinaryenLocalGet(mod, addr_local_idx, ptr_type);
            return BinaryenMemoryInit(mod,
                                      SWAM_BULK_SEGMENT_NAME,
                                      ptr,
                                      swam_make_index_const(mod, 0, is_memory64),
                                      swam_make_index_const(mod, 1, is_memory64),
                                      mem_name);
        default:
            return BinaryenNop(mod);
    }

}

void swam_add_marker_globals(BinaryenModuleRef mod) {

    if (mod == NULL) return;

    swam_ensure_marker_global(mod, SWAM_MARKER_BEFORE);
    swam_ensure_marker_global(mod, SWAM_MARKER_AFTER);

}

BinaryenExpressionRef swam_build_probe(BinaryenModuleRef mod,
                                       const SwamScenario* scenario,
                                       BinaryenIndex addr_local_idx,
                                       const char* mem_name,
                                       bool is_memory64) {

    BinaryenExpressionRef children[4];

    if (mod == NULL) return NULL;
    if (scenario == NULL) return BinaryenNop(mod);

    swam_add_marker_globals(mod);

    children[0] = swam_build_marker_set(mod, SWAM_MARKER_BEFORE, scenario->id);
    children[1] = BinaryenLocalSet(mod,
                                   addr_local_idx,
                                   swam_build_base_address(mod,
                                                           scenario,
                                                           mem_name,
                                                           is_memory64));
    children[2] =
      swam_build_target_op(mod, scenario, addr_local_idx, mem_name, is_memory64);
    children[3] = swam_build_marker_set(mod, SWAM_MARKER_AFTER, scenario->id);

    return BinaryenBlock(mod, NULL, children, 4, BinaryenTypeNone());

}
