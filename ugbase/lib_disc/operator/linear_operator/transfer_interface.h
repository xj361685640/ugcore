/*
 * transfer_interface.h
 *
 *  Created on: 20.12.2011
 *      Author: andreasvogel
 */

#ifndef __H__UG__LIB_DISC__OPERATOR__LINEAR_OPERATOR__TRANSFER_INTERFACE__
#define __H__UG__LIB_DISC__OPERATOR__LINEAR_OPERATOR__TRANSFER_INTERFACE__

#include "lib_algebra/operator/interface/operator.h"
#include "lib_disc/dof_manager/grid_level.h"

namespace ug{

//predeclaration
template <typename TAlgebra>
class IConstraint;


///////////////////////////////////////////////////////////////////////////////
// Transfer Operator
///////////////////////////////////////////////////////////////////////////////

/// interface for transfer routines
template <typename TAlgebra>
class ITransferOperator
{
	public:
	///	Vector type
		typedef typename TAlgebra::vector_type vector_type;

	public:
	/// Set Levels for Prolongation coarse -> fine
		virtual void set_levels(GridLevel coarseLevel, GridLevel fineLevel) = 0;

	///	clears dirichlet post processes
		virtual void clear_constraints() = 0;

	///	adds a dirichlet post process (not added if already registered)
		virtual void add_constraint(SmartPtr<IConstraint<TAlgebra> > pp) = 0;

	///	removes a post process
		virtual void remove_constraint(SmartPtr<IConstraint<TAlgebra> > pp) = 0;

	public:
	///	initialize the operator
		virtual void init() = 0;

	/// Prolongates vector, i.e. moves data from coarse to fine level
		virtual void prolongate(vector_type& uFine, const vector_type& uCoarse) = 0;

	/// Restricts vector, i.e. moves data from fine to coarse level
		virtual void restrict(vector_type& uCoarse, const vector_type& uFine) = 0;

	///	Clone
		virtual SmartPtr<ITransferOperator<TAlgebra> > clone() = 0;

	///	virtual destructor
		~ITransferOperator() {}
};

///////////////////////////////////////////////////////////////////////////////
// Transfer Post Process
///////////////////////////////////////////////////////////////////////////////

/// interface for transfer routines
template <typename TAlgebra>
class ITransferPostProcess
{
	public:
	///	Vector type
		typedef typename TAlgebra::vector_type vector_type;

	public:
	/// Set Levels for Post Process
		virtual void set_levels(GridLevel level) = 0;

	///	initialize the operator
		virtual void init() = 0;

	/// apply post process
		virtual void post_process(vector_type& u) = 0;

	///	Clone
		virtual SmartPtr<ITransferPostProcess<TAlgebra> > clone() = 0;

	///	virtual destructor
		~ITransferPostProcess() {}
};

} // end namespace ug

#endif /* __H__UG__LIB_DISC__OPERATOR__LINEAR_OPERATOR__TRANSFER_INTERFACE__ */
