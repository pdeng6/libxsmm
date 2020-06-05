/******************************************************************************
* Copyright (c) Intel Corporation - All rights reserved.                      *
* This file is part of the LIBXSMM library.                                   *
*                                                                             *
* For information on the license, see the LICENSE file.                       *
* Further information: https://github.com/hfp/libxsmm/                        *
* SPDX-License-Identifier: BSD-3-Clause                                       *
******************************************************************************/
/* Evangelos Georganas, Alexander Heinecke (Intel Corp.)
******************************************************************************/
#include "generator_mateltwise_avx_avx512.h"
#include "generator_x86_instructions.h"
#include "generator_common.h"
#include "libxsmm_main.h"

LIBXSMM_API_INTERN
void libxsmm_generator_mateltwise_header_m_loop( libxsmm_generated_code*                io_generated_code,
                                              libxsmm_loop_label_tracker*               io_loop_label_tracker,
                                              const libxsmm_mateltwise_kernel_config*   i_kernel_config,
                                              const unsigned int                        i_gp_reg_m_loop ) {
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_kernel_config->alu_mov_instruction, i_gp_reg_m_loop, 0);
  libxsmm_x86_instruction_register_jump_back_label( io_generated_code, io_loop_label_tracker );
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_kernel_config->alu_add_instruction, i_gp_reg_m_loop, 1);
}

LIBXSMM_API_INTERN
void libxsmm_generator_mateltwise_footer_m_loop( libxsmm_generated_code*                    io_generated_code,
                                              libxsmm_loop_label_tracker*                   io_loop_label_tracker,
                                              const libxsmm_mateltwise_kernel_config*       i_kernel_config,
                                              const unsigned int                            i_gp_reg_m_loop,
                                              const unsigned int                            i_m ) {
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_kernel_config->alu_cmp_instruction, i_gp_reg_m_loop, i_m );
  libxsmm_x86_instruction_jump_back_to_label( io_generated_code, i_kernel_config->alu_jmp_instruction, io_loop_label_tracker );
}

LIBXSMM_API_INTERN
void libxsmm_generator_mateltwise_header_n_loop( libxsmm_generated_code*                io_generated_code,
                                              libxsmm_loop_label_tracker*               io_loop_label_tracker,
                                              const libxsmm_mateltwise_kernel_config*   i_kernel_config,
                                              const unsigned int                        i_gp_reg_n_loop ) {
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_kernel_config->alu_mov_instruction, i_gp_reg_n_loop, 0);
  libxsmm_x86_instruction_register_jump_back_label( io_generated_code, io_loop_label_tracker );
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_kernel_config->alu_add_instruction, i_gp_reg_n_loop, 1);
}

LIBXSMM_API_INTERN
void libxsmm_generator_mateltwise_footer_n_loop( libxsmm_generated_code*                    io_generated_code,
                                              libxsmm_loop_label_tracker*                   io_loop_label_tracker,
                                              const libxsmm_mateltwise_kernel_config*       i_kernel_config,
                                              const unsigned int                            i_gp_reg_n_loop,
                                              const unsigned int                            i_n ) {
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_kernel_config->alu_cmp_instruction, i_gp_reg_n_loop, i_n );
  libxsmm_x86_instruction_jump_back_to_label( io_generated_code, i_kernel_config->alu_jmp_instruction, io_loop_label_tracker );
}

LIBXSMM_API_INTERN
void libxsmm_generator_mateltwise_initialize_avx512_mask( libxsmm_generated_code*            io_generated_code,
    const unsigned int                       i_gp_reg_tmp,
    const unsigned int                       i_mask_reg,
    const unsigned int                       i_mask_count,
    const unsigned int                       i_precision) {

  unsigned long long l_mask = 0;
  if (i_precision == LIBXSMM_GEMM_PRECISION_F32) {
    l_mask = 0xffff;
  } else if (i_precision == LIBXSMM_GEMM_PRECISION_BF16) {
    l_mask = 0xffffffff;
  } else if (i_precision == LIBXSMM_GEMM_PRECISION_I8) {
    l_mask = 0xffffffffffffffff;
  }
  /* shift right by "inverse" remainder */
  l_mask = l_mask >> i_mask_count;

  /* move mask to GP register */
  libxsmm_x86_instruction_alu_imm( io_generated_code,
      LIBXSMM_X86_INSTR_MOVQ,
      i_gp_reg_tmp,
      l_mask );

  if ( io_generated_code->arch >= LIBXSMM_X86_AVX512_CORE  ) {
    if ( LIBXSMM_GEMM_PRECISION_F32 == i_precision ) {
      libxsmm_x86_instruction_mask_move( io_generated_code,
          LIBXSMM_X86_INSTR_KMOVW,
          i_gp_reg_tmp,
          i_mask_reg, 0 );
    } else if ( LIBXSMM_GEMM_PRECISION_BF16 == i_precision ) {
      libxsmm_x86_instruction_mask_move( io_generated_code,
          LIBXSMM_X86_INSTR_KMOVD,
          i_gp_reg_tmp,
          i_mask_reg, 0 );
    } else if ( LIBXSMM_GEMM_PRECISION_I8 == i_precision ) {
      libxsmm_x86_instruction_mask_move( io_generated_code,
          LIBXSMM_X86_INSTR_KMOVQ,
          i_gp_reg_tmp,
          i_mask_reg, 0 );
    } else {
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_UNSUP_DATATYPE );
      return;
    }
  } else {
    /* shouldn't happen */
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_ARCH );
    return;
  }
}

LIBXSMM_API_INTERN
void libxsmm_generator_mateltwise_init_micro_kernel_config_fullvector( libxsmm_generated_code*         io_generated_code,
    libxsmm_mateltwise_kernel_config*    io_micro_kernel_config,
    const unsigned int              i_arch,
    const libxsmm_meltw_descriptor* i_mateltwise_desc) {
  memset(io_micro_kernel_config, 0, sizeof(*io_micro_kernel_config)); /* avoid warning "maybe used uninitialized" */
  if ( i_arch >= LIBXSMM_X86_AVX512_CORE ) {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX512_CORE;
    io_micro_kernel_config->vector_reg_count = 16;
    /* Configure input specific microkernel options */
    if ( LIBXSMM_GEMM_PRECISION_F32 == LIBXSMM_GETENUM_INP( i_mateltwise_desc->datatype ) ) {
      io_micro_kernel_config->datatype_size_in = 4;
      io_micro_kernel_config->vector_length_in = 16;
      io_micro_kernel_config->vmove_instruction_in = LIBXSMM_X86_INSTR_VMOVUPS;
    } else if ( LIBXSMM_GEMM_PRECISION_BF16 == LIBXSMM_GETENUM_INP( i_mateltwise_desc->datatype ) ) {
      io_micro_kernel_config->datatype_size_in = 2;
      io_micro_kernel_config->vector_length_in = 32;
      io_micro_kernel_config->vmove_instruction_in = LIBXSMM_X86_INSTR_VMOVDQU16;
    } else if ( LIBXSMM_GEMM_PRECISION_I8 == LIBXSMM_GETENUM_INP( i_mateltwise_desc->datatype ) ) {
      io_micro_kernel_config->datatype_size_in = 1;
      io_micro_kernel_config->vector_length_in = 64;
      io_micro_kernel_config->vmove_instruction_in = LIBXSMM_X86_INSTR_VMOVDQU8;
    } else {
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_UNSUP_DATATYPE );
      return;
    }
    /* Configure output specific microkernel options */
    if ( LIBXSMM_GEMM_PRECISION_F32 == LIBXSMM_GETENUM_OUT( i_mateltwise_desc->datatype ) ) {
      io_micro_kernel_config->datatype_size_out = 4;
      io_micro_kernel_config->vector_length_out = 16;
      io_micro_kernel_config->vmove_instruction_out = LIBXSMM_X86_INSTR_VMOVUPS;
    } else if ( LIBXSMM_GEMM_PRECISION_BF16 == LIBXSMM_GETENUM_OUT( i_mateltwise_desc->datatype ) ) {
      io_micro_kernel_config->datatype_size_out = 2;
      io_micro_kernel_config->vector_length_out = 32;
      io_micro_kernel_config->vmove_instruction_out = LIBXSMM_X86_INSTR_VMOVDQU16;
    } else if ( LIBXSMM_GEMM_PRECISION_I8 == LIBXSMM_GETENUM_OUT( i_mateltwise_desc->datatype ) ) {
      io_micro_kernel_config->datatype_size_out = 1;
      io_micro_kernel_config->vector_length_out = 64;
      io_micro_kernel_config->vmove_instruction_out = LIBXSMM_X86_INSTR_VMOVDQU8;
    } else {
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_UNSUP_DATATYPE );
      return;
    }
    io_micro_kernel_config->alu_add_instruction = LIBXSMM_X86_INSTR_ADDQ;
    io_micro_kernel_config->alu_cmp_instruction = LIBXSMM_X86_INSTR_CMPQ;
    io_micro_kernel_config->alu_jmp_instruction = LIBXSMM_X86_INSTR_JL;
    io_micro_kernel_config->alu_mov_instruction = LIBXSMM_X86_INSTR_MOVQ;
    io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VPXORD;
    io_micro_kernel_config->vector_name = 'z';
  } else {
    /* That should not happen */
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_ARCH );
  }
}

