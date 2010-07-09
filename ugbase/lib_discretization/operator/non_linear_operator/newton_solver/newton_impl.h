/*
 * newton_impl.h
 *
 *  Created on: 18.06.2010
 *      Author: andreasvogel
 */

#ifndef __H__LIBDISCRETIZATION__OPERATOR__NON_LINEAR_OPERATOR__NEWTON_SOLVER__NEWTON_IMPL__
#define __H__LIBDISCRETIZATION__OPERATOR__NON_LINEAR_OPERATOR__NEWTON_SOLVER__NEWTON_IMPL__

#include "newton.h"
#include "lib_discretization/io/vtkoutput.h"

namespace ug{

template <typename TDiscreteFunction>
bool
NewtonSolver<TDiscreteFunction>::
init(IOperator<discrete_function_type, discrete_function_type>& N)
{
	m_N = dynamic_cast<AssembledOperator<discrete_function_type>* >(&N);
	if(m_N == NULL)
	{
		UG_LOG("NewtonSolver currently only works for AssembledDiscreteOperator.\n");
		return false;
	}
	m_ass = m_N->get_assemble();
	return true;
}


template <typename TDiscreteFunction>
bool
NewtonSolver<TDiscreteFunction>::
allocate_memory(const discrete_function_type& u)
{
	// Jacobian
	m_J = new AssembledLinearizedOperator<discrete_function_type>(*m_ass);

	// defect
	m_d = new discrete_function_type;
	m_d->clone_pattern(u);

	// correction
	m_c = new discrete_function_type;
	m_c->clone_pattern(u);

	if(m_d == NULL || m_c == NULL || m_J == NULL)
	{
		UG_ASSERT(0, "Cannot allocate memory.");
		return false;
	}

	m_allocated = true;
	return true;
}

template <typename TDiscreteFunction>
bool
NewtonSolver<TDiscreteFunction>::
deallocate_memory()
{
	if(m_allocated)
	{
		delete m_d; delete m_c;
		delete m_J;
	}
	m_allocated = false;
	return true;
}


template <typename TDiscreteFunction>
bool
NewtonSolver<TDiscreteFunction>::
prepare(discrete_function_type& u)
{
	if(!m_allocated)
	{
		if(allocate_memory(u) != true)
		{
			UG_LOG("NewtonSolver: Cannot allocate memory.\n");
			return false;
		}
	}

	return true;
}


template <typename TDiscreteFunction>
NewtonSolver<TDiscreteFunction>::
~NewtonSolver()
{
	if(m_allocated)
	{
		if(!deallocate_memory())
			UG_ASSERT(0, "Cannot deallocate memory");
	}
}


template <typename TDiscreteFunction>
bool
NewtonSolver<TDiscreteFunction>::
apply(discrete_function_type& u)
{
	number norm, norm_old, norm_start;

	// Set Dirichlet - Nodes to exact values
	if(m_ass->assemble_solution(u) != IAssemble_OK)
		{UG_LOG("NewtonSolver::apply: Cannot set dirichlet values in solution.\n"); return false;}

	// Compute first Defect
	if(m_N->prepare(u, *m_d) != true)
		{UG_LOG("NewtonSolver::apply: Cannot prepare Non-linear Operator.\n"); return false;}
	if(m_N->apply(u, *m_d) != true)
		{UG_LOG("NewtonSolver::apply: Cannot apply Non-linear Operator to compute start defect.\n"); return false;}

	// Compute first Residuum
	norm = norm_old = norm_start = m_d->two_norm();

	//verbose
	UG_LOG(" ###### NEWTON - METHOD ######" << std::endl);
	UG_LOG(" ##   Iter     Defect         Rate \n");
	UG_LOG(" ## " << std::setw(4) << 0 << ":  " << std::scientific << norm_old <<  "      -------" << std::endl);

	discrete_function_type s;
	s.clone_pattern(u);

	//loop iteration
	int i;
	for(i = 1; ; ++i)
	{
		// check that defect is a still a valid number
		if(!is_valid_number(norm))
		{
			UG_LOG(" ##### Defect " << norm << " is not a valid number. Newton Solver did NOT CONVERGE. #####\n\n");
			if(m_reallocate){deallocate_memory();}
			return false;
		}

		// check if defect is small enough (absolute)
		if(norm < m_absTol)
		{
			UG_LOG(" ##### Absolute defect " << m_absTol << " reached. Newton Solver converged. #####\n\n");
			if(m_reallocate){deallocate_memory();}
			return true;
		}

		// check if defect is small enough (relative)
		if(norm/norm_start < m_relTol)
		{
			UG_LOG(" ##### Relative defect " << m_relTol << " reached. Newton Solver converged. #####\n\n");
			if(m_reallocate){deallocate_memory();}
			return true;
		}

		// check that maximum number of iterations is not reached
		if(i > m_MaxIterations)
		{
			UG_LOG(" ##### Absolute defect " << m_absTol << " and relative defect " << m_relTol << " NOT reached after " << m_MaxIterations << " Iterations. #####\n");
			UG_LOG(" ##### Iterative Newton Solver did NOT CONVERGE. #####\n\n");
			if(m_reallocate){deallocate_memory();}
			return false;
		}

		// COMPUTE next iterate
		// set c = 0
		if(m_c->set(0.0) != true)
			{UG_LOG("NewtonSolver::apply: Cannot reset correction to zero.\n"); return false;}

		// Compute Jacobian
		if(m_J->prepare(u, *m_c, *m_d) != true)
			{UG_LOG("NewtonSolver::apply: Cannot prepare Jacobi Operator.\n"); return false;}

		// Init Jacobi Inverse
		if(m_LinearSolver.init(*m_J) != true)
			{UG_LOG("NewtonSolver::apply: Cannot init Inverse Linear Operator for Jacobi-Operator.\n"); return false;}
		if(m_LinearSolver.prepare(u, *m_d, *m_c) != true)
			{UG_LOG("NewtonSolver::apply: Cannot prepare Inverse Linear Operator for Jacobi-Operator.\n"); return false;}

		// Solve Linearized System
		if(m_LinearSolver.apply(*m_d, *m_c) != true)
			{UG_LOG("NewtonSolver::apply: Cannot apply Inverse Linear Operator for Jacobi-Operator.\n"); return false;}


		VTKOutput<discrete_function_type> out;
		//out.print("correction_bcgs", *m_c, i, i);


		////////////////
		// Line Search
		////////////////

		number lambda = m_lambda_start;
		number alpha = 0.25;
		s = u;
		UG_LOG(" # ++ Begin Line Search (lambda_start = " << lambda << ", alpha = " << alpha << ")\n");
		for(int k = 1; k <= m_maxLineSearch; ++k)
		{
			// try on line
			VecScaleAdd(u, *m_c, (-1)*lambda);

			// compute new Defect
			if(m_N->prepare(u, *m_d) != true)
				{UG_LOG("NewtonSolver::apply: Cannot prepare Non-linear Operator for defect computation.\n"); return false;}
			if(m_N->apply(u, *m_d) != true)
				{UG_LOG("NewtonSolver::apply: Cannot apply Non-linear Operator to compute defect after step " << i << ".\n"); return false;}

			//compute new Residuum
			norm = m_d->two_norm();

			// compute reduction
			number rho = norm/norm_old;

			// print rate
			UG_LOG(" # +  s = " << k << ",  lambda = " << lambda << ", norm = " << norm << ", rho = " << rho <<"\n");

			// check if reduction fits
			if(rho <= 1 - alpha * fabs(lambda)) break;
			else lambda *= m_lambda_reduce;

			if(k == m_maxLineSearch)
				{UG_LOG(" # ++ Line Search did not converge. Newton Solver did not converge.\n"); return false;}

			// reset u
			u = s;
		}

		//out.print("u_bcgs", u, i, i);

		// print convergence rate
		UG_LOG(" # ++ Line Search converged.\n");
		UG_LOG(" ## " << std::setw(4) << i << ":  " << std::scientific << norm << "    " << norm/norm_old << std::endl);

		// remember current norm
		norm_old = norm;

	}
	UG_ASSERT(0, "Should never pass this line.");
	return false;
}

}

#endif /* __H__LIBDISCRETIZATION__OPERATOR__NON_LINEAR_OPERATOR__NEWTON_SOLVER__NEWTON_IMPL__ */

