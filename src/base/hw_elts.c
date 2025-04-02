/**************************************************************************
 * C S 429 system emulator
 * 
 * hw_elts.c - Module for emulating hardware elements.
 * 
 * Copyright (c) 2022, 2023, 2024, 2025. 
 * Authors: S. Chatterjee, Z. Leeper., P. Jamadagni 
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 

#include <assert.h>
#include "hw_elts.h"
#include "mem.h"
#include "machine.h"
#include "err_handler.h"

extern machine_t guest;

comb_logic_t 
imem(uint64_t imem_addr,
     uint32_t *imem_rval, bool *imem_err) {
    // imem_addr must be in "instruction memory" and a multiple of 4
    *imem_err = (!addr_in_imem(imem_addr) || (imem_addr & 0x3U));
    *imem_rval = (uint32_t) mem_read_I(imem_addr);
}

comb_logic_t
regfile(uint8_t src1, uint8_t src2, uint8_t dst, uint64_t val_w,
        bool w_enable,
        uint64_t *val_a, uint64_t *val_b) {
    // Student TODO
    switch (src1) {
        case 31:
            *val_a = guest.proc->SP;
            break;
        case 32:
            *val_a = 0;
            break;
        default:
            *val_a = guest.proc->GPR[src1];
            break;
    }
    switch (src2) {
        case 31:
            *val_b = guest.proc->SP;
            break;
        case 32:
            *val_b = 0;
            break;
        default:
            *val_b = guest.proc->GPR[src2];
            break;
    }
    if (w_enable) {
        if (dst == 31)
            guest.proc->SP = val_w;
        else
            guest.proc->GPR[dst] = val_w;
    }
}

static bool cond_holds(cond_t cond, uint8_t flags) {
    // Student TODO
    // NZCV
    switch (cond) {
        case C_EQ:
            return (flags >> 2) & 1; 
        case C_NE:
            return !((flags >> 2) & 1);
        case C_CS:
            return (flags >> 1) & 1; 
        case C_CC:
            return !((flags >> 1) & 1); 
        case C_MI:
            return (flags >> 3) & 1;
        case C_PL:
            return !((flags >> 3) & 1);
        case C_VS:
            return (flags >> 0) & 1;
        case C_VC:
            return !((flags >> 0) & 1);
        case C_HI:
            return ((flags >> 1) & 1) && !((flags >> 2) & 1);
        case C_LS:
            return !(((flags >> 1) & 1) && !((flags >> 2) & 1));
        case C_GE:
            return ((flags >> 3) & 1) == ((flags >> 0) & 1);
        case C_LT:
            return ((flags >> 3) & 1) != ((flags >> 0) & 1);
        case C_GT:
            return (!((flags >> 2) & 1)) && (((flags >> 3) & 1) == ((flags >> 0) & 1));
        case C_LE:
            return ((flags >> 2) & 1) || (((flags >> 3) & 1) != ((flags >> 0) & 1));
        case C_AL:
            return true;
        case C_NV:
            return true;
        default:
            return false;        
    }
}

comb_logic_t 
alu(uint64_t alu_vala, uint64_t alu_valb, uint8_t alu_valhw, alu_op_t ALUop, bool set_flags, cond_t cond, 
    uint64_t *val_e, bool *cond_val, uint8_t *nzcv) {
    uint64_t res = 0xFEEDFACEDEADBEEF;  // To make it easier to detect errors.
    // Student TODO
    bool v = false;
    bool c = false;
    switch (ALUop) {
        case PLUS_OP:
            res = alu_vala + alu_valb;
            c = res < alu_vala;
            v = ((alu_vala >> 63) == (alu_valb >> 63)) && ((res >> 63) != (alu_vala >> 63));
            break;
        case MINUS_OP:
            res = alu_vala - alu_valb;
            c = (alu_vala >= alu_valb);
            v = ((alu_vala >> 63) != (alu_valb >> 63)) && ((res >> 63) == (alu_vala >> 63));
            break;
        case INV_OP:
            res = alu_vala | ~alu_valb;
            break;
        case OR_OP:
            res = alu_vala | alu_valb;
            break;
        case EOR_OP:
            res = alu_vala ^ alu_valb;
            break;
        case AND_OP:
            res = alu_vala & alu_valb;
            break;
        case MOV_OP:
            res = alu_vala | (alu_valb << alu_valhw);
            break;
        case LSL_OP:
            res = alu_vala << (alu_valb & 0x3FUL);
            break;
        case LSR_OP:
            res = alu_vala >> (alu_valb & 0x3FUL);
            break;
        case ASR_OP:
            res = (int64_t)alu_vala >> (alu_valb & 0x3FUL);
            break;
        case PASS_A_OP:
            res = alu_vala;
            break;
    }
    if (set_flags) {
        // NZCV
        *nzcv = 0;
        if (res == 0) {
            *nzcv |= 1 << 2; // Z
        }
        if ((int64_t)res < 0) {
            *nzcv |= 1 << 3; // N
        }
        if ((alu_vala > 0 && alu_valb > 0 && res < alu_vala) || (alu_vala < 0 && alu_valb < 0 && res > alu_vala)) {
            *nzcv |= c << 1; // C
        }
        if (((alu_vala ^ alu_valb) & (1ULL << 63)) == 0 && ((alu_vala ^ res) & (1ULL << 63)) != 0)
        {
            *nzcv |= (v << 0); // V
        }
    }
    *cond_val = cond_holds(cond, *nzcv);
    *val_e = res;
    return;
}

comb_logic_t 
dmem(uint64_t dmem_addr, uint64_t dmem_wval, bool dmem_read, bool dmem_write, 
     uint64_t *dmem_rval, bool *dmem_err) {
    if(!dmem_read && !dmem_write) {
        return;
    }
    // dmem_addr must be in "data memory" and a multiple of 8
    *dmem_err = (!addr_in_dmem(dmem_addr) || (dmem_addr & 0x7U));
    if (is_special_addr(dmem_addr)) *dmem_err = false;
    if (dmem_read) *dmem_rval = (uint64_t) mem_read_L(dmem_addr);
    if (dmem_write) mem_write_L(dmem_addr, dmem_wval);
}