LIBXSMM_API_INTERN
void libxsmm_generator_cvtfp32bf16_avx512_replacement_sequence( libxsmm_generated_code*                        io_generated_code,
    const libxsmm_mateltwise_kernel_config*        i_micro_kernel_config,
    const unsigned int                             i_vec_reg ) {

  /* and with naninf */
  libxsmm_x86_instruction_vec_compute_mem( io_generated_code,
      i_micro_kernel_config->instruction_set,
      LIBXSMM_X86_INSTR_VPANDD,
      1,
      LIBXSMM_X86_GP_REG_RSP,
      LIBXSMM_X86_GP_REG_UNDEF,
      0,
      24,
      i_micro_kernel_config->vector_name,
      i_vec_reg,
      0 );

  /* and with fixup */
  libxsmm_x86_instruction_vec_compute_mem( io_generated_code,
      i_micro_kernel_config->instruction_set,
      LIBXSMM_X86_INSTR_VPANDD,
      1,
      LIBXSMM_X86_GP_REG_RSP,
      LIBXSMM_X86_GP_REG_UNDEF,
      0,
      16,
      i_micro_kernel_config->vector_name,
      i_vec_reg,
      1 );

  /* compute naninf mask k7 */
  libxsmm_x86_instruction_vec_compute_mem_mask( io_generated_code,
      i_micro_kernel_config->instruction_set,
      LIBXSMM_X86_INSTR_VPCMPD,
      1,
      LIBXSMM_X86_GP_REG_RSP,
      LIBXSMM_X86_GP_REG_UNDEF,
      0,
      24,
      i_micro_kernel_config->vector_name,
      0,
      LIBXSMM_X86_VEC_REG_UNDEF,
      4,
      7, 0 );

  /* compute fixup mask k6 */
  libxsmm_x86_instruction_vec_compute_mem_mask( io_generated_code,
      i_micro_kernel_config->instruction_set,
      LIBXSMM_X86_INSTR_VPCMPD,
      1,
      LIBXSMM_X86_GP_REG_RSP,
      LIBXSMM_X86_GP_REG_UNDEF,
      0,
      16,
      i_micro_kernel_config->vector_name,
      1,
      LIBXSMM_X86_VEC_REG_UNDEF,
      0,
      6, 0 );

  /* load rneadd */
  libxsmm_x86_instruction_vec_move( io_generated_code,
      i_micro_kernel_config->instruction_set,
      LIBXSMM_X86_INSTR_VBROADCASTSS,
      LIBXSMM_X86_GP_REG_RSP,
      LIBXSMM_X86_GP_REG_UNDEF, 0,
      8,
      i_micro_kernel_config->vector_name,
      0, 0, 1, 0 );

  /* load fixup */
  libxsmm_x86_instruction_vec_move( io_generated_code,
      i_micro_kernel_config->instruction_set,
      LIBXSMM_X86_INSTR_VBROADCASTSS,
      LIBXSMM_X86_GP_REG_RSP,
      LIBXSMM_X86_GP_REG_UNDEF, 0,
      0,
      i_micro_kernel_config->vector_name,
      1, 0, 1, 0 );

  /* compute fixup */
  libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
      i_micro_kernel_config->instruction_set,
      LIBXSMM_X86_INSTR_VPADDD,
      i_micro_kernel_config->vector_name,
      1,
      0,
      0,
      LIBXSMM_X86_IMM_UNDEF,
      6,
      0 );

  /* compute fixup */
  libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
      i_micro_kernel_config->instruction_set,
      LIBXSMM_X86_INSTR_VPADDD,
      i_micro_kernel_config->vector_name,
      0,
      i_vec_reg,
      i_vec_reg,
      LIBXSMM_X86_IMM_UNDEF,
      7,
      0 );
}


LIBXSMM_API_INTERN
void libxsmm_generator_cvtfp32bf16_avx512_microkernel( libxsmm_generated_code*                        io_generated_code,
    libxsmm_loop_label_tracker*                    io_loop_label_tracker,
    libxsmm_mateltwise_gp_reg_mapping*             i_gp_reg_mapping,
    const libxsmm_mateltwise_kernel_config*        i_micro_kernel_config,
    const libxsmm_meltw_descriptor*                i_mateltwise_desc ) {

  unsigned int i = 0, im, m, n, m_trips, use_m_masking, mask_in_count, mask_out_count, reg_0, reg_1;

  /* Some rudimentary checking of M, N and LDs*/
  if ( (i_mateltwise_desc->m > i_mateltwise_desc->ldi) || (i_mateltwise_desc->m > i_mateltwise_desc->ldo) ) {
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_LDA );
    return;
  }

  /* Configure the register mapping for this eltwise kernel */
  i_gp_reg_mapping->gp_reg_in     = LIBXSMM_X86_GP_REG_R8;
  i_gp_reg_mapping->gp_reg_out    = LIBXSMM_X86_GP_REG_R9;
  i_gp_reg_mapping->gp_reg_m_loop = LIBXSMM_X86_GP_REG_R10;
  i_gp_reg_mapping->gp_reg_n_loop = LIBXSMM_X86_GP_REG_R11;

  /* load the input pointer and output pointer */
  libxsmm_x86_instruction_alu_mem( io_generated_code,
      i_micro_kernel_config->alu_mov_instruction,
      i_gp_reg_mapping->gp_reg_param_struct,
      LIBXSMM_X86_GP_REG_UNDEF, 0,
      0,
      i_gp_reg_mapping->gp_reg_in,
      0 );

  libxsmm_x86_instruction_alu_mem( io_generated_code,
      i_micro_kernel_config->alu_mov_instruction,
      i_gp_reg_mapping->gp_reg_param_struct,
      LIBXSMM_X86_GP_REG_UNDEF, 0,
      8,
      i_gp_reg_mapping->gp_reg_out,
      0 );

  /* We fully unroll in M dimension, calculate mask if there is remainder */
  m                 = i_mateltwise_desc->m;
  n                 = i_mateltwise_desc->n;
  use_m_masking     = ( m % 32 == 0 ) ? 0 : 1;
  m_trips           = (m + 31) / 32;

  /* Calculate input and output masks in case we see m_masking */
  if (use_m_masking == 1) {
    /* If the remaining elements are < 16, then we read a full vector and a partial one at the last m trip */
    /* If the remaining elements are >= 16, then we read a partial vector at the last m trip  */
    /* Calculate mask reg 1 for input-reading */
    mask_in_count = ( (m % 32) > 16) ? 32 - (m % 32) : 16 - (m % 32);
    libxsmm_generator_mateltwise_initialize_avx512_mask(io_generated_code, LIBXSMM_X86_GP_REG_R12, 1, mask_in_count, LIBXSMM_GEMM_PRECISION_F32);
    /* Calculate mask reg 2 for output-writing */
    mask_out_count = 32 - (m % 32);
    libxsmm_generator_mateltwise_initialize_avx512_mask(io_generated_code, LIBXSMM_X86_GP_REG_R12, 2, mask_out_count, LIBXSMM_GEMM_PRECISION_BF16);
  }

  /* In this case we have to use CPX replacement sequence for downconverts... */
  if (io_generated_code->arch < LIBXSMM_X86_AVX512_CPX) {
    /* init stack with helper variables for SW-based RNE rounding */
    /* push 0x7f800000 on the stack, naninf masking */
    libxsmm_x86_instruction_alu_imm( io_generated_code, LIBXSMM_X86_INSTR_MOVQ, LIBXSMM_X86_GP_REG_R12, 0x7f800000);
    libxsmm_x86_instruction_push_reg( io_generated_code, LIBXSMM_X86_GP_REG_R12 );

    /* push 0x00010000 on the stack, fixup masking */
    libxsmm_x86_instruction_alu_imm( io_generated_code, LIBXSMM_X86_INSTR_MOVQ, LIBXSMM_X86_GP_REG_R12, 0x00010000);
    libxsmm_x86_instruction_push_reg( io_generated_code, LIBXSMM_X86_GP_REG_R12 );

    /* push 0x00007fff on the stack, rneadd */
    libxsmm_x86_instruction_alu_imm( io_generated_code, LIBXSMM_X86_INSTR_MOVQ, LIBXSMM_X86_GP_REG_R12, 0x00007fff);
    libxsmm_x86_instruction_push_reg( io_generated_code, LIBXSMM_X86_GP_REG_R12);

    /* push 0x00000001 on the stack, fixup */
    libxsmm_x86_instruction_alu_imm( io_generated_code, LIBXSMM_X86_INSTR_MOVQ, LIBXSMM_X86_GP_REG_R12, 0x00000001);
    libxsmm_x86_instruction_push_reg( io_generated_code, LIBXSMM_X86_GP_REG_R12 );

    /* If we are using the 3 operant convert variant, then generate the proper permute table in zmm2 for the replacement code */
    if (m > 16) {
      short perm_array[32] = { 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31};
      short selector_array[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0 ,0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 };
      for (i = 0; i < 32; i++) {
        perm_array[i] = perm_array[i] | selector_array[i];
      }
      libxsmm_x86_instruction_full_vec_load_of_constants ( io_generated_code,
          (const unsigned char *) perm_array,
          "perm_arrray_",
          i_micro_kernel_config->vector_name,
          2);
    }
  }

  if (n > 1) {
    /* open n loop */
    libxsmm_generator_mateltwise_header_n_loop(  io_generated_code, io_loop_label_tracker, i_micro_kernel_config, i_gp_reg_mapping->gp_reg_n_loop );
  }

  for (im = 0; im < m_trips; im++) {
    /* Load in zmm_i and zmm_i+16 the two input fp32 zmms  */
    reg_0 = im % 16;
    reg_1 = (im % 16)+16;
    /* In this case we have to reserve zmm0 and zmm1 for the replacement sequence  */
    if (io_generated_code->arch < LIBXSMM_X86_AVX512_CPX) {
      reg_0 = (im % 13) + 3;
      reg_1 = ((im % 13) + 3)+16;
    }

    libxsmm_x86_instruction_vec_move( io_generated_code,
        i_micro_kernel_config->instruction_set,
        i_micro_kernel_config->vmove_instruction_in,
        i_gp_reg_mapping->gp_reg_in,
        LIBXSMM_X86_GP_REG_UNDEF, 0,
        im * 32 * i_micro_kernel_config->datatype_size_in,
        i_micro_kernel_config->vector_name,
        reg_0, ((im == (m_trips-1)) && (m % 32 < 16)) ? use_m_masking : 0, 1, 0 );

    /* If last iteration and remainder is less than 16, do not load anything  */
    if (!((use_m_masking == 1) && (im == m_trips-1) && (m % 32 <= 16))) {
      libxsmm_x86_instruction_vec_move( io_generated_code,
          i_micro_kernel_config->instruction_set,
          i_micro_kernel_config->vmove_instruction_in,
          i_gp_reg_mapping->gp_reg_in,
          LIBXSMM_X86_GP_REG_UNDEF, 0,
          (im * 32 + 16) * i_micro_kernel_config->datatype_size_in,
          i_micro_kernel_config->vector_name,
          reg_1, (im == (m_trips-1)) ? use_m_masking : 0, 1, 0 );
    }

    /* Downconvert to BF16  */
    if (io_generated_code->arch >=  LIBXSMM_X86_AVX512_CPX) {
      if (!((use_m_masking == 1) && (im == m_trips-1) && (m % 32 <= 16))) {
        libxsmm_x86_instruction_vec_compute_convert( io_generated_code,
            i_micro_kernel_config->instruction_set,
            LIBXSMM_X86_INSTR_VCVTNE2PS2BF16,
            i_micro_kernel_config->vector_name,
            reg_0, reg_1,
            reg_0,
            0);
      } else {
        libxsmm_x86_instruction_vec_compute_convert( io_generated_code,
            i_micro_kernel_config->instruction_set,
            LIBXSMM_X86_INSTR_VCVTNEPS2BF16,
            i_micro_kernel_config->vector_name,
            reg_0, LIBXSMM_X86_VEC_REG_UNDEF,
            reg_0,
            0);
      }
    } else {
      if (!((use_m_masking == 1) && (im == m_trips-1) && (m % 32 <= 16))) {
        /* RNE convert reg_0 and reg_1 */
        libxsmm_generator_cvtfp32bf16_avx512_replacement_sequence( io_generated_code, i_micro_kernel_config, reg_0 );
        libxsmm_generator_cvtfp32bf16_avx512_replacement_sequence( io_generated_code, i_micro_kernel_config, reg_1 );
        /* Properly interleave reg_0 and reg_1 into reg_0  */
        libxsmm_x86_instruction_vec_compute_reg(io_generated_code,
            i_micro_kernel_config->instruction_set,
            LIBXSMM_X86_INSTR_VPERMT2W,
            i_micro_kernel_config->vector_name,
            reg_1,
            2,
            reg_0);
      } else {
        /* RNE convert reg_0 */
        libxsmm_generator_cvtfp32bf16_avx512_replacement_sequence( io_generated_code, i_micro_kernel_config, reg_0 );
        /* shift FP32 by 16bit to right */
        libxsmm_x86_instruction_vec_shuffle_reg(io_generated_code,
            i_micro_kernel_config->instruction_set,
            LIBXSMM_X86_INSTR_VPSRAD,
            i_micro_kernel_config->vector_name,
            reg_0,
            reg_0,
            LIBXSMM_X86_VEC_REG_UNDEF,
            16);
        /* store 16 bit values into ymm portion of reg_0 */
        libxsmm_x86_instruction_vec_compute_convert( io_generated_code,
            i_micro_kernel_config->instruction_set,
            LIBXSMM_X86_INSTR_VPMOVDW,
            i_micro_kernel_config->vector_name,
            reg_0, LIBXSMM_X86_VEC_REG_UNDEF,
            reg_0,
            LIBXSMM_X86_VEC_REG_UNDEF);
      }
    }

    /* Store the result  */
    libxsmm_x86_instruction_vec_move( io_generated_code,
        i_micro_kernel_config->instruction_set,
        i_micro_kernel_config->vmove_instruction_out,
        i_gp_reg_mapping->gp_reg_out,
        LIBXSMM_X86_GP_REG_UNDEF, 0,
        im * 32 * i_micro_kernel_config->datatype_size_out,
        i_micro_kernel_config->vector_name,
        reg_0, (im == (m_trips-1)) ? use_m_masking * 2 : 0, 0, 1 );
  }

  if (n > 1) {
    /* Adjust input and output pointer */
    libxsmm_x86_instruction_alu_imm(  io_generated_code,
        i_micro_kernel_config->alu_add_instruction,
        i_gp_reg_mapping->gp_reg_in,
        i_mateltwise_desc->ldi *  i_micro_kernel_config->datatype_size_in);

    libxsmm_x86_instruction_alu_imm(  io_generated_code,
        i_micro_kernel_config->alu_add_instruction,
        i_gp_reg_mapping->gp_reg_out,
        i_mateltwise_desc->ldo *  i_micro_kernel_config->datatype_size_out);

    /* close n loop */
    libxsmm_generator_mateltwise_footer_n_loop(  io_generated_code, io_loop_label_tracker, i_micro_kernel_config, i_gp_reg_mapping->gp_reg_n_loop, n);
  }

  if (io_generated_code->arch < LIBXSMM_X86_AVX512_CPX) {
    libxsmm_x86_instruction_pop_reg( io_generated_code, LIBXSMM_X86_GP_REG_R12 );
    libxsmm_x86_instruction_pop_reg( io_generated_code, LIBXSMM_X86_GP_REG_R12 );
    libxsmm_x86_instruction_pop_reg( io_generated_code, LIBXSMM_X86_GP_REG_R12 );
    libxsmm_x86_instruction_pop_reg( io_generated_code, LIBXSMM_X86_GP_REG_R12 );
  }

}

