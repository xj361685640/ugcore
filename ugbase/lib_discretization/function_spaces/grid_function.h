/*
 * grid_function_space.h
 *
 *  Created on: 13.06.2010
 *      Author: andreasvogel
 */

#ifndef __H__LIBDISCRETIZATION__FUNCTION_SPACE__GRID_FUNCTION__
#define __H__LIBDISCRETIZATION__FUNCTION_SPACE__GRID_FUNCTION__

namespace ug{

// predeclaration
template <typename TDomain, typename TDoFDistribution, typename TAlgebra>
class ApproximationSpace;


// A grid function brings approximation space and algebra together. For a given DoFManager and level, a grid function
// represents the solutions on the level 'level'
template <typename TDomain, typename TDoFDistribution, typename TAlgebra>
class GridFunction{
	public:
		// this type
		typedef GridFunction<TDomain, TDoFDistribution, TAlgebra> this_type;

		// this type
		typedef ApproximationSpace<TDomain, TDoFDistribution, TAlgebra> approximation_space_type;

		// Domain
		typedef TDomain domain_type;

		// ALGEBRA
		// algebra type
		typedef TAlgebra algebra_type;

		// vector type used to store dof values
		typedef typename algebra_type::vector_type vector_type;

		// local vector type
		typedef LocalVector<typename vector_type::entry_type> local_vector_type;

		// local index type
		typedef LocalIndices local_index_type;

		// multi index
		typedef typename TDoFDistribution::multi_index_vector_type multi_index_vector_type;

		// algebra index
		typedef typename TDoFDistribution::algebra_index_vector_type algebra_index_vector_type;

		// DOF DISTRIBUTION
		// dof manager used for this approximation space
		typedef TDoFDistribution dof_distribution_type;

	public:
		// Default constructor
		GridFunction() :
			m_name(""), m_pApproxSpace(NULL), m_pDoFDistribution(NULL), m_pVector(NULL)
			{}

		// Constructor
		GridFunction(std::string name, approximation_space_type& approxSpace, dof_distribution_type& DoFDistr, bool allocate = true) :
			m_name(name), m_pApproxSpace(&approxSpace), m_pDoFDistribution(&DoFDistr), m_pVector(NULL)
		{
			if(allocate)
				if(!create_storage())
					UG_ASSERT(0, "Cannot create vector memory.\n");
		};

		// copy constructor
		GridFunction(const this_type& v) :
			m_name(v.m_name), m_pApproxSpace(v.m_pApproxSpace),
			m_pDoFDistribution(v.m_pDoFDistribution), m_pVector(NULL)
		{
			assign(v);
		};

		// sets grid function
		this_type& operator=(const this_type& v)
			{assign(v); return *this;}

		// destructor
		virtual ~GridFunction()
		{
			release_storage();
		}

		// clone
		this_type& clone()
		{
			return *(new this_type(*this));
		}

		// copies the GridFunction v, except that the values are copied.
		virtual bool clone_pattern(const this_type& v)
		{
			// delete memory if vector storage exists already
			if(m_pVector != NULL) release_storage();

			// copy informations
			m_name = v.m_name;
			m_pApproxSpace = v.m_pApproxSpace;
			m_pDoFDistribution = v.m_pDoFDistribution;

			// create new vector
			if(!create_storage())
				{UG_LOG("Cannot create pattern.\n"); return false;}

			return true;
		};

		// projects a surface function to this grid function
		// currently this is only implemented for a full refinement
		// (surface level == full refinement level).
		// Then, only the pointer to the dof storage is copied to avoid unnecessary copy-work.
		template <typename TDoFDist>
		bool project_surface(GridFunction<TDomain, TDoFDist, TAlgebra>& v)
		{
			if(m_pDoFDistribution == v.m_pDoFDistribution)
			{
				// set pointer to dof storage
				m_pVector = v.m_pVector;
				return true;
			}
			else{UG_ASSERT(0, "Not implemented.");}
			return false;
		}

		// inverse operation to project surface
		template <typename TDoFDist>
		bool release_surface(GridFunction<TDomain, TDoFDist, TAlgebra>& v)
		{
			if(m_pDoFDistribution == v.m_pDoFDistribution)
			{
				if(m_pVector != v.m_pVector) return false;

				// forget about memory without deleting it.
				m_pVector = NULL;
				return true;
			}
			else
			{UG_ASSERT(0, "Not implemented.");}
			return false;
		}

		/////////////////////////////////
		// help functions
		/////////////////////////////////

	protected:
		// create storage vector. DoFDistrution must be set.
		bool create_storage()
		{
			if(m_pDoFDistribution == NULL)
				{UG_LOG("Cannot create vector without DoFDistribution.\n"); return false;}

			m_pVector = new vector_type;
			if(m_pVector == NULL)
				{UG_LOG("Cannot create new storage vector.(Memory?)\n"); return false;}

			size_t num_dofs = m_pDoFDistribution->num_dofs();
			if(!m_pVector->create(num_dofs)) return false;
			return true;
		}

