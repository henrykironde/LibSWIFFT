/*
 * Copyright (C) 2020 Yaron Gvili and Gvili Tech Ltd.
 *
 * See the accompanying LICENSE.txt file for licensing information.
 */
/*! \file src/swifft.c
 * \brief LibSWIFFT public C implementation
 *
 * Implementation using the best instruction set available at build time.
 */

#include "libswifft/swifft.h"
#include "libswifft/swifft_avx.h"
#include "libswifft/swifft_avx2.h"
#include "libswifft/swifft_avx512.h"

#undef SWIFFT_ISET
#define SWIFFT_ISET() SWIFFT_INSTRUCTION_SET
#include "swifft_ops.inl"


LIBSWIFFT_BEGIN_EXTERN_C

SWIFFT_ALIGN const BitSequence SWIFFT_sign0[SWIFFT_INPUT_BLOCK_SIZE] = {0};

void SWIFFT_fft(const BitSequence * LIBSWIFFT_RESTRICT input, const BitSequence * LIBSWIFFT_RESTRICT sign, int m, int16_t * LIBSWIFFT_RESTRICT fftout)
{
	SWIFFT_ISET_NAME(SWIFFT_fft_)(input, sign, m, fftout);
}

void SWIFFT_fftsum(const int16_t * LIBSWIFFT_RESTRICT ikey,
	const int16_t * LIBSWIFFT_RESTRICT ifftout, int m, int16_t * LIBSWIFFT_RESTRICT iout)
{
	SWIFFT_ISET_NAME(SWIFFT_fftsum_)(ikey, ifftout, m, iout);
}

//! \brief Converts from base-257 to base-256.
//!
//! vals array is assumed to have (SWIFFT_W=8) SWIFFT_N=8 digits in base 257.
//! Assume that most significiant is last.
//! output in vals is the same 8 numbers encoded in base 256.
//!
//! \param[in,out] vals the vals array.
static void ToBase256(Z1vec vals[8])
{
	Z1vec Z1_255 = Z1CONST(255), Z1_8 = Z1CONST(8);
	int i;
	for (i=8-1; i>0; i--){
		int j;
		for (j=i-1; j<8-1; j++) {
			Z1vec v = vals[j]+vals[j+1];
			vals[j] = v & Z1_255;
			vals[j+1] += (v >> Z1_8);
		}
	}
}

//! \brief Compacts a hash value of SWIFFT.
//! The result is not composable with other compacted hash values.
//!
//! \param[in] output the hash value of SWIFFT, of size 128 bytes (1024 bit).
//! \param[out] compact the compacted hash value of SWIFFT, of size 64 bytes (512 bit).
void SWIFFT_Compact(const BitSequence output[SWIFFT_OUTPUT_BLOCK_SIZE],
	BitSequence compact[SWIFFT_COMPACT_BLOCK_SIZE])
{
	//
	// The 8*8 output int16_ts needs to be transposed before and after
	// SIMD base change.
	// This could be avoided by defining the base change differently
	// but then a transpose like operation would have to be performed
	// by the normal (Non-SIMD) version.
	//
	Z1vec transposed[SWIFFT_N/SWIFFT_W];
	int16_t *tin = (int16_t *) output;
	int16_t *tout = (int16_t *) transposed;
	int i;
	for (i=0; i<SWIFFT_N/SWIFFT_W; i++,tin+=8,tout++) {
		tout[0] = tin[0];
		tout[8] = tin[1];
		tout[16] = tin[2];
		tout[24] = tin[3];
		tout[32] = tin[4];
		tout[40] = tin[5];
		tout[48] = tin[6];
		tout[56] = tin[7];
	}
	ToBase256(transposed);
	tin = (int16_t *) transposed;
	BitSequence *cout = compact;
	int carry = 0;
	for (i=0; i<SWIFFT_N/SWIFFT_W; i++,tin++,cout+=8) {
		cout[0] = tin[0];
		cout[1] = tin[8];
		cout[2] = tin[16];
		cout[3] = tin[24];
		cout[4] = tin[32];
		cout[5] = tin[40];
		cout[6] = tin[48];
		cout[7] = tin[56]&255;
		carry |= ((tin[56]>>8)<<i);
	}
	// ignore carry
}

//! \brief Sets a constant value at each SWIFFT hash value element.
//!
//! \param[out] output the hash value of SWIFFT to modify.
//! \param[in] operand the constant value to set.
void SWIFFT_ConstSet(BitSequence output[SWIFFT_OUTPUT_BLOCK_SIZE],
	const int16_t operand)
{
	SWIFFT_ISET_NAME(SWIFFT_ConstSet_)(output, operand);
}

//! \brief Adds a constant value to each SWIFFT hash value element.
//!
//! \param[in,out] output the hash value of SWIFFT to modify.
//! \param[in] operand the constant value to add.
void SWIFFT_ConstAdd(BitSequence output[SWIFFT_OUTPUT_BLOCK_SIZE],
	const int16_t operand)
{
	SWIFFT_ISET_NAME(SWIFFT_ConstAdd_)(output, operand);
}

