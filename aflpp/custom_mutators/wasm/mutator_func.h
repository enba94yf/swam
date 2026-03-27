#ifndef _MUTATOR_FUNC_H
#define _MUTATOR_FUNC_H

#include "afl-fuzz.h"
#include "swam_scenarios.h"

#include <math.h>
#include <stdint.h>
#include <binaryen-c.h>

/* instruction groups */
typedef struct InstructionType {
    BinaryenExpressionId expr_id;    /* higher-level expression type                        */
    union {
        BinaryenType opd_type;       /* operand  type                                       */
        BinaryenOp   opt_type;       /* operator type                                       */
    };
    uint32_t union_type;             /* type currently used by union                        */

    uint8_t  mem_bytes;              /* bytes loaded/stored by memory instructions          */
    uint8_t  mem_signed;             /* signedness of the bytes in memory instructions      */
    uint64_t hash_value;             /* hash value for the ease of comparison               */
} InstrTy;

typedef struct InstructionGroup {
    BinaryenType* required;         /* value types required to be on the stack              */
    BinaryenType* produced;         /* value types produced on the stack by instruction     */
    uint32_t      num_req;          /* number of value types required                       */
    uint32_t      num_pro;          /* number of value types produced                       */

    InstrTy*      groups;           /* all instructions satisfying (required) -> (produced) */
    uint32_t      num_inst;         /* number of instructions belonging to this group       */
} InstrGrp;

/* context information */
typedef struct LocalContext {
    BinaryenIndex local_idx;        /* the index of current local inside function           */
    BinaryenType  local_type;       /* the (tuple) type of current local inside function    */

    bool          pre_added;        /* whether this local is pre-added to the context       */
} localCxt;

typedef struct LabelContext {
    const char*  label_name;        /* the name of corresponding label inside function      */
    BinaryenType label_type;        /* the type of corresponding label inside function      */

    bool         pre_added;         /* whether this label is pre-added to the context       */
} labelCxt;

/* fragment pool */
typedef struct Fragment {
    BinaryenFunctionRef   func;     /* the function that current fragment lives in.         */
    BinaryenExpressionRef expr;     /* the fragment expression.                             */
} Frag;

typedef struct FragmentGroup {
    Frag*        frags;             /* code fragments with matching stack type              */
    uint32_t     num_frags;         /* number of code fragments in this group               */
    BinaryenType frag_type;         /* the stack type of code fragments inside this group   */
} fragGrp;

/* name pool */
typedef struct NameGroup {
    uint32_t     name_cnt;          /* total number of names in the name pool              */

    list_t       free_names;        /* the list of available names in the name pool        */
    list_t       used_names;        /* the list of used names in the name pool             */
} nameGrp;

/* predefined macros */
#define CALL_MUTATION_MAX              3
#define BRANCH_MUTATION_MAX            3
#define DEFAULT_MUTATION_MAX           5
#define RECURSE_MUTATION_MAX           7
#define OPERATOR_MUTATION_MAX         10
#define SPLICE_INSERT_MUTATION_MAX     4
#define SPLICE_OVERWRITE_MUTATION_MAX  6

#define CUSTOM_SPLICE_HAVOC           32
#define CUSTOM_HAVOC_CYCLES           64

#define INITIALIZE_INSTR_GRP(INSTR_GRP, NUM_REQ, NUM_PRO, NUM_INSTR)    \
    do {                                                                \
                                                                        \
        INSTR_GRP.num_req  = NUM_REQ;                                   \
        INSTR_GRP.num_pro  = NUM_PRO;                                   \
        if (INSTR_GRP.num_req != 0)                                     \
            INSTR_GRP.required = calloc(NUM_REQ, sizeof(BinaryenType)); \
        if (INSTR_GRP.num_pro != 0)                                     \
            INSTR_GRP.produced = calloc(NUM_PRO, sizeof(BinaryenType)); \
                                                                        \
        INSTR_GRP.num_inst = NUM_INSTR;                                 \
        if (INSTR_GRP.num_inst != 0)                                    \
            INSTR_GRP.groups = calloc(NUM_INSTR, sizeof(InstrTy));      \
                                                                        \
    } while (0);

#define CONCATENATE(arg1, arg2) arg1##arg2

#define FILL_GRP_TYPE_1(IND, TYPE_ARRAY, TYPE, ...)                     \
    TYPE_ARRAY[IND] = TYPE;
#define FILL_GRP_TYPE_2(IND, TYPE_ARRAY, TYPE, ...)                     \
    TYPE_ARRAY[IND] = TYPE;                                             \
    FILL_GRP_TYPE_1(IND+1, TYPE_ARRAY, __VA_ARGS__)