		// deletes the memory
		bool release_storage()
		{
			if(m_pVector != NULL){m_pVector->destroy(); delete m_pVector;}
			return true;
		}

		// sets the values of GridFunction 'v' to this GridFunction
		// DofManager and level must be the same
		bool assign(const this_type& v)
		{
			// check that approximation space is equal
			if(m_pApproxSpace != v.m_pApproxSpace)
				return false;

			// check that Grid functions are of same type
			if(	m_pDoFDistribution != v.m_pDoFDistribution)
				return false;

			// allocate memory if needed
			if(m_pVector == NULL)
			{
				if(!create_storage())
					{UG_LOG("Cannot create storage for assignment.\n"); return false;}
			}
			else if (v.m_pVector->size() != m_pVector->size())
				{UG_LOG("Size of discrete function does not match."); return false;}

			// copy values
			*m_pVector = *v.m_pVector;
			return true;
		}

		/////////////////////////////////
		// General informations
		/////////////////////////////////
	public:
		// name of grid function
		std::string name()
			{return m_name;}

		// get approximation space
		const approximation_space_type& get_approximation_space() const {return *m_pApproxSpace;}
		approximation_space_type& get_approximation_space() {return *m_pApproxSpace;}

		/////////////////////////////////
		// DoF Distribution requirements
		/////////////////////////////////

		/// number of discrete functions
		size_t num_fct() const {return m_pDoFDistribution->num_fct();}

		/// number of discrete functions on subset si
		size_t num_fct(int si) const {return m_pDoFDistribution->num_fct(si);}

		/// returns the trial space of the discrete function fct
		LocalShapeFunctionSetID local_shape_function_set_id(size_t fct) const  {return m_pDoFDistribution->local_shape_function_set_id(fct);}

		/// returns the name of the discrete function nr_fct
		std::string name(size_t fct) const {return m_pDoFDistribution->name(fct);}

		/// returns the dimension in which solution lives
		int dim(size_t fct) const {return m_pDoFDistribution->dim(fct);}

		/// returns true if the discrete function nr_fct is defined on subset s
		bool is_def_in_subset(size_t fct, int si) const {return m_pDoFDistribution->is_def_in_subset(fct, si);}


		/// number of subsets
		inline int num_subsets() const {return m_pDoFDistribution->num_subsets();}

		/// return the number of dofs distributed
		inline size_t num_dofs() const {return m_pDoFDistribution->num_dofs();}

		/// return the number of dofs distributed on subset si
		inline size_t num_dofs(int si) const {return m_pDoFDistribution->num_dofs(si);}

		// number of elements of this type for a subset
		template <typename TElem>
		inline size_t num() const
			{return m_pDoFDistribution->template num<TElem>();}

		// iterator for elements where this grid function is defined
		template <typename TElem>
		inline typename geometry_traits<TElem>::iterator begin() const
			{return m_pDoFDistribution->template begin<TElem>();}

		// iterator for elements where this grid function is defined
		template <typename TElem>
		inline typename geometry_traits<TElem>::iterator end() const
			{return m_pDoFDistribution->template end<TElem>();}

		// number of elements of this type for a subset
		template <typename TElem>
		inline size_t num(int si) const
			{return m_pDoFDistribution->template num<TElem>(si);}

		// iterator for elements where this grid function is defined
		template <typename TElem>
		inline typename geometry_traits<TElem>::iterator begin(int si) const
			{return m_pDoFDistribution->template begin<TElem>(si);}

		// iterator for elements where this grid function is defined
		template <typename TElem>
		inline typename geometry_traits<TElem>::iterator end(int si) const
			{return m_pDoFDistribution->template end<TElem>(si);}

		////////// Local Algebra ////////////

		/// number of algebra indices on an element
		size_t num_indices(ReferenceObjectID refID, int si, const FunctionGroup& funcGroup) const
			{return m_pDoFDistribution->num_indices(refID, si, funcGroup);}

		/// number of algebra indices on an element
		size_t num_inner_indices(ReferenceObjectID refID, int si, const FunctionGroup& funcGroup) const
			{return m_pDoFDistribution->num_inner_indices(refID, si, funcGroup);}

		/// fill local informations in LocalIndex
		bool prepare_indices(ReferenceObjectID refID, int si, LocalIndices& ind, bool useHanging = false) const
			{return m_pDoFDistribution->prepare_indices(refID, si, ind, useHanging);}