LIBXSMM_API_INTERN
void libxsmm_generator_reduce_cols_avx512_microkernel( libxsmm_generated_code*                        io_generated_code,
    libxsmm_loop_label_tracker*                    io_loop_label_tracker,
    libxsmm_mateltwise_gp_reg_mapping*             i_gp_reg_mapping,
    const libxsmm_mateltwise_kernel_config*        i_micro_kernel_config,
    const libxsmm_meltw_descriptor*                i_mateltwise_desc ) {

  unsigned int in, m, n, m_full_trips, use_m_masking, mask_count, compute_squared_vals_reduce, compute_plain_vals_reduce, reg_n, reg_sum = 30, reg_sum_squared = 31;

  /* Some rudimentary checking of M, N and LDs*/
  if ( i_mateltwise_desc->m > i_mateltwise_desc->ldi ) {
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_LDA );
    return;
  }

  if ( (i_mateltwise_desc->flags & LIBXSMM_MELTW_REDUCE_ELTS) > 0 ) {
    compute_plain_vals_reduce= 1;
  } else {
    compute_plain_vals_reduce= 0;
  }

  if ( (i_mateltwise_desc->flags & LIBXSMM_MELTW_REDUCE_ELTS_SQUARED) > 0 ) {
    compute_squared_vals_reduce = 1;
  } else {
    compute_squared_vals_reduce = 0;
  }

  /* Configure the register mapping for this eltwise kernel */
  i_gp_reg_mapping->gp_reg_in                   = LIBXSMM_X86_GP_REG_R8;
  i_gp_reg_mapping->gp_reg_reduced_elts         = LIBXSMM_X86_GP_REG_R9;
  i_gp_reg_mapping->gp_reg_reduced_elts_squared = LIBXSMM_X86_GP_REG_R10;
  i_gp_reg_mapping->gp_reg_m_loop               = LIBXSMM_X86_GP_REG_R11;
  i_gp_reg_mapping->gp_reg_n_loop               = LIBXSMM_X86_GP_REG_R12;

  /* load the input pointer and output pointer */
  libxsmm_x86_instruction_alu_mem( io_generated_code,
      i_micro_kernel_config->alu_mov_instruction,
      i_gp_reg_mapping->gp_reg_param_struct,
      LIBXSMM_X86_GP_REG_UNDEF, 0,
      0,
      i_gp_reg_mapping->gp_reg_in,
      0 );

  if ( compute_plain_vals_reduce > 0 ) {
    libxsmm_x86_instruction_alu_mem( io_generated_code,
        i_micro_kernel_config->alu_mov_instruction,
       i_gp_reg_mapping->gp_reg_param_struct,
       LIBXSMM_X86_GP_REG_UNDEF, 0,
       8,
       i_gp_reg_mapping->gp_reg_reduced_elts,
       0 );
  }

  if ( compute_squared_vals_reduce > 0 ) {
    libxsmm_x86_instruction_alu_mem( io_generated_code,
        i_micro_kernel_config->alu_mov_instruction,
       i_gp_reg_mapping->gp_reg_param_struct,
       LIBXSMM_X86_GP_REG_UNDEF, 0,
       16,
       i_gp_reg_mapping->gp_reg_reduced_elts_squared,
       0 );
  }

  /* We fully unroll in N dimension, calculate m-mask if there is remainder */
  m                 = i_mateltwise_desc->m;
  n                 = i_mateltwise_desc->n;
  use_m_masking     = ( m % 16 == 0 ) ? 0 : 1;
  m_full_trips      = m / 16;

  /* Calculate input mask in case we see m_masking */
  if (use_m_masking == 1) {
    /* Calculate mask reg 1 for input-reading */
    mask_count =  16 - (m % 16);
    libxsmm_generator_mateltwise_initialize_avx512_mask(io_generated_code, LIBXSMM_X86_GP_REG_R13, 1, mask_count, LIBXSMM_GEMM_PRECISION_F32);
  }

  if ( m_full_trips >= 1 ) {
    if (m_full_trips > 1) {
      /* open m loop */
      libxsmm_generator_mateltwise_header_m_loop(  io_generated_code, io_loop_label_tracker, i_micro_kernel_config, i_gp_reg_mapping->gp_reg_m_loop );
    }

    /* Initialize accumulators to zero */
    if ( compute_plain_vals_reduce > 0 ) {
      libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VPXORD,
                                               i_micro_kernel_config->vector_name,
                                               reg_sum, reg_sum, reg_sum );
    }

    if ( compute_squared_vals_reduce > 0 ) {
      libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VPXORD,
                                               i_micro_kernel_config->vector_name,
                                               reg_sum_squared, reg_sum_squared, reg_sum_squared );
    }

    for (in = 0; in < n; in++) {
      reg_n = in % 30;

      libxsmm_x86_instruction_vec_move( io_generated_code,
          i_micro_kernel_config->instruction_set,
          i_micro_kernel_config->vmove_instruction_in,
          i_gp_reg_mapping->gp_reg_in,
          LIBXSMM_X86_GP_REG_UNDEF, 0,
          in * i_mateltwise_desc->ldi * i_micro_kernel_config->datatype_size_in,
          i_micro_kernel_config->vector_name,
          reg_n, 0, 1, 0 );

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             reg_n, reg_sum, reg_sum );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VFMADD231PS,
                                             i_micro_kernel_config->vector_name,
                                             reg_n, reg_n, reg_sum_squared );
      }
    }

    /* Store computed results */
    if ( compute_plain_vals_reduce > 0 ) {
      libxsmm_x86_instruction_vec_move( io_generated_code,
                                        i_micro_kernel_config->instruction_set,
                                        i_micro_kernel_config->vmove_instruction_out,
                                        i_gp_reg_mapping->gp_reg_reduced_elts,
                                        LIBXSMM_X86_GP_REG_UNDEF, 0,
                                        0,
                                        i_micro_kernel_config->vector_name,
                                        reg_sum, 0, 0, 1 );

    }

    if ( compute_squared_vals_reduce > 0 ) {
      libxsmm_x86_instruction_vec_move( io_generated_code,
                                        i_micro_kernel_config->instruction_set,
                                        i_micro_kernel_config->vmove_instruction_out,
                                        i_gp_reg_mapping->gp_reg_reduced_elts_squared,
                                        LIBXSMM_X86_GP_REG_UNDEF, 0,
                                        0,
                                        i_micro_kernel_config->vector_name,
                                        reg_sum_squared, 0, 0, 1 );
    }

    if ((m_full_trips > 1) || (use_m_masking == 1)) {
      /* Adjust input and output pointer */
      libxsmm_x86_instruction_alu_imm(  io_generated_code,
          i_micro_kernel_config->alu_add_instruction,
          i_gp_reg_mapping->gp_reg_in,
          16 * i_micro_kernel_config->datatype_size_in);

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_alu_imm(  io_generated_code,
            i_micro_kernel_config->alu_add_instruction,
            i_gp_reg_mapping->gp_reg_reduced_elts,
            16 * i_micro_kernel_config->datatype_size_out);
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_alu_imm(  io_generated_code,
            i_micro_kernel_config->alu_add_instruction,
            i_gp_reg_mapping->gp_reg_reduced_elts_squared,
            16 * i_micro_kernel_config->datatype_size_out);
      }
    }

    if (m_full_trips > 1) {
      /* close m loop */
      libxsmm_generator_mateltwise_footer_m_loop(  io_generated_code, io_loop_label_tracker, i_micro_kernel_config, i_gp_reg_mapping->gp_reg_m_loop, m_full_trips);
    }
  }

  if (use_m_masking == 1) {
    /* Initialize accumulators to zero */
    if ( compute_plain_vals_reduce > 0 ) {
      libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VPXORD,
                                               i_micro_kernel_config->vector_name,
                                               reg_sum, reg_sum, reg_sum );
    }

    if ( compute_squared_vals_reduce > 0 ) {
      libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VPXORD,
                                               i_micro_kernel_config->vector_name,
                                               reg_sum_squared, reg_sum_squared, reg_sum_squared );
    }

    for (in = 0; in < n; in++) {
      reg_n = in % 30;

      libxsmm_x86_instruction_vec_move( io_generated_code,
          i_micro_kernel_config->instruction_set,
          i_micro_kernel_config->vmove_instruction_in,
          i_gp_reg_mapping->gp_reg_in,
          LIBXSMM_X86_GP_REG_UNDEF, 0,
          in * i_mateltwise_desc->ldi * i_micro_kernel_config->datatype_size_in,
          i_micro_kernel_config->vector_name,
          reg_n, use_m_masking, 1, 0 );

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             reg_n, reg_sum, reg_sum );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VFMADD231PS,
                                             i_micro_kernel_config->vector_name,
                                             reg_n, reg_n, reg_sum_squared );
      }
    }

    /* Store computed results */
    if ( compute_plain_vals_reduce > 0 ) {
      libxsmm_x86_instruction_vec_move( io_generated_code,
                                        i_micro_kernel_config->instruction_set,
                                        i_micro_kernel_config->vmove_instruction_out,
                                        i_gp_reg_mapping->gp_reg_reduced_elts,
                                        LIBXSMM_X86_GP_REG_UNDEF, 0,
                                        0,
                                        i_micro_kernel_config->vector_name,
                                        reg_sum, use_m_masking, 0, 1 );

    }

    if ( compute_squared_vals_reduce > 0 ) {
      libxsmm_x86_instruction_vec_move( io_generated_code,
                                        i_micro_kernel_config->instruction_set,
                                        i_micro_kernel_config->vmove_instruction_out,
                                        i_gp_reg_mapping->gp_reg_reduced_elts_squared,
                                        LIBXSMM_X86_GP_REG_UNDEF, 0,
                                        0,
                                        i_micro_kernel_config->vector_name,
                                        reg_sum_squared, use_m_masking, 0, 1 );
    }
  }
}