#define FILL_GRP_TYPE_3(IND, TYPE_ARRAY, TYPE, ...)                     \
    TYPE_ARRAY[IND] = TYPE;                                             \
    FILL_GRP_TYPE_2(IND+1, TYPE_ARRAY, __VA_ARGS__)
#define FILL_GRP_TYPE_4(IND, TYPE_ARRAY, TYPE, ...)                     \
    TYPE_ARRAY[IND] = TYPE;                                             \
    FILL_GRP_TYPE_3(IND+1, TYPE_ARRAY, __VA_ARGS__)
#define FILL_GRP_TYPE_5(IND, TYPE_ARRAY, TYPE, ...)                     \
    TYPE_ARRAY[IND] = TYPE;                                             \
    FILL_GRP_TYPE_4(IND+1, TYPE_ARRAY, __VA_ARGS__)

#define GRP_TYPE_NARG(...)  GRP_TYPE_NARG_(__VA_ARGS__, GRP_TYPE_RESQ_N())
#define GRP_TYPE_NARG_(...) GRP_TYPE_ARG_N(__VA_ARGS__)
#define GRP_TYPE_ARG_N(_1, _2, _3, _4, _5, N, ...) N
#define GRP_TYPE_RESQ_N() 5, 4, 3, 2, 1, 0

#define FILL_GRP_TYPE_(N, TYPE_ARRAY, TYPE, ...) CONCATENATE(FILL_GRP_TYPE_, N)(0, TYPE_ARRAY, TYPE, __VA_ARGS__)
#define FILL_GRP_TYPE(TYPE_ARRAY, TYPE, ...) FILL_GRP_TYPE_(GRP_TYPE_NARG(TYPE, ## __VA_ARGS__), TYPE_ARRAY, TYPE, __VA_ARGS__)

#define FILL_INSTR_TYPE_OPD(INSTR, EXPR_ID, OPD_ID)                     \
    do {                                                                \
                                                                        \
        INSTR.expr_id    = EXPR_ID;                                     \
        INSTR.opd_type   = OPD_ID;                                      \
        INSTR.union_type = 0;                                           \
        INSTR.hash_value = djb2_hash((u8*)&INSTR, sizeof(InstrTy));     \
                                                                        \
    } while (0);

#define FILL_INSTR_TYPE_OPD_MEM(INSTR, EXPR_ID, OPD_ID, BYTES, SIGN)    \
    do {                                                                \
                                                                        \
        INSTR.expr_id    = EXPR_ID;                                     \
        INSTR.opd_type   = OPD_ID;                                      \
        INSTR.union_type = 0;                                           \
        INSTR.mem_bytes  = BYTES;                                       \
        INSTR.mem_signed = SIGN;                                        \
        INSTR.hash_value = djb2_hash((u8*)&INSTR, sizeof(InstrTy));     \
                                                                        \
    } while (0);

#define FILL_INSTR_TYPE_OPT(INSTR, EXPR_ID, OPT_ID)                     \
    do {                                                                \
                                                                        \
        INSTR.expr_id    = EXPR_ID;                                     \
        INSTR.opt_type   = OPT_ID;                                      \
        INSTR.union_type = 1;                                           \
        INSTR.hash_value = djb2_hash((u8*)&INSTR, sizeof(InstrTy));     \
                                                                        \
    } while (0);

struct SwamScheduler;

/* init/cleanup related */
typedef struct wasm_mutator {
    afl_state_t* afl;

    nameGrp*     name_grp;
    InstrGrp*    instr_grps;
    uint32_t     num_instr_grps;

    uint8_t*     name_out_buf;
    uint8_t*     mutated_out_buf;

    uint32_t     splice_cnt;
    uint32_t     recurse_cnt;
    uint32_t     interest_cnt;
    uint32_t     operator_cnt;
    uint32_t     br_target_cnt;
    uint32_t     call_target_cnt;

    /* SWAM exploit state */
    uint32_t     swam_last_scenario_id;      /* scenario ID used in previous exploit iteration */
    bool         swam_last_was_exploit;      /* whether previous iteration was exploit mode    */
    uint32_t     swam_exploit_cnt;           /* total exploit iterations (for describe())      */
    uint8_t*     swam_pending_module;        /* copy of last exploit module for bug saving     */
    size_t       swam_pending_module_size;   /* size of swam_pending_module                    */
    uint32_t     swam_bug_hits_total;        /* total bug detections before save capping       */
    uint8_t      swam_saved_bug_count[SWAM_SCENARIO_COUNT][2]; /* saved bugs per scenario/tag */
    uint32_t     swam_unknown_total[SWAM_SCENARIO_COUNT]; /* total unknown hits per scenario */
    uint32_t     swam_unknown_saved_notrap[SWAM_SCENARIO_COUNT]; /* saved NO_TRAP unknowns per scenario */
    struct SwamScheduler* swam_scheduler;    /* UCB1 scenario scheduler                        */
} wasm_mutator_t;

