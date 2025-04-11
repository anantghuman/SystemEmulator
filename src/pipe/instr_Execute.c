/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Execute.c - Execute stage of instruction processing pipeline.
 **************************************************************************/

 #include <stdint.h>
 #include <stdbool.h>
 #include <assert.h>
 #include "instr.h"
 #include "instr_pipeline.h"
 #include "machine.h"
 #include "hw_elts.h"
 
 extern machine_t guest;
 extern mem_status_t dmem_status;
 
 extern comb_logic_t copy_m_ctl_sigs(m_ctl_sigs_t *, m_ctl_sigs_t *);
 extern comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *, w_ctl_sigs_t *);
 
 /*
	* Execute stage logic.
	* STUDENT TO-DO:
	* Implement the execute stage.
	*
	* Use in as the input pipeline register,
	* and update the out pipeline register as output.
	*
	* You will need the following helper functions:
	* copy_m_ctl_signals, copy_w_ctl_signals, and alu.
	*/
 
 comb_logic_t execute_instr(x_instr_impl_t *in, m_instr_impl_t *out) {

    copy_m_ctl_sigs(&out->M_sigs, &in->M_sigs);
    copy_w_ctl_sigs(&out->W_sigs, &in->W_sigs);

    uint64_t alu_operand_b = in->X_sigs.valb_sel ? in->val_b : in->val_imm;

    alu(in->val_a, alu_operand_b, in->val_hw, in->ALU_op, in->X_sigs.set_flags, in->cond, &out->val_ex, &out->cond_holds, &guest.proc->NZCV);

    out->op = in->op;
    out->print_op = in->print_op;
    out->val_b = in->val_b;
    out->dst = in->dst;
	out->status = in->status;
 }