LIBXSMM_API_INTERN
void libxsmm_generator_reduce_rows_avx512_microkernel( libxsmm_generated_code*                        io_generated_code,
    libxsmm_loop_label_tracker*                    io_loop_label_tracker,
    libxsmm_mateltwise_gp_reg_mapping*             i_gp_reg_mapping,
    const libxsmm_mateltwise_kernel_config*        i_micro_kernel_config,
    const libxsmm_meltw_descriptor*                i_mateltwise_desc ) {

  unsigned int i = 0, im, m, n, m_trips, n_trips, n_full_trips, use_m_masking, use_n_masking, mask_in_count, mask_out_count, n_cols_load = 16, compute_squared_vals_reduce, compute_plain_vals_reduce;

  /* Some rudimentary checking of M, N and LDs*/
  if ( i_mateltwise_desc->m > i_mateltwise_desc->ldi ) {
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_LDA );
    return;
  }

  if ( (i_mateltwise_desc->flags & LIBXSMM_MELTW_REDUCE_ELTS) > 0 ) {
    compute_plain_vals_reduce= 1;
  } else {
    compute_plain_vals_reduce= 0;
  }

  if ( (i_mateltwise_desc->flags & LIBXSMM_MELTW_REDUCE_ELTS_SQUARED) > 0 ) {
    compute_squared_vals_reduce = 1;
  } else {
    compute_squared_vals_reduce = 0;
  }

  /* Configure the register mapping for this eltwise kernel */
  i_gp_reg_mapping->gp_reg_in                   = LIBXSMM_X86_GP_REG_R8;
  i_gp_reg_mapping->gp_reg_reduced_elts         = LIBXSMM_X86_GP_REG_R9;
  i_gp_reg_mapping->gp_reg_reduced_elts_squared = LIBXSMM_X86_GP_REG_R10;
  i_gp_reg_mapping->gp_reg_m_loop               = LIBXSMM_X86_GP_REG_R11;
  i_gp_reg_mapping->gp_reg_n_loop               = LIBXSMM_X86_GP_REG_R12;

  /* load the input pointer and output pointer */
  libxsmm_x86_instruction_alu_mem( io_generated_code,
      i_micro_kernel_config->alu_mov_instruction,
      i_gp_reg_mapping->gp_reg_param_struct,
      LIBXSMM_X86_GP_REG_UNDEF, 0,
      0,
      i_gp_reg_mapping->gp_reg_in,
      0 );

  if ( compute_plain_vals_reduce > 0 ) {
    libxsmm_x86_instruction_alu_mem( io_generated_code,
        i_micro_kernel_config->alu_mov_instruction,
       i_gp_reg_mapping->gp_reg_param_struct,
       LIBXSMM_X86_GP_REG_UNDEF, 0,
       8,
       i_gp_reg_mapping->gp_reg_reduced_elts,
       0 );
  }

  if ( compute_squared_vals_reduce > 0 ) {
    libxsmm_x86_instruction_alu_mem( io_generated_code,
        i_micro_kernel_config->alu_mov_instruction,
       i_gp_reg_mapping->gp_reg_param_struct,
       LIBXSMM_X86_GP_REG_UNDEF, 0,
       16,
       i_gp_reg_mapping->gp_reg_reduced_elts_squared,
       0 );
  }

  /* We fully unroll in M dimension, calculate mask if there is remainder */
  m                 = i_mateltwise_desc->m;
  n                 = i_mateltwise_desc->n;
  use_m_masking     = ( m % 16 == 0 ) ? 0 : 1;
  use_n_masking     = ( n % 16 == 0 ) ? 0 : 1;
  m_trips           = (m + 15) / 16;
  n_trips           = (n + 15) / 16;
  n_full_trips      = ( n % 16 == 0 ) ? n_trips : n_trips-1;

  /* Calculate input mask in case we see m_masking */
  if (use_m_masking == 1) {
    /* Calculate mask reg 1 for input-reading */
    mask_in_count =  16 - (m % 16);
    libxsmm_generator_mateltwise_initialize_avx512_mask(io_generated_code, LIBXSMM_X86_GP_REG_R13, 1, mask_in_count, LIBXSMM_GEMM_PRECISION_F32);
  }

  /* Calculate output mask in case we see n_masking */
  if (use_n_masking == 1) {
    /* Calculate mask reg 2 for output-writing */
    mask_out_count = 16 - (n % 16);
    libxsmm_generator_mateltwise_initialize_avx512_mask(io_generated_code, LIBXSMM_X86_GP_REG_R13, 2, mask_out_count, LIBXSMM_GEMM_PRECISION_F32);
  }

  /* move blend mask value to GP register and to mask register 7 */
  libxsmm_x86_instruction_alu_imm( io_generated_code, LIBXSMM_X86_INSTR_MOVQ, LIBXSMM_X86_GP_REG_R13, 0xff00 );
  libxsmm_x86_instruction_mask_move( io_generated_code, LIBXSMM_X86_INSTR_KMOVW, LIBXSMM_X86_GP_REG_R13, 7, 0 );

  if (n_full_trips >= 1) {
    if (n_full_trips > 1) {
      /* open n loop */
      libxsmm_generator_mateltwise_header_n_loop(  io_generated_code, io_loop_label_tracker, i_micro_kernel_config, i_gp_reg_mapping->gp_reg_n_loop );
    }

    /* We fully unroll M loop here...  */
    for (im = 0; im < m_trips; im++) {
      /* load 16 columns of input matrix */
      for (i = 0 ; i < 16; i++) {
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            i_micro_kernel_config->vmove_instruction_in,
            i_gp_reg_mapping->gp_reg_in,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            (im * 16 + i * i_mateltwise_desc->ldi) * i_micro_kernel_config->datatype_size_in,
            i_micro_kernel_config->vector_name,
            i, (im == (m_trips-1)) ? use_m_masking : 0, 1, 0 );
      }

      /* 1st stage */
      /* zmm0/zmm4; 4444 4444 4444 4444 / 0000 0000 0000 0000 -> zmm0: 4444 4444 0000 0000 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     4, 0, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             4, 0, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              0, 0, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              4, 4, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 0 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 24 );
      }

      /* zmm8/zmm12; cccc cccc cccc cccc / 8888 8888 8888 8888 -> zmm8: cccc cccc 8888 8888 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     12, 8, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             12, 8, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              8, 8, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              12, 12, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 8 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 28 );
      }

      /* zmm1/zmm5; 5555 5555 5555 5555 / 1111 1111 1111 1111 -> zmm1: 5555 5555 1111 1111 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     5, 1, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             5, 1, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              1, 1, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              5, 5, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 1 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 25 );
      }

      /* zmm9/zmm13; dddd dddd dddd dddd / 9999 9999 9999 9999 -> zmm9: dddd dddd 9999 9999 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     13, 9, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             13, 9, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              9, 9, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              13, 13, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 9 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 29 );
      }

      /* zmm2/zmm6; 6666 6666 6666 6666 / 2222 2222 2222 2222 -> zmm2: 6666 6666 2222 2222 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     6, 2, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             6, 2, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              2, 2, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              6, 6, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 2 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 26 );
      }

      /* zmm10/zmm14; eeee eeee eeee eeee / aaaa aaaa aaaa aaaa -> zmm10: eeee eeee aaaa aaaa */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     14, 10, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             14, 10, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              10, 10, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              14, 14, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 10 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 30 );
      }

      /* zmm3/zmm7; 7777 7777 7777 7777 / 3333 3333 3333 3333  -> zmm3: 7777 7777 3333 3333 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     7, 3, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             7, 3, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              3, 3, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              7, 7, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 3 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 27 );
      }

      /* zmm11/zmm15; ffff ffff ffff ffff / bbbb bbbb bbbb bbbb  -> zmm11: ffff ffff bbbb bbbb */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     15, 11, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             15, 11, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              11, 11, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              15, 15, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 11 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 31 );
      }

      /* 2nd stage */
      /* zmm0/zmm8; 4444 4444 0000 0000 / cccc cccc 8888 8888  -> zmm0: cccc 8888 4444 0000 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 8, 0, 16, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 8, 0, 17, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 0 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 28, 24, 20, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 28, 24, 21, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 24 );
      }

      /* zmm1/zmm9; 5555 5555 1111 1111 / dddd dddd 9999 9999  -> zmm1: dddd 9999 5555 1111 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 9, 1, 16, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 9, 1, 17, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 1 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 29, 25, 20, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 29, 25, 21, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 25 );
      }

      /* zmm2/zmm10; 6666 6666 2222 2222 / eeee eeee aaaa aaaa  -> zmm2: eeee aaaa 6666 2222 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 10, 2, 16, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 10, 2, 17, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 2 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 30, 26, 20, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 30, 26, 21, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 26 );
      }

      /* zmm3/zmm11:  7777 7777 3333 3333 / ffff ffff bbbb bbbb  -> zmm3: ffff bbbb 7777 3333 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 11, 3, 16, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 11, 3, 17, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 3 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 31, 27, 20, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 31, 27, 21, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 27 );
      }

      /* 3rd stage */
      /* zmm0/zmm1; cccc 8888 4444 0000 / dddd 9999 5555 1111  -> zmm0: ddcc 9988 5544 1100 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 1, 0, 16, 0x44 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 1, 0, 17, 0xee );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 0 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 25, 24, 20, 0x44 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 25, 24, 21, 0xee );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 24 );
      }

      /* zmm2/zmm3; eeee aaaa 6666 2222 / ffff bbbb 7777 3333  -> zmm2: ffee bbaa 7766 3322 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 3, 2, 16, 0x44 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 3, 2, 17, 0xee );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 2 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 27, 26, 20, 0x44 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 27, 26, 21, 0xee );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 26 );
      }

      /* 4th stage */
      /* zmm0/zmm2; ddcc 9988 5544 1100 / ffee bbaa 7766 3322  -> zmm0: fedc ba98 7654 3210 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 2, 0, 16, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 2, 0, 17, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 0 );

        /* Update the running reduction result */
        if (im == 0) {
          libxsmm_x86_instruction_vec_move( io_generated_code,
                                            i_micro_kernel_config->instruction_set,
                                            i_micro_kernel_config->vmove_instruction_out,
                                            i_gp_reg_mapping->gp_reg_reduced_elts,
                                            LIBXSMM_X86_GP_REG_UNDEF, 0,
                                            0,
                                            i_micro_kernel_config->vector_name,
                                            0, 0, 0, 1 );
        } else {
          libxsmm_x86_instruction_vec_move( io_generated_code,
              i_micro_kernel_config->instruction_set,
              i_micro_kernel_config->vmove_instruction_out,
              i_gp_reg_mapping->gp_reg_reduced_elts,
              LIBXSMM_X86_GP_REG_UNDEF, 0,
              0,
              i_micro_kernel_config->vector_name,
              1, 0, 1, 0 );

          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                  i_micro_kernel_config->instruction_set,
                                                  LIBXSMM_X86_INSTR_VADDPS,
                                                  i_micro_kernel_config->vector_name,
                                                  1, 0, 0 );

          libxsmm_x86_instruction_vec_move( io_generated_code,
                                            i_micro_kernel_config->instruction_set,
                                            i_micro_kernel_config->vmove_instruction_out,
                                            i_gp_reg_mapping->gp_reg_reduced_elts,
                                            LIBXSMM_X86_GP_REG_UNDEF, 0,
                                            0,
                                            i_micro_kernel_config->vector_name,
                                            0, 0, 0, 1 );
        }
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 26, 24, 20, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 26, 24, 21, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 24 );

        /* Update the running reduction result */
        if (im == 0) {
          libxsmm_x86_instruction_vec_move( io_generated_code,
                                            i_micro_kernel_config->instruction_set,
                                            i_micro_kernel_config->vmove_instruction_out,
                                            i_gp_reg_mapping->gp_reg_reduced_elts_squared,
                                            LIBXSMM_X86_GP_REG_UNDEF, 0,
                                            0,
                                            i_micro_kernel_config->vector_name,
                                            24, 0, 0, 1 );
        } else {
          libxsmm_x86_instruction_vec_move( io_generated_code,
              i_micro_kernel_config->instruction_set,
              i_micro_kernel_config->vmove_instruction_out,
              i_gp_reg_mapping->gp_reg_reduced_elts_squared,
              LIBXSMM_X86_GP_REG_UNDEF, 0,
              0,
              i_micro_kernel_config->vector_name,
              25, 0, 1, 0 );

          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                  i_micro_kernel_config->instruction_set,
                                                  LIBXSMM_X86_INSTR_VADDPS,
                                                  i_micro_kernel_config->vector_name,
                                                  25, 24, 24 );

          libxsmm_x86_instruction_vec_move( io_generated_code,
                                            i_micro_kernel_config->instruction_set,
                                            i_micro_kernel_config->vmove_instruction_out,
                                            i_gp_reg_mapping->gp_reg_reduced_elts_squared,
                                            LIBXSMM_X86_GP_REG_UNDEF, 0,
                                            0,
                                            i_micro_kernel_config->vector_name,
                                            24, 0, 0, 1 );
        }
      }
    }

    if ((n_full_trips >  1) || (n % 16 != 0)) {
      /* Adjust input and output pointer */
      libxsmm_x86_instruction_alu_imm(  io_generated_code,
          i_micro_kernel_config->alu_add_instruction,
          i_gp_reg_mapping->gp_reg_in,
          16 * i_mateltwise_desc->ldi *  i_micro_kernel_config->datatype_size_in);

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_alu_imm(  io_generated_code,
          i_micro_kernel_config->alu_add_instruction,
          i_gp_reg_mapping->gp_reg_reduced_elts,
          16 * i_micro_kernel_config->datatype_size_out);
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_alu_imm(  io_generated_code,
          i_micro_kernel_config->alu_add_instruction,
          i_gp_reg_mapping->gp_reg_reduced_elts_squared,
          16 * i_micro_kernel_config->datatype_size_out);
      }
    }

    if (n_full_trips > 1) {
      /* close n loop */
      libxsmm_generator_mateltwise_footer_n_loop(  io_generated_code, io_loop_label_tracker, i_micro_kernel_config, i_gp_reg_mapping->gp_reg_n_loop, n_full_trips);
    }
  }

  /* In this case we load only partial number of columns  */
  n_cols_load = n % 16;
  if (n_cols_load != 0) {
    /* We fully unroll M loop here...  */
    for (im = 0; im < m_trips; im++) {
      /* load 16 columns of input matrix */
      for (i = 0 ; i < n_cols_load; i++) {
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            i_micro_kernel_config->vmove_instruction_in,
            i_gp_reg_mapping->gp_reg_in,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            (im * 16 + i * i_mateltwise_desc->ldi) * i_micro_kernel_config->datatype_size_in,
            i_micro_kernel_config->vector_name,
            i, (im == (m_trips-1)) ? use_m_masking : 0, 1, 0 );
      }

      for ( i = n_cols_load; i < 16; i++) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VPXORD,
                                                 i_micro_kernel_config->vector_name,
                                                 i, i, i );
      }

      /* 1st stage */
      /* zmm0/zmm4; 4444 4444 4444 4444 / 0000 0000 0000 0000 -> zmm0: 4444 4444 0000 0000 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     4, 0, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             4, 0, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              0, 0, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              4, 4, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 0 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 24 );
      }

      if (n_cols_load > 7) {
        /* zmm8/zmm12; cccc cccc cccc cccc / 8888 8888 8888 8888 -> zmm8: cccc cccc 8888 8888 */
        if ( compute_plain_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                       i_micro_kernel_config->instruction_set,
                                                       LIBXSMM_X86_INSTR_VBLENDMPS,
                                                       i_micro_kernel_config->vector_name,
                                                       12, 8, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

          libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VSHUFF64X2,
                                               i_micro_kernel_config->vector_name,
                                               12, 8, 17, 0x4e );
        }

        if ( compute_squared_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                i_micro_kernel_config->instruction_set,
                                                LIBXSMM_X86_INSTR_VMULPS,
                                                i_micro_kernel_config->vector_name,
                                                8, 8, 18 );

          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                i_micro_kernel_config->instruction_set,
                                                LIBXSMM_X86_INSTR_VMULPS,
                                                i_micro_kernel_config->vector_name,
                                                12, 12, 19 );


          libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                       i_micro_kernel_config->instruction_set,
                                                       LIBXSMM_X86_INSTR_VBLENDMPS,
                                                       i_micro_kernel_config->vector_name,
                                                       19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

          libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VSHUFF64X2,
                                               i_micro_kernel_config->vector_name,
                                               19, 18, 21, 0x4e );
        }

        if ( compute_plain_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               16, 17, 8 );
        }

        if ( compute_squared_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               20, 21, 28 );
        }
      }

      /* zmm1/zmm5; 5555 5555 5555 5555 / 1111 1111 1111 1111 -> zmm1: 5555 5555 1111 1111 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     5, 1, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             5, 1, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              1, 1, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              5, 5, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 1 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 25 );
      }

      if (n_cols_load > 8) {
        /* zmm9/zmm13; dddd dddd dddd dddd / 9999 9999 9999 9999 -> zmm9: dddd dddd 9999 9999 */
        if ( compute_plain_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                       i_micro_kernel_config->instruction_set,
                                                       LIBXSMM_X86_INSTR_VBLENDMPS,
                                                       i_micro_kernel_config->vector_name,
                                                       13, 9, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

          libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VSHUFF64X2,
                                               i_micro_kernel_config->vector_name,
                                               13, 9, 17, 0x4e );
        }

        if ( compute_squared_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                i_micro_kernel_config->instruction_set,
                                                LIBXSMM_X86_INSTR_VMULPS,
                                                i_micro_kernel_config->vector_name,
                                                9, 9, 18 );

          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                i_micro_kernel_config->instruction_set,
                                                LIBXSMM_X86_INSTR_VMULPS,
                                                i_micro_kernel_config->vector_name,
                                                13, 13, 19 );


          libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                       i_micro_kernel_config->instruction_set,
                                                       LIBXSMM_X86_INSTR_VBLENDMPS,
                                                       i_micro_kernel_config->vector_name,
                                                       19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

          libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VSHUFF64X2,
                                               i_micro_kernel_config->vector_name,
                                               19, 18, 21, 0x4e );
        }

        if ( compute_plain_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               16, 17, 9 );
        }

        if ( compute_squared_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               20, 21, 29 );
        }
      }

      /* zmm2/zmm6; 6666 6666 6666 6666 / 2222 2222 2222 2222 -> zmm2: 6666 6666 2222 2222 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     6, 2, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             6, 2, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              2, 2, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              6, 6, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 2 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 26 );
      }

      if (n_cols_load > 9) {
        /* zmm10/zmm14; eeee eeee eeee eeee / aaaa aaaa aaaa aaaa -> zmm10: eeee eeee aaaa aaaa */
        if ( compute_plain_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                       i_micro_kernel_config->instruction_set,
                                                       LIBXSMM_X86_INSTR_VBLENDMPS,
                                                       i_micro_kernel_config->vector_name,
                                                       14, 10, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

          libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VSHUFF64X2,
                                               i_micro_kernel_config->vector_name,
                                               14, 10, 17, 0x4e );
        }

        if ( compute_squared_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                i_micro_kernel_config->instruction_set,
                                                LIBXSMM_X86_INSTR_VMULPS,
                                                i_micro_kernel_config->vector_name,
                                                10, 10, 18 );

          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                i_micro_kernel_config->instruction_set,
                                                LIBXSMM_X86_INSTR_VMULPS,
                                                i_micro_kernel_config->vector_name,
                                                14, 14, 19 );


          libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                       i_micro_kernel_config->instruction_set,
                                                       LIBXSMM_X86_INSTR_VBLENDMPS,
                                                       i_micro_kernel_config->vector_name,
                                                       19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

          libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VSHUFF64X2,
                                               i_micro_kernel_config->vector_name,
                                               19, 18, 21, 0x4e );
        }

        if ( compute_plain_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               16, 17, 10 );
        }

        if ( compute_squared_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               20, 21, 30 );
        }
      }

      /* zmm3/zmm7; 7777 7777 7777 7777 / 3333 3333 3333 3333  -> zmm3: 7777 7777 3333 3333 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     7, 3, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             7, 3, 17, 0x4e );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              3, 3, 18 );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                              i_micro_kernel_config->instruction_set,
                                              LIBXSMM_X86_INSTR_VMULPS,
                                              i_micro_kernel_config->vector_name,
                                              7, 7, 19 );


        libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                     i_micro_kernel_config->instruction_set,
                                                     LIBXSMM_X86_INSTR_VBLENDMPS,
                                                     i_micro_kernel_config->vector_name,
                                                     19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VSHUFF64X2,
                                             i_micro_kernel_config->vector_name,
                                             19, 18, 21, 0x4e );
      }

      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             16, 17, 3 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             20, 21, 27 );
      }

      if (n_cols_load > 10) {
        /* zmm11/zmm15; ffff ffff ffff ffff / bbbb bbbb bbbb bbbb  -> zmm11: ffff ffff bbbb bbbb */
        if ( compute_plain_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                       i_micro_kernel_config->instruction_set,
                                                       LIBXSMM_X86_INSTR_VBLENDMPS,
                                                       i_micro_kernel_config->vector_name,
                                                       15, 11, 16, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

          libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VSHUFF64X2,
                                               i_micro_kernel_config->vector_name,
                                               15, 11, 17, 0x4e );
        }

        if ( compute_squared_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                i_micro_kernel_config->instruction_set,
                                                LIBXSMM_X86_INSTR_VMULPS,
                                                i_micro_kernel_config->vector_name,
                                                11, 11, 18 );

          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                i_micro_kernel_config->instruction_set,
                                                LIBXSMM_X86_INSTR_VMULPS,
                                                i_micro_kernel_config->vector_name,
                                                15, 15, 19 );


          libxsmm_x86_instruction_vec_compute_reg_mask( io_generated_code,
                                                       i_micro_kernel_config->instruction_set,
                                                       LIBXSMM_X86_INSTR_VBLENDMPS,
                                                       i_micro_kernel_config->vector_name,
                                                       19, 18, 20, LIBXSMM_X86_IMM_UNDEF, 7, 0 );

          libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VSHUFF64X2,
                                               i_micro_kernel_config->vector_name,
                                               19, 18, 21, 0x4e );
        }

        if ( compute_plain_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               16, 17, 11 );
        }

        if ( compute_squared_vals_reduce > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               20, 21, 31 );
        }
      }

      /* 2nd stage */
      /* zmm0/zmm8; 4444 4444 0000 0000 / cccc cccc 8888 8888  -> zmm0: cccc 8888 4444 0000 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 8, 0, 16, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 8, 0, 17, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 0 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 28, 24, 20, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 28, 24, 21, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 24 );
      }

      /* zmm1/zmm9; 5555 5555 1111 1111 / dddd dddd 9999 9999  -> zmm1: dddd 9999 5555 1111 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 9, 1, 16, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 9, 1, 17, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 1 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 29, 25, 20, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 29, 25, 21, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 25 );
      }

      /* zmm2/zmm10; 6666 6666 2222 2222 / eeee eeee aaaa aaaa  -> zmm2: eeee aaaa 6666 2222 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 10, 2, 16, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 10, 2, 17, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 2 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 30, 26, 20, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 30, 26, 21, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 26 );
      }

      /* zmm3/zmm11:  7777 7777 3333 3333 / ffff ffff bbbb bbbb  -> zmm3: ffff bbbb 7777 3333 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 11, 3, 16, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 11, 3, 17, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 3 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 31, 27, 20, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFF64X2,
                                                 i_micro_kernel_config->vector_name,
                                                 31, 27, 21, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 27 );
      }

      /* 3rd stage */
      /* zmm0/zmm1; cccc 8888 4444 0000 / dddd 9999 5555 1111  -> zmm0: ddcc 9988 5544 1100 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 1, 0, 16, 0x44 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 1, 0, 17, 0xee );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 0 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 25, 24, 20, 0x44 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 25, 24, 21, 0xee );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 24 );
      }

      /* zmm2/zmm3; eeee aaaa 6666 2222 / ffff bbbb 7777 3333  -> zmm2: ffee bbaa 7766 3322 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 3, 2, 16, 0x44 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 3, 2, 17, 0xee );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 2 );
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 27, 26, 20, 0x44 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 27, 26, 21, 0xee );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 26 );
      }

      /* 4th stage */
      /* zmm0/zmm2; ddcc 9988 5544 1100 / ffee bbaa 7766 3322  -> zmm0: fedc ba98 7654 3210 */
      if ( compute_plain_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 2, 0, 16, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 2, 0, 17, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 16, 17, 0 );

        /* Update the running reduction result */
        if (im == 0) {
          libxsmm_x86_instruction_vec_move( io_generated_code,
                                            i_micro_kernel_config->instruction_set,
                                            i_micro_kernel_config->vmove_instruction_out,
                                            i_gp_reg_mapping->gp_reg_reduced_elts,
                                            LIBXSMM_X86_GP_REG_UNDEF, 0,
                                            0,
                                            i_micro_kernel_config->vector_name,
                                            0, 2, 0, 1 );
        } else {
          libxsmm_x86_instruction_vec_move( io_generated_code,
              i_micro_kernel_config->instruction_set,
              i_micro_kernel_config->vmove_instruction_out,
              i_gp_reg_mapping->gp_reg_reduced_elts,
              LIBXSMM_X86_GP_REG_UNDEF, 0,
              0,
              i_micro_kernel_config->vector_name,
              1, 2, 1, 0 );

          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                  i_micro_kernel_config->instruction_set,
                                                  LIBXSMM_X86_INSTR_VADDPS,
                                                  i_micro_kernel_config->vector_name,
                                                  1, 0, 0 );

          libxsmm_x86_instruction_vec_move( io_generated_code,
                                            i_micro_kernel_config->instruction_set,
                                            i_micro_kernel_config->vmove_instruction_out,
                                            i_gp_reg_mapping->gp_reg_reduced_elts,
                                            LIBXSMM_X86_GP_REG_UNDEF, 0,
                                            0,
                                            i_micro_kernel_config->vector_name,
                                            0, 2, 0, 1 );
        }
      }

      if ( compute_squared_vals_reduce > 0 ) {
        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 26, 24, 20, 0x88 );

        libxsmm_x86_instruction_vec_shuffle_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VSHUFPS,
                                                 i_micro_kernel_config->vector_name,
                                                 26, 24, 21, 0xdd );

        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                 i_micro_kernel_config->instruction_set,
                                                 LIBXSMM_X86_INSTR_VADDPS,
                                                 i_micro_kernel_config->vector_name,
                                                 20, 21, 24 );

        /* Update the running reduction result */
        if (im == 0) {
          libxsmm_x86_instruction_vec_move( io_generated_code,
                                            i_micro_kernel_config->instruction_set,
                                            i_micro_kernel_config->vmove_instruction_out,
                                            i_gp_reg_mapping->gp_reg_reduced_elts_squared,
                                            LIBXSMM_X86_GP_REG_UNDEF, 0,
                                            0,
                                            i_micro_kernel_config->vector_name,
                                            24, 2, 0, 1 );
        } else {
          libxsmm_x86_instruction_vec_move( io_generated_code,
              i_micro_kernel_config->instruction_set,
              i_micro_kernel_config->vmove_instruction_out,
              i_gp_reg_mapping->gp_reg_reduced_elts_squared,
              LIBXSMM_X86_GP_REG_UNDEF, 0,
              0,
              i_micro_kernel_config->vector_name,
              25, 2, 1, 0 );

          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                                  i_micro_kernel_config->instruction_set,
                                                  LIBXSMM_X86_INSTR_VADDPS,
                                                  i_micro_kernel_config->vector_name,
                                                  25, 24, 24 );

          libxsmm_x86_instruction_vec_move( io_generated_code,
                                            i_micro_kernel_config->instruction_set,
                                            i_micro_kernel_config->vmove_instruction_out,
                                            i_gp_reg_mapping->gp_reg_reduced_elts_squared,
                                            LIBXSMM_X86_GP_REG_UNDEF, 0,
                                            0,
                                            i_micro_kernel_config->vector_name,
                                            24, 2, 0, 1 );
        }
      }
    }
  }
}