void wasm_init(wasm_mutator_t* data);
void wasm_deinit(wasm_mutator_t* data);
void wasm_init_name_grp(wasm_mutator_t* data);
void wasm_init_instr_grps(wasm_mutator_t* data);
void wasm_destroy_name_grp(wasm_mutator_t* data);
void wasm_destroy_instr_grps(wasm_mutator_t* data);

/* hash related */
uint64_t djb2_hash(unsigned char* str, int len);

/* generation functions */
BinaryenExpressionRef wasm_generate_instr_seqs_top_level(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, 
                                                         list_t* local_cxt, list_t* label_cxt, s32 depth);
BinaryenExpressionRef wasm_generate_instr_seqs_base_case(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, 
                                                         list_t* local_cxt, list_t* label_cxt, s32 depth);
BinaryenExpressionRef wasm_generate_one_call(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, list_t* local_cxt,
                                             list_t* label_cxt, s32 depth);
BinaryenExpressionRef wasm_generate_one_local_tee(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, list_t* local_cxt,
                                                  list_t* label_cxt, s32 depth);
BinaryenExpressionRef wasm_generate_one_call_indirect(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, list_t* local_cxt,
                                                      list_t* label_cxt, s32 depth);
BinaryenExpressionRef wasm_generate_one_control_transfer(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType result, list_t* local_cxt,
                                                         list_t* label_cxt, s32 depth);
BinaryenExpressionRef wasm_generate_one_local_global_set(wasm_mutator_t* data, BinaryenModuleRef mod, list_t* local_cxt, list_t* label_cxt,
                                                         bool use_local, s32 depth);
BinaryenExpressionRef wasm_generate_one_local_global_get(afl_state_t* afl, BinaryenModuleRef mod, BinaryenType result, list_t* local_cxt,
                                                         list_t* label_cxt, bool use_local, s32 depth);
BinaryenExpressionRef wasm_generate_one_simple_expr(afl_state_t* afl, BinaryenModuleRef mod, InstrTy new_instr, BinaryenExpressionRef* child, 
                                                    BinaryenIndex size);


/* mutator related */
u32 wasm_operators_mutate(wasm_mutator_t* data, BinaryenModuleRef mod);
u32 wasm_recursive_mutate(wasm_mutator_t* data, BinaryenModuleRef mod);
u32 wasm_branch_target_mutate(wasm_mutator_t* data, BinaryenModuleRef mod);
u32 wasm_interest_values_mutate(wasm_mutator_t* data, BinaryenModuleRef mod);
u32 wasm_call_target_function_mutate(wasm_mutator_t* data, BinaryenModuleRef mod);
u32 wasm_splicing_mutate(wasm_mutator_t* data, BinaryenModuleRef mod_1, BinaryenModuleRef mod_2, list_t* fragment_pool);

/* helper related */
void wasm_reset_name_grp(wasm_mutator_t* data);
char* wasm_find_available_name(wasm_mutator_t* data);
BinaryenType wasm_simplify_type(BinaryenType original);
InstrTy wasm_get_instr_type(BinaryenExpressionRef expr);
BinaryenType wasm_get_single_type(BinaryenExpressionRef expr);
void fisher_yates_shuffle(afl_state_t* afl, u32 arr[], size_t n);
int32_t wasm_find_instr_grps(wasm_mutator_t* data, InstrTy instr);
void wasm_fragmentize_module(BinaryenModuleRef mod, list_t* fragment_pool);
void wasm_set_memory(afl_state_t* afl, BinaryenModuleRef mod, bool add_seg);
void wasm_find_available_locals(BinaryenFunctionRef func, list_t* local_cxt);
void wasm_substitute_expr_fields(afl_state_t* afl, BinaryenExpressionRef expr, InstrTy new_instr);
BinaryenIndex wasm_get_children_expr(BinaryenExpressionRef parent, BinaryenExpressionRef** child);
void wasm_set_children_expr(BinaryenExpressionRef parent, BinaryenExpressionRef new_child, BinaryenIndex index);
bool wasm_find_available_labels(BinaryenExpressionRef container, BinaryenExpressionRef target, list_t* label_cxt);
const char* wasm_generate_one_func(wasm_mutator_t* data, BinaryenModuleRef mod, BinaryenType param, BinaryenType result);
bool wasm_splicing_fixup(wasm_mutator_t* data, BinaryenModuleRef cur_mod, BinaryenModuleRef spl_mod, Frag* current, Frag* target);
BinaryenIndex wasm_find_matching_funcs(BinaryenModuleRef mod, BinaryenType param, BinaryenType result, BinaryenFunctionRef** funcs,
                                       bool ignore_param);
#endif
