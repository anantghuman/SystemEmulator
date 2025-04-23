/**************************************************************************
 * C S 429 system emulator
 *
 * Bubble and stall checking logic.
 * STUDENT TO-DO:
 * Implement logic for hazard handling.
 *
 * handle_hazards is called from proc.c with the appropriate
 * parameters already set, you must implement the logic for it.
 *
 * You may optionally use the other three helper functions to
 * make it easier to follow your logic.
 **************************************************************************/

#include "hazard_control.h"

#include "machine.h"

extern machine_t guest;
extern mem_status_t dmem_status;

/* Use this method to actually bubble/stall a pipeline stage.
 * Call it in handle_hazards(). Do not modify this code. */
void pipe_control_stage(proc_stage_t stage, bool bubble, bool stall) {
    pipe_reg_t *pipe;
    switch (stage) {
        case S_FETCH:
            pipe = F_instr;
            break;
        case S_DECODE:
            pipe = D_instr;
            break;
        case S_EXECUTE:
            pipe = X_instr;
            break;
        case S_MEMORY:
            pipe = M_instr;
            break;
        case S_WBACK:
            pipe = W_instr;
            break;
        default:
            printf("Error: incorrect stage provided to pipe control.\n");
            return;
    }
    if (bubble && stall) {
        printf("Error: cannot bubble and stall at the same time.\n");
        pipe->ctl = P_ERROR;
    }
    // If we were previously in an error state, stay there.
    if (pipe->ctl == P_ERROR) return;

    if (bubble) {
        pipe->ctl = P_BUBBLE;
    } else if (stall) {
        pipe->ctl = P_STALL;
    } else {
        pipe->ctl = P_LOAD;
    }
}

bool check_ret_hazard(opcode_t D_opcode) { return D_out->op == OP_RET; }
#ifdef EC
bool check_br_hazard(opcode_t D_opcode) {
    // if (D_opcode == OP_B || D_opcode == OP_B_COND)
    //   return true;
    return false;
}

bool check_cb_hazard(opcode_t D_opcode, uint64_t D_val_a) {
    // if (D_opcode == OP_CBZ || D_opcode == OP_CBNZ) {
    //   if (D_val_a == 0)
    //     return true;
    // }
    return false;
}
#endif

bool check_mispred_branch_hazard(opcode_t X_opcode, bool X_condval) {
    // Student TODO
    return X_out->op == OP_B_COND && !M_in->cond_holds;
}

bool check_load_use_hazard(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2,
                           opcode_t X_opcode, uint8_t X_dst) {
    // Student TODO
    return X_out->op == OP_LDUR &&
           ((X_out->dst == D_src1) || (X_out->dst == D_src2));
}

bool error(stat_t status) {
    // Student TODO
    return status != STAT_BUB && status != STAT_AOK;
}

comb_logic_t handle_hazards(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2,
                            uint64_t D_val_a, opcode_t X_opcode, uint8_t X_dst,
                            bool X_condval) {
    /* Students: Change this code */
    // This will need to be updated in week 3, good enough for week 1-2

    // Student TODO WEEK 3
    if (error(X_out->status)) {
        pipe_control_stage(S_FETCH, false, true);
        pipe_control_stage(S_DECODE, false, true);
        pipe_control_stage(S_EXECUTE, false, false);
        pipe_control_stage(S_MEMORY, false, false);
        pipe_control_stage(S_WBACK, false, false);
    } else if (error(W_in->status)) {
        pipe_control_stage(S_FETCH, false, true);
        pipe_control_stage(S_DECODE, false, true);
        pipe_control_stage(S_EXECUTE, false, true);
        pipe_control_stage(S_MEMORY, false, true);
        pipe_control_stage(S_WBACK, false, false);
    } else if (error(W_out->status)) {
        pipe_control_stage(S_FETCH, false, true);
        pipe_control_stage(S_DECODE, false, true);
        pipe_control_stage(S_EXECUTE, false, true);
        pipe_control_stage(S_MEMORY, false, true);
        pipe_control_stage(S_WBACK, false, true);
    } else if (check_load_use_hazard(D_opcode, D_src1, D_src2, X_opcode,
                                     X_dst)) {
        pipe_control_stage(S_FETCH, false, true);
        pipe_control_stage(S_DECODE, false, true);
        pipe_control_stage(S_EXECUTE, true, false);
        pipe_control_stage(S_MEMORY, false, false);
        pipe_control_stage(S_WBACK, false, false);
    } else if (check_mispred_branch_hazard(X_opcode, X_condval)) {
        pipe_control_stage(S_FETCH, false, false);
        pipe_control_stage(S_DECODE, true, false);
        pipe_control_stage(S_EXECUTE, true, false);
        pipe_control_stage(S_MEMORY, false, false);
        pipe_control_stage(S_WBACK, false, false);
    } else if (check_ret_hazard(D_opcode)) {
        pipe_control_stage(S_FETCH, false, true);
        pipe_control_stage(S_DECODE, true, false);
        pipe_control_stage(S_EXECUTE, false, false);
        pipe_control_stage(S_MEMORY, false, false);
        pipe_control_stage(S_WBACK, false, false);
    } else if (dmem_status == IN_FLIGHT) {
        pipe_control_stage(S_FETCH,  false, true);
        pipe_control_stage(S_DECODE, false, true);
        pipe_control_stage(S_EXECUTE, false,  true);
        pipe_control_stage(S_MEMORY, false, true);
        pipe_control_stage(S_WBACK,  false, true);
    } else {
        pipe_control_stage(S_FETCH, false, false);
        pipe_control_stage(S_DECODE, false, false);
        pipe_control_stage(S_EXECUTE, false, false);
        pipe_control_stage(S_MEMORY, false, false);
        pipe_control_stage(S_WBACK, false, false);
    }
}