LIBXSMM_API_INTERN
void libxsmm_generator_scale_avx512_microkernel( libxsmm_generated_code*                        io_generated_code,
    libxsmm_loop_label_tracker*                    io_loop_label_tracker,
    libxsmm_mateltwise_gp_reg_mapping*             i_gp_reg_mapping,
    const libxsmm_mateltwise_kernel_config*        i_micro_kernel_config,
    const libxsmm_meltw_descriptor*                i_mateltwise_desc ) {

  unsigned int in, im, m, n, m_full_trips, m_trips, use_m_masking, mask_count, reg_n, reg_m;
  unsigned int scale_rows = 0, scale_cols = 0, perform_scale = 0, perform_shift = 0, perform_addbias = 0;
  unsigned int reg_shift = 31, reg_bias = 30, reg_scale = 29;

  /* Some rudimentary checking of M, N and LDs*/
  if ( i_mateltwise_desc->m > i_mateltwise_desc->ldi ) {
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_LDA );
    return;
  }

  if ( (((i_mateltwise_desc->flags & LIBXSMM_MELTW_SCALE_ROWS) > 0) && ((i_mateltwise_desc->flags & LIBXSMM_MELTW_SCALE_COLS) > 0)) ||
       (((i_mateltwise_desc->flags & LIBXSMM_MELTW_SCALE_ROWS) == 0) && ((i_mateltwise_desc->flags & LIBXSMM_MELTW_SCALE_COLS) == 0)) ) {
    /* This should not happen  */
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_GENERAL );
    return;
  }

  /* Determine what operations to perform */
  scale_rows    = ((i_mateltwise_desc->flags & LIBXSMM_MELTW_SCALE_ROWS) > 0) ? 1 : 0;
  scale_cols    = ((i_mateltwise_desc->flags & LIBXSMM_MELTW_SCALE_COLS) > 0) ? 1 : 0;
  perform_scale = ((i_mateltwise_desc->flags & LIBXSMM_MELTW_SCALE_MULT) > 0) ? 1 : 0;
  perform_shift = ((i_mateltwise_desc->flags & LIBXSMM_MELTW_SCALE_SHIFT) > 0) ? 1 : 0;
  perform_addbias = ((i_mateltwise_desc->flags & LIBXSMM_MELTW_SCALE_ADD_BIAS) > 0) ? 1 : 0;

  /* Configure the register mapping for this eltwise kernel */
  i_gp_reg_mapping->gp_reg_in                     = LIBXSMM_X86_GP_REG_R8;
  i_gp_reg_mapping->gp_reg_out                    = LIBXSMM_X86_GP_REG_R9;
  i_gp_reg_mapping->gp_reg_shift_vals             = LIBXSMM_X86_GP_REG_R10;
  i_gp_reg_mapping->gp_reg_scale_vals             = LIBXSMM_X86_GP_REG_R11;
  i_gp_reg_mapping->gp_reg_bias_vals              = LIBXSMM_X86_GP_REG_R12;
  i_gp_reg_mapping->gp_reg_m_loop                 = LIBXSMM_X86_GP_REG_R13;
  i_gp_reg_mapping->gp_reg_n_loop                 = LIBXSMM_X86_GP_REG_R14;

  /* We fully unroll in N dimension, calculate m-mask if there is remainder */
  m                 = i_mateltwise_desc->m;
  n                 = i_mateltwise_desc->n;
  use_m_masking     = ( m % 16 == 0 ) ? 0 : 1;

  /* Calculate input mask in case we see m_masking */
  if (use_m_masking == 1) {
    /* Calculate mask reg 1 for input-reading */
    mask_count =  16 - (m % 16);
    libxsmm_generator_mateltwise_initialize_avx512_mask(io_generated_code, LIBXSMM_X86_GP_REG_R13, 1, mask_count, LIBXSMM_GEMM_PRECISION_F32);
  }

  /* load the input pointer(s) and output pointer */
  libxsmm_x86_instruction_alu_mem( io_generated_code,
      i_micro_kernel_config->alu_mov_instruction,
      i_gp_reg_mapping->gp_reg_param_struct,
      LIBXSMM_X86_GP_REG_UNDEF, 0,
      0,
      i_gp_reg_mapping->gp_reg_in,
      0 );

  if ( perform_shift > 0 ) {
    libxsmm_x86_instruction_alu_mem( io_generated_code,
       i_micro_kernel_config->alu_mov_instruction,
       i_gp_reg_mapping->gp_reg_param_struct,
       LIBXSMM_X86_GP_REG_UNDEF, 0,
       8,
       i_gp_reg_mapping->gp_reg_shift_vals,
       0 );
  }

  if ( perform_scale > 0 ) {
    libxsmm_x86_instruction_alu_mem( io_generated_code,
       i_micro_kernel_config->alu_mov_instruction,
       i_gp_reg_mapping->gp_reg_param_struct,
       LIBXSMM_X86_GP_REG_UNDEF, 0,
       16,
       i_gp_reg_mapping->gp_reg_scale_vals,
       0 );
  }

  if ( perform_addbias > 0 ) {
    libxsmm_x86_instruction_alu_mem( io_generated_code,
       i_micro_kernel_config->alu_mov_instruction,
       i_gp_reg_mapping->gp_reg_param_struct,
       LIBXSMM_X86_GP_REG_UNDEF, 0,
       24,
       i_gp_reg_mapping->gp_reg_bias_vals,
       0 );
  }

  libxsmm_x86_instruction_alu_mem( io_generated_code,
      i_micro_kernel_config->alu_mov_instruction,
      i_gp_reg_mapping->gp_reg_param_struct,
      LIBXSMM_X86_GP_REG_UNDEF, 0,
      32,
      i_gp_reg_mapping->gp_reg_out,
      0 );

  /* If scaling cols: follow an MN loop order with fully unrolled N loop */
  if (scale_cols == 1) {
    m_full_trips = m / 16;

    if ( m_full_trips >= 1 ) {
      if (m_full_trips > 1) {
        /* open m loop */
        libxsmm_generator_mateltwise_header_m_loop(  io_generated_code, io_loop_label_tracker, i_micro_kernel_config, i_gp_reg_mapping->gp_reg_m_loop );
      }

      /* Load the correspodning columns to be used for scaling */
      if ( perform_shift > 0 ) {
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            i_micro_kernel_config->vmove_instruction_in,
            i_gp_reg_mapping->gp_reg_shift_vals,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            0,
            i_micro_kernel_config->vector_name,
            reg_shift, 0, 1, 0 );
      }

      if ( perform_scale > 0 ) {
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            i_micro_kernel_config->vmove_instruction_in,
            i_gp_reg_mapping->gp_reg_scale_vals,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            0,
            i_micro_kernel_config->vector_name,
            reg_scale, 0, 1, 0 );
      }


      if ( perform_addbias > 0 ) {
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            i_micro_kernel_config->vmove_instruction_in,
            i_gp_reg_mapping->gp_reg_bias_vals,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            0,
            i_micro_kernel_config->vector_name,
            reg_bias, 0, 1, 0 );
      }

      for (in = 0; in < n; in++) {
        reg_n = in % 29;

        /* Load part of the column  */
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            i_micro_kernel_config->vmove_instruction_in,
            i_gp_reg_mapping->gp_reg_in,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            in * i_mateltwise_desc->ldi * i_micro_kernel_config->datatype_size_in,
            i_micro_kernel_config->vector_name,
            reg_n, 0, 1, 0 );

        /* Perform transformations */
        if ( perform_shift > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               reg_n, reg_shift, reg_n );
        }

        if ( perform_scale > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VMULPS,
                                               i_micro_kernel_config->vector_name,
                                               reg_n, reg_scale, reg_n );
        }

        if ( perform_addbias> 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               reg_n, reg_bias, reg_n );
        }

        /* Store part of the column */
        libxsmm_x86_instruction_vec_move( io_generated_code,
                                          i_micro_kernel_config->instruction_set,
                                          i_micro_kernel_config->vmove_instruction_out,
                                          i_gp_reg_mapping->gp_reg_out,
                                          LIBXSMM_X86_GP_REG_UNDEF, 0,
                                          in * i_mateltwise_desc->ldo * i_micro_kernel_config->datatype_size_out,
                                          i_micro_kernel_config->vector_name,
                                          reg_n, 0, 0, 1 );
      }

      if ((m_full_trips > 1) || (use_m_masking == 1)) {
        /* Adjust input and output pointer */
        libxsmm_x86_instruction_alu_imm(  io_generated_code,
            i_micro_kernel_config->alu_add_instruction,
            i_gp_reg_mapping->gp_reg_in,
            16 * i_micro_kernel_config->datatype_size_in);

        libxsmm_x86_instruction_alu_imm(  io_generated_code,
            i_micro_kernel_config->alu_add_instruction,
            i_gp_reg_mapping->gp_reg_out,
            16 * i_micro_kernel_config->datatype_size_out);

        if ( perform_shift > 0 ) {
          libxsmm_x86_instruction_alu_imm(  io_generated_code,
              i_micro_kernel_config->alu_add_instruction,
              i_gp_reg_mapping->gp_reg_shift_vals,
              16 * i_micro_kernel_config->datatype_size_in);
        }

        if ( perform_scale > 0 ) {
          libxsmm_x86_instruction_alu_imm(  io_generated_code,
              i_micro_kernel_config->alu_add_instruction,
              i_gp_reg_mapping->gp_reg_scale_vals,
              16 * i_micro_kernel_config->datatype_size_in);
        }

        if ( perform_addbias > 0 ) {
          libxsmm_x86_instruction_alu_imm(  io_generated_code,
              i_micro_kernel_config->alu_add_instruction,
              i_gp_reg_mapping->gp_reg_bias_vals,
              16 * i_micro_kernel_config->datatype_size_in);
        }
      }

      if (m_full_trips > 1) {
        /* close m loop */
        libxsmm_generator_mateltwise_footer_m_loop(  io_generated_code, io_loop_label_tracker, i_micro_kernel_config, i_gp_reg_mapping->gp_reg_m_loop, m_full_trips);
      }
    }

    if (use_m_masking == 1) {
      /* Load the correspodning columns to be used for scaling */
      if ( perform_shift > 0 ) {
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            i_micro_kernel_config->vmove_instruction_in,
            i_gp_reg_mapping->gp_reg_shift_vals,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            0,
            i_micro_kernel_config->vector_name,
            reg_shift, 0, 1, 0 );
      }

      if ( perform_scale > 0 ) {
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            i_micro_kernel_config->vmove_instruction_in,
            i_gp_reg_mapping->gp_reg_scale_vals,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            0,
            i_micro_kernel_config->vector_name,
            reg_scale, 0, 1, 0 );
      }


      if ( perform_addbias > 0 ) {
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            i_micro_kernel_config->vmove_instruction_in,
            i_gp_reg_mapping->gp_reg_bias_vals,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            0,
            i_micro_kernel_config->vector_name,
            reg_bias, 0, 1, 0 );
      }

      for (in = 0; in < n; in++) {
        reg_n = in % 29;

        /* Load part of the column  */
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            i_micro_kernel_config->vmove_instruction_in,
            i_gp_reg_mapping->gp_reg_in,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            in * i_mateltwise_desc->ldi * i_micro_kernel_config->datatype_size_in,
            i_micro_kernel_config->vector_name,
            reg_n, use_m_masking, 1, 0 );

        /* Perform transformations */
        if ( perform_shift > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               reg_n, reg_shift, reg_n );
        }

        if ( perform_scale > 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VMULPS,
                                               i_micro_kernel_config->vector_name,
                                               reg_n, reg_scale, reg_n );
        }

        if ( perform_addbias> 0 ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                               i_micro_kernel_config->instruction_set,
                                               LIBXSMM_X86_INSTR_VADDPS,
                                               i_micro_kernel_config->vector_name,
                                               reg_n, reg_bias, reg_n );
        }

        /* Store part of the column */
        libxsmm_x86_instruction_vec_move( io_generated_code,
                                          i_micro_kernel_config->instruction_set,
                                          i_micro_kernel_config->vmove_instruction_out,
                                          i_gp_reg_mapping->gp_reg_out,
                                          LIBXSMM_X86_GP_REG_UNDEF, 0,
                                          in * i_mateltwise_desc->ldo * i_micro_kernel_config->datatype_size_out,
                                          i_micro_kernel_config->vector_name,
                                          reg_n, use_m_masking, 0, 1 );
      }
    }
  }

  /* If scaling rows: follow an NM loop order with fully unrolled M loop */
  if (scale_rows == 1) {
    m_trips = (m + 15) / 16;
    if (n > 1) {
      /* open n loop */
      libxsmm_generator_mateltwise_header_n_loop(  io_generated_code, io_loop_label_tracker, i_micro_kernel_config, i_gp_reg_mapping->gp_reg_n_loop );
    }

    /* Load the correspodning columns to be used for scaling */
    if ( perform_shift > 0 ) {
      libxsmm_x86_instruction_vec_move( io_generated_code,
          i_micro_kernel_config->instruction_set,
          LIBXSMM_X86_INSTR_VBROADCASTSS,
          i_gp_reg_mapping->gp_reg_shift_vals,
          LIBXSMM_X86_GP_REG_UNDEF, 0,
          0,
          i_micro_kernel_config->vector_name,
          reg_shift, 0, 1, 0 );
    }

    if ( perform_scale > 0 ) {
      libxsmm_x86_instruction_vec_move( io_generated_code,
          i_micro_kernel_config->instruction_set,
          LIBXSMM_X86_INSTR_VBROADCASTSS,
          i_gp_reg_mapping->gp_reg_scale_vals,
          LIBXSMM_X86_GP_REG_UNDEF, 0,
          0,
          i_micro_kernel_config->vector_name,
          reg_scale, 0, 1, 0 );
    }

    if ( perform_addbias > 0 ) {
      libxsmm_x86_instruction_vec_move( io_generated_code,
          i_micro_kernel_config->instruction_set,
          LIBXSMM_X86_INSTR_VBROADCASTSS,
          i_gp_reg_mapping->gp_reg_bias_vals,
          LIBXSMM_X86_GP_REG_UNDEF, 0,
          0,
          i_micro_kernel_config->vector_name,
          reg_bias, 0, 1, 0 );
    }

    for (im = 0; im < m_trips; im++) {
      reg_m = im % 29;

      libxsmm_x86_instruction_vec_move( io_generated_code,
          i_micro_kernel_config->instruction_set,
          i_micro_kernel_config->vmove_instruction_in,
          i_gp_reg_mapping->gp_reg_in,
          LIBXSMM_X86_GP_REG_UNDEF, 0,
          im * 16 * i_micro_kernel_config->datatype_size_in,
          i_micro_kernel_config->vector_name,
          reg_m, (im == (m_trips-1)) ? use_m_masking : 0, 1, 0 );

      /* Perform transformations */
      if ( perform_shift > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             reg_m, reg_shift, reg_m );
      }

      if ( perform_scale > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VMULPS,
                                             i_micro_kernel_config->vector_name,
                                             reg_m, reg_scale, reg_m );
      }

      if ( perform_addbias > 0 ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
                                             i_micro_kernel_config->instruction_set,
                                             LIBXSMM_X86_INSTR_VADDPS,
                                             i_micro_kernel_config->vector_name,
                                             reg_m, reg_bias, reg_m );
      }

      /* Store the result  */
      libxsmm_x86_instruction_vec_move( io_generated_code,
          i_micro_kernel_config->instruction_set,
          i_micro_kernel_config->vmove_instruction_out,
          i_gp_reg_mapping->gp_reg_out,
          LIBXSMM_X86_GP_REG_UNDEF, 0,
          im * 16 * i_micro_kernel_config->datatype_size_out,
          i_micro_kernel_config->vector_name,
          reg_m, (im == (m_trips-1)) ? use_m_masking : 0, 0, 1 );
    }

    if (n > 1) {
      /* Adjust input and output pointer */
      libxsmm_x86_instruction_alu_imm(  io_generated_code,
          i_micro_kernel_config->alu_add_instruction,
          i_gp_reg_mapping->gp_reg_in,
          i_mateltwise_desc->ldi *  i_micro_kernel_config->datatype_size_in);

      libxsmm_x86_instruction_alu_imm(  io_generated_code,
          i_micro_kernel_config->alu_add_instruction,
          i_gp_reg_mapping->gp_reg_out,
          i_mateltwise_desc->ldo * i_micro_kernel_config->datatype_size_out);

      if ( perform_shift > 0 ) {
        libxsmm_x86_instruction_alu_imm(  io_generated_code,
            i_micro_kernel_config->alu_add_instruction,
            i_gp_reg_mapping->gp_reg_shift_vals,
            i_micro_kernel_config->datatype_size_in);
      }

      if ( perform_scale > 0 ) {
        libxsmm_x86_instruction_alu_imm(  io_generated_code,
            i_micro_kernel_config->alu_add_instruction,
            i_gp_reg_mapping->gp_reg_scale_vals,
            i_micro_kernel_config->datatype_size_in);
      }

      if ( perform_addbias > 0 ) {
        libxsmm_x86_instruction_alu_imm(  io_generated_code,
            i_micro_kernel_config->alu_add_instruction,
            i_gp_reg_mapping->gp_reg_bias_vals,
            i_micro_kernel_config->datatype_size_in);
      }

      /* close n loop */
      libxsmm_generator_mateltwise_footer_n_loop(  io_generated_code, io_loop_label_tracker, i_micro_kernel_config, i_gp_reg_mapping->gp_reg_n_loop, n);
    }

  }
}

