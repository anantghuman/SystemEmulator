/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Fetch.c - Fetch stage of instruction processing pipeline.
 **************************************************************************/

 #include "hw_elts.h"
 #include "instr.h"
 #include "instr_pipeline.h"
 #include "machine.h"
 #include <assert.h>
 #include <stdbool.h>
 #include <stdint.h>
 
 extern machine_t guest;
 extern mem_status_t dmem_status;
 
 /*
  * Select PC logic.
  * STUDENT TO-DO:
  * Write the next PC to *current_PC.
  */
 
 static comb_logic_t
 select_PC(uint64_t pred_PC,                  // The predicted PC
           opcode_t D_opcode, uint64_t val_a, // Possible correction from RET
           uint64_t D_seq_succ,               // this is only used in CBZ/CBNZ EC
           opcode_t M_opcode, bool M_cond_val, // b.cond correction
           uint64_t seq_succ, // Possible correction from B.cond
           uint64_t *current_PC) {
   /*
    * Students: Please leave this code
    * at the top of this function.
    * You may modify below it.
    */
   if (D_opcode == OP_RET && val_a == RET_FROM_MAIN_ADDR) {
     *current_PC = 0; 
     return;
   }

   if (D_opcode == OP_RET) {
     *current_PC = val_a;
     return;
   }
   if (M_opcode == OP_B_COND && !M_cond_val) {
     F_in->status = STAT_BUB;  
     *current_PC = seq_succ;
     return;
   }
   *current_PC = pred_PC;
 }
 
 /*
  * Predict PC logic. Conditional branches are predicted taken.
  * STUDENT TO-DO:
  * Write the predicted next PC to *predicted_PC
  * and the next sequential pc to *seq_succ.
  */
 
 static comb_logic_t predict_PC(uint64_t current_PC, uint32_t insnbits,
                                opcode_t op, uint64_t *predicted_PC,
                                uint64_t *seq_succ) {
   /*
    * Students: Please leave this code
    * at the top of this function.
    * You may modify below it.
    */
   if (!current_PC) {
     return; // We use this to generate a halt instruction.
   }
   
   *seq_succ = current_PC + 4;
   if (op == OP_B || op == OP_BL) {
     *predicted_PC = current_PC + (bitfield_s64(insnbits, 0, 26) << 2);
     return;
   }
   if (op == OP_B_COND) {
     *predicted_PC = current_PC + (bitfield_s64(insnbits, 5, 19) << 2);
     return;
   }
   if (op == OP_RET) {
     *predicted_PC = *seq_succ;
     return;
   }
   *predicted_PC = *seq_succ;
 }
 
 /*
  * Helper function to recognize the aliased instructions:
  * LSL, LSR, CMP, CMN, and TST. We do this only to simplify the
  * implementations of the shift operations (rather than having
  * to implement UBFM in full).
  * STUDENT TO-DO
  */
 
 static void fix_instr_aliases(uint32_t insnbits, opcode_t *op) {
   uint8_t dest_reg = bitfield_u32(insnbits, 0, 5);
   *op = itable[bitfield_u32(insnbits, 21, 11)];
   if (dest_reg == 31) {
     if (*op == OP_ADDS_RR) {
       *op = OP_CMN_RR;
     }
     if (*op == OP_SUBS_RR) {
       *op = OP_CMP_RR;
     }
     if (*op == OP_ANDS_RR) {
       *op = OP_TST_RR;
     }
   }
   if (*op == OP_UBFM) {
     uint8_t sf = bitfield_u32(insnbits, 31, 1);
     uint32_t size = 1 << (5 + sf);
     uint8_t immr = bitfield_u32(insnbits, 16, 6);
     uint8_t imms = bitfield_u32(insnbits, 10, 6);
     if (imms == size - 1) {
       *op = OP_LSR;
       return;
     }
     if (imms + 1 == immr) {
       *op = OP_LSL;
       return;
     }
   }
 }
 
 /*
  * Fetch stage logic.
  * STUDENT TO-DO:
  * Implement the fetch stage.
  *
  * Use in as the input pipeline register,
  * and update the out pipeline register as output.
  * Additionally, update PC for the next
  * cycle's predicted PC.
  *
  * You will also need the following helper functions:
  * select_pc, predict_pc, and imem.
  */
 
 comb_logic_t fetch_instr(f_instr_impl_t *in, d_instr_impl_t *out) {
   opcode_t D_opcode = X_out->op;
   uint64_t val_a = X_out->val_a;
   opcode_t M_opcode = M_out->op;
   bool M_cond_val = M_out->cond_holds;
   uint64_t M_seq_succ = M_out->seq_succ_PC;
   bool imem_err = 0;
   uint64_t current_PC;
   select_PC(in->pred_PC, D_opcode, val_a, D_out->multipurpose_val.seq_succ_PC, 
             M_opcode, M_cond_val, M_seq_succ, &current_PC);
 
   uint64_t seq_succ;
   if (D_out->status == STAT_INS && X_out->status == STAT_INS) {
     seq_succ = current_PC;
   } else {
     seq_succ = current_PC + 4;
   }
 
   /*
    * Students: This case is for generating HLT instructions
    * to stop the pipeline. Only write your code in the **else** case.
    */
   if (!current_PC || F_in->status == STAT_HLT) {
     out->insnbits = 0xD4400000U;
     out->op = OP_HLT;
     out->print_op = OP_HLT;
     imem_err = false;
   } else {
     uint32_t instr;
     opcode_t op;
     imem(current_PC, &instr, &imem_err);
     if (imem_err) {
       guest.proc->PC = seq_succ;
       in->status = STAT_INS;
       out->status = in->status;
       out->op = OP_ERROR;
       out->print_op = out->op;
       F_in->status = in->status;
       return;
     }
     out->insnbits = instr;
     op = itable[bitfield_u32(instr, 21, 11)];
     fix_instr_aliases(instr, &op);
     out->op = op;
     if (op == OP_ADRP) {
       out->multipurpose_val.adrp_val = current_PC & ~0xfffUL;
     } else {
       out->multipurpose_val.seq_succ_PC = seq_succ;
     }
     uint64_t predicted_PC;
     predict_PC(current_PC, instr, op, &predicted_PC, &seq_succ);
     guest.proc->PC = predicted_PC;
   }
   if (imem_err) {
     guest.proc->PC = seq_succ;
   }
   out->print_op = out->op;
   if (imem_err || out->op == OP_ERROR) {
     in->status = STAT_INS;
     out->op = OP_ERROR;
   } else if (out->op == OP_HLT) {
     in->status = STAT_HLT;
   } else {
     in->status = STAT_AOK;
   }
   out->status = in->status;
   F_in->status = in->status;
   return;
 }