		/// fill local informations in LocalIndex
		bool prepare_inner_indices(ReferenceObjectID refID, int si, LocalIndices& ind) const
			{return m_pDoFDistribution->prepare_inner_indices(refID, si, ind);}

		/// fill the global algebra indices in LocalIndex
		template<typename TElem>
		void update_indices(TElem* elem, LocalIndices& ind, bool useHanging = false) const
			{return m_pDoFDistribution->update_indices(elem, ind, useHanging);}

		/// fill the global algebra indices in LocalIndex
		template<typename TElem>
		void update_inner_indices(TElem* elem, LocalIndices& ind) const
			{return m_pDoFDistribution->update_inner_indices(elem, ind);}

		////////// Multi indices ////////////

		// number of multi indices on an finite element in canonical order
		template <typename TElem>
		inline size_t num_multi_indices(TElem* elem, size_t fct) const
			{return m_pDoFDistribution->num_multi_indices(elem, fct);}

		// number of multi indices on an geometric object in canonical order
		template <typename TGeomObj>
		inline size_t num_inner_multi_indices(TGeomObj* elem, size_t fct) const
			{return m_pDoFDistribution->num_inner_multi_indices(elem, fct);}

		// get multi indices on an finite element in canonical order
		template <typename TElem>
		inline size_t get_multi_indices(TElem* elem, size_t fct, multi_index_vector_type& ind) const
			{return m_pDoFDistribution->get_multi_indices(elem, fct, ind);}

		// get multi indices on an geometric object in canonical order
		template <typename TGeomObj>
		inline size_t get_inner_multi_indices(TGeomObj* elem, size_t fct,	multi_index_vector_type& ind) const
			{return m_pDoFDistribution->get_inner_multi_indices(elem, fct, ind);}

		////////// Algebra indices ////////////

		// number of algebra indices on an geometric object in canonical order
		template <typename TGeomObj>
		inline size_t num_algebra_indices(TGeomObj* elem, size_t fct) const
			{return m_pDoFDistribution->num_algebra_indices(elem, fct);}

		// number of algebra indices on an geometric object in canonical order
		template <typename TGeomObj>
		inline size_t num_inner_algebra_indices(TGeomObj* elem, size_t fct) const
			{return m_pDoFDistribution->num_inner_algebra_indices(elem, fct);}

		// get algebra indices on an geometric object in canonical order
		template <typename TGeomObj>
		inline void get_algebra_indices(TGeomObj* elem, algebra_index_vector_type& ind) const
			{m_pDoFDistribution->get_algebra_indices(elem, ind);}

		// get algebra indices on an geometric object in canonical order
		template <typename TGeomObj>
		inline void get_inner_algebra_indices(TGeomObj* elem, algebra_index_vector_type& ind) const
			{m_pDoFDistribution->get_inner_algebra_indices(elem, ind);}

		////////// DoF Values ////////////

		// get dof values
		inline number get_dof_value(size_t i, size_t comp) const
			{return BlockRef(((*m_pVector)[i]), comp);}

		////////////////////////////
		// Algebra requirements
		////////////////////////////

		// export the dof storage of this vector
		vector_type& get_vector()
			{return *m_pVector;}

		// export the dof storage of this vector
		const vector_type& get_vector() const
			{return *m_pVector;}

		// this function finalizes the dof pattern.
		// Afterwards the pattern can only be changed by
		// destroying the vector and creating a new one.
		bool finalize()
			{return m_pVector->finalize();}

		number dotprod(const this_type& v)
			{return m_pVector->dotprod(*v.m_pVector);}

		// add a grid function to this grid function
		this_type& operator+=(const this_type& v)
			{*m_pVector += *(v.m_pVector); return *this;}

		// subtract a grid function from this grid function
		this_type& operator-=(const this_type& v)
			{*m_pVector -= *(v.m_pVector); return *this;}

		// multiply grid function by scalar
		this_type& operator*=(number w)
			{*m_pVector *= w; return *this;}

		// set all dofs on level 'level' to value 'w'
		bool set(number w)
			{return m_pVector->set(w);}

		// two norm
		inline number two_norm()
			{return m_pVector->two_norm();}

	protected:
		// name
		std::string m_name;

		// Approximation Space
		approximation_space_type* m_pApproxSpace;

		// dof manager of this discrete function
		dof_distribution_type* m_pDoFDistribution;

		// vector storage, to store values of local degrees of freedom
		vector_type* m_pVector;
};

template <typename TDomain, typename TDoFDistribution, typename TAlgebra>
inline std::ostream& operator<< (std::ostream& outStream, const GridFunction<TDomain, TDoFDistribution, TAlgebra>& v)
{
	outStream << v.get_vector();
	return outStream;
}

} // end namespace ug

#endif /* __H__LIBDISCRETIZATION__FUNCTION_SPACE__GRID_FUNCTION__ */