LIBXSMM_API_INTERN
void libxsmm_generator_mateltwise_avx_avx512_kernel( libxsmm_generated_code*         io_generated_code,
    const libxsmm_meltw_descriptor* i_mateltwise_desc) {
  libxsmm_mateltwise_kernel_config  l_kernel_config;
  libxsmm_mateltwise_gp_reg_mapping l_gp_reg_mapping;
  libxsmm_loop_label_tracker        l_loop_label_tracker;
  /* define loop_label_tracker */
  libxsmm_reset_loop_label_tracker( &l_loop_label_tracker );

  /* define gp register mapping */
  memset(&l_gp_reg_mapping, 0, sizeof(l_gp_reg_mapping));
#if defined(_WIN32) || defined(__CYGWIN__)
  l_gp_reg_mapping.gp_reg_param_struct = LIBXSMM_X86_GP_REG_RCX;
#else /* match calling convention on Linux */
  l_gp_reg_mapping.gp_reg_param_struct = LIBXSMM_X86_GP_REG_RDI;
#endif

  /* define mateltwise kernel config */
  libxsmm_generator_mateltwise_init_micro_kernel_config_fullvector( io_generated_code, &l_kernel_config, io_generated_code->arch, i_mateltwise_desc);

  /* open asm */
  libxsmm_x86_instruction_open_stream_mateltwise( io_generated_code, l_gp_reg_mapping.gp_reg_param_struct, NULL );

  /* Depending on the elementwise function, dispatch the proper code JITer */
  if (i_mateltwise_desc->operation == LIBXSMM_MELTW_OPERATION_CVTFP32BF16) {
    if ( (LIBXSMM_GEMM_PRECISION_F32 == LIBXSMM_GETENUM_INP( i_mateltwise_desc->datatype )) && (LIBXSMM_GEMM_PRECISION_BF16 == LIBXSMM_GETENUM_OUT( i_mateltwise_desc->datatype ))) {
      libxsmm_generator_cvtfp32bf16_avx512_microkernel( io_generated_code, &l_loop_label_tracker, &l_gp_reg_mapping, &l_kernel_config, i_mateltwise_desc );
    } else {
      /* This should not happen  */
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_UNSUP_DATATYPE );
      return;
    }
  } else if (i_mateltwise_desc->operation == LIBXSMM_MELTW_OPERATION_REDUCE) {
    if ( (LIBXSMM_GEMM_PRECISION_F32 == LIBXSMM_GETENUM_INP( i_mateltwise_desc->datatype )) && (LIBXSMM_GEMM_PRECISION_F32 == LIBXSMM_GETENUM_OUT( i_mateltwise_desc->datatype ))) {
      if ((i_mateltwise_desc->flags & LIBXSMM_MELTW_REDUCE_ROWS) > 0) {
        libxsmm_generator_reduce_rows_avx512_microkernel( io_generated_code, &l_loop_label_tracker, &l_gp_reg_mapping, &l_kernel_config, i_mateltwise_desc );
      } else if ((i_mateltwise_desc->flags & LIBXSMM_MELTW_REDUCE_COLS) > 0) {
        libxsmm_generator_reduce_cols_avx512_microkernel( io_generated_code, &l_loop_label_tracker, &l_gp_reg_mapping, &l_kernel_config, i_mateltwise_desc );
      } else {
        /* This should not happen  */
        LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_GENERAL );
        return;
      }
    } else {
      /* This should not happen  */
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_UNSUP_DATATYPE );
      return;
    }
  } else if (i_mateltwise_desc->operation == LIBXSMM_MELTW_OPERATION_SCALE) {
    if ( (LIBXSMM_GEMM_PRECISION_F32 == LIBXSMM_GETENUM_INP( i_mateltwise_desc->datatype )) && (LIBXSMM_GEMM_PRECISION_F32 == LIBXSMM_GETENUM_OUT( i_mateltwise_desc->datatype ))) {
      libxsmm_generator_scale_avx512_microkernel( io_generated_code, &l_loop_label_tracker, &l_gp_reg_mapping, &l_kernel_config, i_mateltwise_desc );
    } else {
      /* This should not happen  */
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_UNSUP_DATATYPE );
      return;
    }
  } else  {
    /* This should not happen  */
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_GENERAL );
    return;
  }

  /* close asm */
  libxsmm_x86_instruction_close_stream_mateltwise( io_generated_code, NULL );
}
