/*
 * lgmath_matrix_vector_functions_common_impl.hpp
 *
 *  Created on: 07.07.2009
 *      Author: andreasvogel
 */

#ifndef __H__LGMATH__MATRIX_VECTOR_FUNCTIONS_COMMON_IMPL__
#define __H__LGMATH__MATRIX_VECTOR_FUNCTIONS_COMMON_IMPL__

#include <cmath>
#include "math_matrix.h"
#include "math_vector.h"

namespace ug
{

/// Matrix - Vector Muliplication
// vOut = m * v
template <typename matrix_t, typename vector_t>
inline
void
MatVecMult(vector_t& vOut, const matrix_t& m, const vector_t& v)
{
	typedef typename matrix_t::size_type size_type;
	for(size_type i = 0; i < vOut.size(); ++i)
	{
		vOut[i] = 0.0;
		for(size_type j = 0; j < v.size(); ++j)
		{
			vOut[i] += m(i,j) * v[j];
		}
	}
}

/// Transposed Matrix - Vector Muliplication
// vOut = Transpose(m) * v
template <typename matrix_t, typename vector_t>
inline
void
TransposedMatVecMult(vector_t& vOut, const matrix_t& m, const vector_t& v)
{
	typedef typename matrix_t::size_type size_type;
	for(size_type i = 0; i < vOut.size(); ++i)
	{
		vOut[i] = 0.0;
		for(size_type j = 0; j < v.size(); ++j)
		{
			vOut[i] += m(j,i) * v[j];
		}
	}
}



}// end of namespace: lgmath

#endif /* __H__LGMATH__LGMATH_MATRIX_VECTOR_FUNCTIONS_COMMON_IMPL__ */
