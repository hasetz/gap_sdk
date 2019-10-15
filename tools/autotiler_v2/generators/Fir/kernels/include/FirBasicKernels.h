/* 
 * Copyright (C) 2017 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */

#ifndef __FIRPAR_H__
#define __FIRPAR_H__
#include "Gap8.h"

/** @addtogroup groupFIR
@{ */

/** @defgroup FIRBasicK FIRBasicKernels

@{ */

/** @brief Template for FIR basic kernel group of basic kernels

Template for FIR basic kernel group of basic kernels
*/
typedef struct {
        short int *In;		/**< Pointer to a 1D tile of fixed point samples */
        short int *NextIn;	/**< Pointer to a delay line of size NCoeffs-1, it is placed right before In */
        short int *Coeffs;	/**< Pointer to NCoeffs fixed point FIR filter coefficients */
        short int *Out;		/**< Pointer to filtered In */
        unsigned int NSamples;	/**< Total number of samples */
        unsigned int NCoeffs;	/**< Number of Taps (fir filter coefficients) */
        unsigned int Norm;	/**< Normalization factor for fixed point operations */
} KerFirParallel_ArgT;

/** @brief Generic Parallel vectorial FIR filter, Any number of Taps

Generic Parallel scalar FIR filter, Any number of Taps.

Samples and coefficient are 16bits quantities represented as fixed point numbers.
Tap's number is assumed to be even. Since this implementation takes benefit of SIMD2 support, in case the number
of taps is odd you should simply add a 0 at the end of the coefficient's list to make it even.

This implementation is thought to be used in conjuction with Gap8's AutoTiler, it operates on on tile of inputs assumed to be located
in shared L1. This tile comes from L2 memory through DMA transfers generated by the AutoTiler.
The ith output (Out) of a FIR filter is a function of the current input sample (In) and of the NCoeffs-1 previous input. To capture this dependency
this kernel manages a delay line (NextIn) physically allocated just bellow input samples In. When the kernel is done with the current tile
the first NCoeffs-1 inputs in In are copied into NextIn.

Samples by coeffs sum of products are evaluated by groups of 2 using Gap8's sum of dot products on vector of 2 short int elements.
Since 2 pairs of samples/coeffs are evaluated in a row the number of iteration is divided by 2 from NCoeffs to NCoeffs/2.
When the NCoeffs/2 pairs of sum of products have been evaluated we first normalized and round the results to Norm fractional bits and clip
the result to 15 bits (Out is signed short) before storing the result into Out.

Parallelization is performed spliting the NSamples Samples contains into indepedant buckets, bucket size is NSamples / Number of cores.
Each of the available core is given a bucket. A synchronization barrier is used to make sure all cores are done before moving to delay
line management. Delay line copy itself is handled by the core 0 of the cluster, note that the copy could have been parallelized.
The kernels ends with another synchronization barrier just to be sure we don't return to master core with some cores still evaluating FIR outputs.
*/
void KerFirParallelNTaps      (KerFirParallel_ArgT *Arg);

/** @brief Specialized Parallel vectorial FIR filter, 20 Taps

A specialized implementation of a 20 taps FIR filter. The general implementation
pattern is explained at KerFirParallelNTaps().

Here we unroll the inner loop (evaluation of one FIR output), move all FIR filter coefficients to
separate variables, they will be promoted to registers by the compiler saving shared L1 memory
accesses.
*/
void KerFirParallel20Taps     (KerFirParallel_ArgT *Arg);

/** @brief Specialized Parallel vectorial FIR filter, 10 Taps

A specialized implementation of a 10 taps FIR filter. The general implementation
pattern is explained at KerFirParallelNTaps().

Here we unroll the inner loop (evaluation of one FIR output), move all FIR filter coefficients to
separate variables, they will be promoted to registers by the compiler saving shared L1 memory
accesses.

Since number of taps is relatively low we could also consider managing input samples in a register based delay line
where at each iteration on i samples are moved in the group of registers used for the delay line by one position while
the last element of the delay line receives one fresh data. Performances would be identical by memory traffic to shared L1
would be further reduced with a positive power consumption impact.

*/
void KerFirParallel10Taps     (KerFirParallel_ArgT *Arg);


/** @brief Generic Parallel scalar FIR filter, Any number of Taps

A generic Ntaps fixed point FIR filter.

This is a pure scalar implementatio, it's main goal if to illustrate the gain of the vectorized implementation of KerFirParallelNTaps().
*/
void KerFirParallelScalarNTaps(KerFirParallel_ArgT *Arg);

/** @} */ // End of FIRBasicKernels
/** @} */

#endif

