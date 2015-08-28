/******************************************************************************
** Copyright (c) 2015, Intel Corporation                                     **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Alexander Heinecke (Intel Corp.)
******************************************************************************/

#ifndef GENERATOR_DENSE_INSTRUCTIONS_H
#define GENERATOR_DENSE_INSTRUCTIONS_H

#include "generator_common.h"

/**
 * Opens the inline assembly section / jit stream
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param i_gp_reg_mapping gp register mapping for initialization
 * @param i_arch architecture code was generated for (needed to build clobber)
 * @param i_prefetch prefetch mode which may result in addtional gp reg inits
 */
void libxsmm_generator_dense_x86_open_instruction_stream( libxsmm_generated_code*       io_generated_code,
                                                         const libxsmm_gp_reg_mapping* i_gp_reg_mapping,
                                                         const char*                   i_arch,
                                                         const char*                   i_prefetch );

/**
 * Closes the inline assembly section / jit stream
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param i_gp_reg_mapping gp register mapping for clobbering
 * @param i_arch architecture code was generated for (needed to build clobber)
 * @param i_prefetch prefetch mode which may result in addtional gp reg clobbers
 */
void libxsmm_generator_dense_x86_close_instruction_stream( libxsmm_generated_code*       io_generated_code,
                                                           const libxsmm_gp_reg_mapping* i_gp_reg_mapping,
                                                           const char*                   i_arch, 
                                                           const char*                   i_prefetch );

/**
 * Generates vmovapd/vmovupd/vmovaps/vmovups/vmovsd/vmovss/vbroadcastsd/vbroastcastss/vmovddup instructions with displacements, explicit SIB addressing is not
 * supported by this function
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param i_instruction_set requested instruction set to encode
 * @param i_vmove_instr actual vmov variant
 * @param i_gp_reg_number the register number (rax=0,rcx=1,rdx=2,rbx=3,rsp=4,rbp=5,rsi=6,rdi=7,r8=8,r9=9,r10=10,r11=11,r12=12,r13=13,r14=14,r15=15) of the base address register
 * @param i_displacement the offset to the base address 
 * @param i_vector_name the vector register name prefix (x, y or z)
 * @param i_vec_reg_number_0 the vector register number (xmm/ymm: 0-15, zmm: 0-31)
 * @param i_mask_reg_number the mask register to be used
 * @param i_is_store 0: load semantik, other: store semantik  
 */
void libxsmm_instruction_vec_move( libxsmm_generated_code* io_generated_code, 
                                   const unsigned int      i_instruction_set,
                                   const unsigned int      i_vmove_instr, 
                                   const unsigned int      i_gp_reg_base,
                                   const unsigned int      i_gp_reg_idx,
                                   const unsigned int      i_scale,
                                   const int               i_displacement,
                                   const char              i_vector_name,
                                   const unsigned int      i_vec_reg_number_0,
                                   const unsigned int      i_use_masking,
                                   const unsigned int      i_is_store );

/**
 * Generates (v)XYZpd/(v)XYZps/(v)XYZsd/(v)XYZss instructions with 2 or 3 vector registers, memory operands are not supported as first operand
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param i_instruction_set requested instruction set to encode
 * @param i_vec_instr actual operation variant
 * @param i_vector_name the vector register name prefix (x,y or z)
 * @param i_vec_reg_number_0 the first vector register number (xmm/ymm: 0-15, zmm: 0-31)
 * @param i_vec_reg_number_1 the second vector register number (xmm/ymm: 0-15, zmm: 0-31)
 * @param i_vec_reg_number_2 the third vector register number (xmm/ymm: 0-15, zmm: 0-31), if this operand equals LIBXSMM_X86_VEC_REG_UNDEF -> SSE3 code generation
 */
void libxsmm_instruction_vec_compute_reg( libxsmm_generated_code* io_generated_code,
                                          const unsigned int      i_instruction_set, 
                                          const unsigned int      i_vec_instr,
                                          const char              i_vector_name,                              
                                          const unsigned int      i_vec_reg_number_0,
                                          const unsigned int      i_vec_reg_number_1,
                                          const unsigned int      i_vec_reg_number_2 );

/**
 * Generates (v)XYZpd/(v)XYZps/(v)XYZsd/(v)XYZss instructions with 2 vector registers, memory operand as first operand beinf broadcasted
 * Only supported on AVX512 and IMCI
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param i_instruction_set requested instruction set to encode
 * @param i_vec_instr actual operation variant
 * @param i_gp_reg_base base address register for memory broadcast
 * @param i_gp_reg_idx index register for memory broadcast, can be LIBXSMM_X86_GP_REG_UNDEF -> then regular displacement version is generated
 * @param i_scale scale of index register, ignored if i_gp_reg_idx is LIBXSMM_X86_GP_REG_UNDEF
 * @param i_displacement displacement to SIB address
 * @param i_vector_name the vector register name prefix (z)
 * @param i_vec_reg_number_0 the first vector register number (zmm: 0-31)
 * @param i_vec_reg_number_1 the second vector register number (zmm: 0-31)
 */
void libxsmm_instruction_vec_compute_membcast( libxsmm_generated_code* io_generated_code, 
                                               const unsigned int      i_instruction_set,
                                               const unsigned int      i_vec_instr,
                                               const unsigned int      i_gp_reg_base,
                                               const unsigned int      i_gp_reg_idx,
                                               const unsigned int      i_scale,
                                               const int               i_displacement,
                                               const char              i_vector_name,                                
                                               const unsigned int      i_vec_reg_number_0,
                                               const unsigned int      i_vec_reg_number_1 );