//! \brief Subtracts a constant value from each SWIFFT hash value element.
//!
//! \param[in,out] output the hash value of SWIFFT to modify.
//! \param[in] operand the constant value to subtract.
void SWIFFT_ConstSub(BitSequence output[SWIFFT_OUTPUT_BLOCK_SIZE],
	const int16_t operand)
{
	SWIFFT_ISET_NAME(SWIFFT_ConstSub_)(output, operand);
}

//! \brief Multiply a constant value into each SWIFFT hash value element.
//!
//! \param[in,out] output the hash value of SWIFFT to modify.
//! \param[in] operand the constant value to multiply by.
void SWIFFT_ConstMul(BitSequence output[SWIFFT_OUTPUT_BLOCK_SIZE],
	const int16_t operand)
{
	SWIFFT_ISET_NAME(SWIFFT_ConstMul_)(output, operand);
}

//! \brief Adds a SWIFFT hash value to another, element-wise.
//!
//! \param[in,out] output the hash value of SWIFFT to modify.
//! \param[in] operand the hash value to add.
void SWIFFT_Add(BitSequence output[SWIFFT_OUTPUT_BLOCK_SIZE],
	const BitSequence operand[SWIFFT_OUTPUT_BLOCK_SIZE])
{
	SWIFFT_ISET_NAME(SWIFFT_Add_)(output, operand);
}

//! \brief Subtracts a SWIFFT hash value from another, element-wise.
//!
//! \param[in,out] output the hash value of SWIFFT to modify.
//! \param[in] operand the hash value to subtract.
void SWIFFT_Sub(BitSequence output[SWIFFT_OUTPUT_BLOCK_SIZE],
	const BitSequence operand[SWIFFT_OUTPUT_BLOCK_SIZE])
{
	SWIFFT_ISET_NAME(SWIFFT_Sub_)(output, operand);
}

//! \brief Multiplies a SWIFFT hash value from another, element-wise.
//!
//! \param[in,out] output the hash value of SWIFFT to modify.
//! \param[in] operand the hash value to multiply by.
void SWIFFT_Mul(BitSequence output[SWIFFT_OUTPUT_BLOCK_SIZE],
	const BitSequence operand[SWIFFT_OUTPUT_BLOCK_SIZE])
{
	SWIFFT_ISET_NAME(SWIFFT_Mul_)(output, operand);
}

//! \brief Computes the result of a SWIFFT operation.
//! The result is composable with other hash values.
//!
//! \param[in] input the input of 256 bytes (2048 bit).
//! \param[out] output the resulting hash value of SWIFFT, of size 128 bytes (1024 bit).
void SWIFFT_Compute(const BitSequence input[SWIFFT_INPUT_BLOCK_SIZE],
	BitSequence output[SWIFFT_OUTPUT_BLOCK_SIZE])
{
	SWIFFT_ISET_NAME(SWIFFT_Compute_)(input, output);
}

//! \brief Computes the result of a SWIFFT operation.
//! The result is composable with other hash values.
//!
//! \param[in] input the input of 256 bytes (2048 bit).
//! \param[in] sign the sign bits corresponding to the input of 256 bytes (2048 bit).
//! \param[out] output the resulting hash value of SWIFFT, of size 128 bytes (1024 bit).
void SWIFFT_ComputeSigned(const BitSequence input[SWIFFT_INPUT_BLOCK_SIZE],
	const BitSequence sign[SWIFFT_INPUT_BLOCK_SIZE],
	BitSequence output[SWIFFT_OUTPUT_BLOCK_SIZE])
{
	SWIFFT_ISET_NAME(SWIFFT_ComputeSigned_)(input, sign, output);
}

//! \brief Computes the result of multiple SWIFFT operations.
//! The result is composable with other hash values.
//!
//! \param[in] nblocks the number of blocks to operate on.
//! \param[in] input the blocks of input, each of 256 bytes (2048 bit).
//! \param[out] output the resulting blocks of hash values of SWIFFT, each of size 128 bytes (1024 bit).
void SWIFFT_ComputeMultiple(int nblocks, const BitSequence * input, BitSequence * output)
{
	SWIFFT_ISET_NAME(SWIFFT_ComputeMultiple_)(nblocks, input, output);
}

//! \brief Computes the result of multiple SWIFFT operations.
//! The result is composable with other hash values.
//!
//! \param[in] nblocks the number of blocks to operate on.
//! \param[in] input the blocks of input, each of 256 bytes (2048 bit).
//! \param[in] sign the blocks of sign bits corresponding to blocks of input of 256 bytes (2048 bit).
//! \param[out] output the resulting blocks of hash values of SWIFFT, each of size 128 bytes (1024 bit).
void SWIFFT_ComputeMultipleSigned(int nblocks, const BitSequence * input,
	const BitSequence * sign, BitSequence * output)
{
	SWIFFT_ISET_NAME(SWIFFT_ComputeMultipleSigned_)(nblocks, input, sign, output);
}

LIBSWIFFT_END_EXTERN_C