/**
 * Generates shuffle instructions with 2 or 3 vector registers, memory operands are not supported as first operand
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param i_instruction_set requested instruction set to encode
 * @param i_vec_instr actual operation variant
 * @param i_vector_name the vector register name prefix (x,y or z)
 * @param i_vec_reg_number_0 the first vector register number (xmm/ymm: 0-15, zmm: 0-31)
 * @param i_vec_reg_number_1 the second vector register number (xmm/ymm: 0-15, zmm: 0-31)
 * @param i_vec_reg_number_2 the third vector register number (xmm/ymm: 0-15, zmm: 0-31), if this operand equals LIBXSMM_X86_VEC_REG_UNDEF -> SSE3 code generation
 */
void libxsmm_instruction_vec_shuffle_reg( libxsmm_generated_code* io_generated_code, 
                                          const unsigned int      i_instruction_set,
                                          const unsigned int      i_vec_instr,
                                          const char              i_vector_name,                              
                                          const unsigned int      i_vec_reg_number_0,
                                          const unsigned int      i_vec_reg_number_1,
                                          const unsigned int      i_vec_reg_number_2,
                                          const unsigned int      i_shuffle_operand );

/* @TODO check if we can merge this alu_imm */
/**
 * Generates prefetch instructions with displacements, SIB addressing is not
 * supported by this function
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param i_prefetch_instr actual prefetch variant
 * @param i_gp_reg_number the register number (rax=0,rcx=1,rdx=2,rbx=3,rsp=4,rbp=5,rsi=6,rdi=7,r8=8,r9=9,r10=10,r11=11,r12=12,r13=13,r14=14,r15=15) of the base address register
 * @param i_displacement the offset to the base address 
 */
void libxsmm_instruction_prefetch( libxsmm_generated_code* io_generated_code,
                                   const unsigned int      i_prefetch_instr, 
                                   const unsigned int      i_gp_reg_base,
                                   const unsigned int      i_gp_reg_idx,
                                   const unsigned int      i_scale,
                                   const int               i_displacement );

/**
 * Generates regular all instructions with immediates
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param i_alu_instr actual alu gpr instruction
 * @param i_gp_reg_number the register number (rax=0,rcx=1,rdx=2,rbx=3,rsp=4,rbp=5,rsi=6,rdi=7,r8=8,r9=9,r10=10,r11=11,r12=12,r13=13,r14=14,r15=15) of the base address register
 * @param i_immediate the immediate operand 
 */
void libxsmm_instruction_alu_imm( libxsmm_generated_code* io_generated_code,
                                  const unsigned int      i_alu_instr,
                                  const unsigned int      i_gp_reg_number,
                                  const unsigned int      i_immediate );

/**
 * Generates regular all instructions with immediates
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param i_alu_instr actual alu gpr instruction
 * @param i_gp_reg_number_src the source register number (rax=0,rcx=1,rdx=2,rbx=3,rsp=4,rbp=5,rsi=6,rdi=7,r8=8,r9=9,r10=10,r11=11,r12=12,r13=13,r14=14,r15=15) of the base address register
 * @param i_gp_reg_number_dest the destination register number (rax=0,rcx=1,rdx=2,rbx=3,rsp=4,rbp=5,rsi=6,rdi=7,r8=8,r9=9,r10=10,r11=11,r12=12,r13=13,r14=14,r15=15) of the base address register
 */
void libxsmm_instruction_alu_reg( libxsmm_generated_code* io_generated_code,
                                  const unsigned int      i_alu_instr,
                                  const unsigned int      i_gp_reg_number_src,
                                  const unsigned int      i_gp_reg_number_dest);

/**
 * Allows for mask move instructions in AVX512
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param i_mask_instr actual mask move instruction
 * @param i_gp_reg_number the register number (rax=0,rcx=1,rdx=2,rbx=3,rsp=4,rbp=5,rsi=6,rdi=7,r8=8,r9=9,r10=10,r11=11,r12=12,r13=13,r14=14,r15=15) of the base address register
 * @param i_mask_reg_number the register number (k1=1...k7=7)
 */
void libxsmm_instruction_mask_move( libxsmm_generated_code* io_generated_code,
                                    const unsigned int      i_mask_instr,
                                    const unsigned int      i_gp_reg_number,
                                    const unsigned int      i_mask_reg_number );

/**
 * Generates regular all instructions with immediates
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param io_loop_label_tracker data structure to handle loop labels, nested loops are supported, but not overlapping loops
*/
void libxsmm_instruction_register_jump_label( libxsmm_generated_code*     io_generated_code,
                                              libxsmm_loop_label_tracker* io_loop_label_tracker );

/**
 * Generates regular all instructions with immediates
 *
 * @param io_generated_code pointer to the pointer of the generated code structure
 * @param i_jmp_instr the particular jump instruction used
 * @param io_loop_label_tracker data structure to handle loop labels will jump to latest registered label
*/
void libxsmm_instruction_jump_back_to_label( libxsmm_generated_code*     io_generated_code,
                                             const unsigned int          i_jmp_instr,
                                             libxsmm_loop_label_tracker* io_loop_label_tracker );

#endif /* GENERATOR_DENSE_INSTRUCTIONS_H */

