/*
 * grid_function_impl.h
 *
 *  Created on: 13.06.2010
 *      Author: andreasvogel
 */

#ifndef __H__UG__LIB_DISC__FUNCTION_SPACE__GRID_FUNCTION_IMPL__
#define __H__UG__LIB_DISC__FUNCTION_SPACE__GRID_FUNCTION_IMPL__

#include "grid_function.h"

#include "lib_algebra/algebra_type.h"
#include "lib_disc/local_finite_element/local_shape_function_set.h"
#include "lib_disc/local_finite_element/local_dof_set.h"
#include "lib_disc/reference_element/reference_mapping.h"
#include "lib_disc/domain_util.h"
#include "lib_disc/domain_traits.h"

#ifdef UG_PARALLEL
	#include "pcl/pcl.h"
	#include "lib_algebra/parallelization/parallelization.h"
#endif

namespace ug{

////////////////////////////////////////////////////////////////////////////////
//	DoFPositions
////////////////////////////////////////////////////////////////////////////////

template <typename TElem, typename TDomain>
bool
InnerDoFPosition(std::vector<MathVector<TDomain::dim> >& vPos,
                 TElem* elem, TDomain& domain, LFEID lfeID)
{
//	reference element
	typedef typename reference_element_traits<TElem>::reference_element_type
			reference_element_type;

//	reference element dimension
	static const int refDim = reference_element_type::dim;

//	physical world dimension
	static const int dim = TDomain::dim;

//	reference object id
	static const ReferenceObjectID roid = reference_element_type::REFERENCE_OBJECT_ID;

//	vector for the vertex positions
	std::vector<MathVector<dim> > vVertPos(reference_element_type::numCorners);

//	get the vertices
	CollectCornerCoordinates(vVertPos, *elem, domain, true);

//	create a reference mapping
	ReferenceMapping<reference_element_type, dim> map(&(vVertPos[0]));

//	\TODO: This is a lousy quick-hack. Remove, when dof pos on lower dim elemens
	//		can be handeled correctly for non-lagrangian spaces.
	if(lfeID == LFEID(LFEID::CROUZEIX_RAVIART, dim, 1))
	{
		vPos.clear();
		if(lfeID.dim() != refDim+1) return true;

		MathVector<dim> center;
		VecSet(center, 0.0);
		for(size_t co = 0; co < vVertPos.size(); ++co)
			VecAppend(center, vVertPos[co]);
		VecScale(center, center, 1./(vVertPos.size()));

		vPos.push_back(center);
		return true;
	}
	if(lfeID == LFEID(LFEID::PIECEWISE_CONSTANT, dim, 0))
	{
		vPos.clear();
		if(lfeID.dim() != refDim) return true;

		MathVector<dim> center;
		VecSet(center, 0.0);
		for(size_t co = 0; co < vVertPos.size(); ++co)
			VecAppend(center, vVertPos[co]);
		VecScale(center, center, 1./(vVertPos.size()));

		vPos.push_back(center);
		return true;
	}
	if(lfeID == LFEID(LFEID::NEDELEC, dim, 1))
	{
		vPos.clear();
		if(refDim != 1) return true;

		MathVector<dim> center;
		VecSet(center, 0.0);
		for(size_t co = 0; co < vVertPos.size(); ++co)
			VecAppend(center, vVertPos[co]);
		VecScale(center, center, 1./(vVertPos.size()));

		vPos.push_back(center);
		return true;
	}

//	get local shape function set
	const LocalShapeFunctionSet<refDim>& lsfs
		= LocalShapeFunctionSetProvider::get<refDim>(roid, lfeID);

//	get local dof set
	const LocalDoFSet& lds = LocalDoFSetProvider::get(roid, lfeID);

//	typedef local position type
	typedef typename LocalShapeFunctionSet<refDim>::position_type
		local_pos_type;

//	clear pos
	vPos.clear();

//	bool flag if position is exact, or no exact position available for shapes
	bool bExact = true;

//	loop all shape functions
	for(size_t sh = 0; sh < lsfs.num_sh(); ++sh)
	{
	//	check if dof in interior
		if(lds.local_dof(sh).dim() != refDim) continue;

	//	get local position
		local_pos_type locPos;
		bExact &= lsfs.position(sh, locPos);

	//	map to global position
		MathVector<dim> globPos;
		map.local_to_global(globPos, locPos);

	//	add
		vPos.push_back(globPos);
	}

//	return if positions are given exactly
	return bExact;
};

template <typename TDomain>
bool
DoFPositionOnVertex(std::vector<MathVector<TDomain::dim> >& vPos, VertexBase* elem, TDomain& domain, LFEID lfeID)
{
//\todo: handle finite element spaces with more than on DoF in Vertex

//	get the vertices
	CollectCornerCoordinates(vPos, *elem, domain, true);

//	return if positions are given exactly
	return true;
};

template <typename TDomain>
bool InnerDoFPosition(std::vector<MathVector<TDomain::dim> >& vPos, GeometricObject* elem, TDomain& domain, LFEID lfeID)
{
	switch(elem->base_object_id())
	{
		case VERTEX: return InnerDoFPosition(vPos, static_cast<VertexBase*>(elem), domain, lfeID);
		case EDGE:   return InnerDoFPosition(vPos, static_cast<EdgeBase*>(elem), domain, lfeID);
		case FACE:   return InnerDoFPosition(vPos, static_cast<Face*>(elem), domain, lfeID);
		case VOLUME: return InnerDoFPosition(vPos, static_cast<Volume*>(elem), domain, lfeID);
	}
	UG_THROW("Base Object type not found.");
}

template <typename TDomain>
bool InnerDoFPosition(std::vector<MathVector<TDomain::dim> >& vPos, VertexBase* elem, TDomain& domain, LFEID lfeID)
{
	return  DoFPositionOnVertex(vPos, elem, domain, lfeID);
}

template <typename TDomain>
bool InnerDoFPosition(std::vector<MathVector<TDomain::dim> >& vPos, EdgeBase* elem, TDomain& domain, LFEID lfeID)
{
	switch(elem->container_section())
	{
		case CSEDGE_EDGE: 			   return InnerDoFPosition(vPos, static_cast<Edge*>(elem), domain, lfeID);
		case CSEDGE_CONSTRAINED_EDGE: return InnerDoFPosition(vPos, static_cast<ConstrainedEdge*>(elem), domain, lfeID);
		case CSEDGE_CONSTRAINING_EDGE:return InnerDoFPosition(vPos, static_cast<ConstrainingEdge*>(elem), domain, lfeID);
	}
	UG_THROW("Edge type not found.");
}

template <typename TDomain>
bool InnerDoFPosition(std::vector<MathVector<TDomain::dim> >& vPos, Face* elem, TDomain& domain, LFEID lfeID)
{
	switch(elem->container_section())
	{
		case CSFACE_TRIANGLE: return InnerDoFPosition(vPos, static_cast<Triangle*>(elem), domain, lfeID);
		case CSFACE_CONSTRAINED_TRIANGLE: return InnerDoFPosition(vPos, static_cast<ConstrainedTriangle*>(elem), domain, lfeID);
		case CSFACE_CONSTRAINING_TRIANGLE: return InnerDoFPosition(vPos, static_cast<ConstrainingTriangle*>(elem), domain, lfeID);
		case CSFACE_QUADRILATERAL: return InnerDoFPosition(vPos, static_cast<Quadrilateral*>(elem), domain, lfeID);
		case CSFACE_CONSTRAINED_QUADRILATERAL: return InnerDoFPosition(vPos, static_cast<ConstrainedQuadrilateral*>(elem), domain, lfeID);
		case CSFACE_CONSTRAINING_QUADRILATERAL: return InnerDoFPosition(vPos, static_cast<ConstrainingQuadrilateral*>(elem), domain, lfeID);
	}
	UG_THROW("Face type not found.");
}

template <typename TDomain>
bool InnerDoFPosition(std::vector<MathVector<TDomain::dim> >& vPos, Volume* elem, TDomain& domain, LFEID lfeID)
{
	switch(elem->container_section())
	{
		case CSVOL_TETRAHEDRON: return InnerDoFPosition(vPos, static_cast<Tetrahedron*>(elem), domain, lfeID);
		case CSVOL_PYRAMID: return InnerDoFPosition(vPos, static_cast<Pyramid*>(elem), domain, lfeID);
		case CSVOL_PRISM: return InnerDoFPosition(vPos, static_cast<Prism*>(elem), domain, lfeID);
		case CSVOL_HEXAHEDRON: return InnerDoFPosition(vPos, static_cast<Hexahedron*>(elem), domain, lfeID);
	}
	UG_THROW("Volume type not found.");
}




template <typename TElem, typename TDomain>
bool DoFPosition(std::vector<MathVector<TDomain::dim> >& vPos, TElem* elem, TDomain& domain, LFEID lfeID)
{
//	reference element
	typedef typename reference_element_traits<TElem>::reference_element_type
			reference_element_type;

//	physical world dimension
	static const int dim = TDomain::dim;

//	reference element dimension
	static const int refDim = reference_element_type::dim;

//	reference object id
	static const ReferenceObjectID roid = reference_element_type::REFERENCE_OBJECT_ID;

//	vector for the vertex positions
	std::vector<MathVector<dim> > vVertPos(reference_element_type::numCorners);

//	get the vertices
	CollectCornerCoordinates(vVertPos, *elem, domain, true);

//	\TODO: This is a lousy quick-hack. Remove, when dof pos on lower dim elemens
	//		can be handeled correctly for non-lagrangian spaces.
	if(lfeID.type() == LFEID::CROUZEIX_RAVIART)
	{
		vPos.clear();

		// case, that we are on a side
		if(lfeID.dim() == refDim+1)
		{
			MathVector<dim> center;
			VecSet(center, 0.0);
			for(size_t co = 0; co < vVertPos.size(); ++co)
				VecAppend(center, vVertPos[co]);
			VecScale(center, center, 1./(vVertPos.size()));
			vPos.push_back(center);
			return true;
		}

		// case, that we are on elements with lower dim, than a side
		if(lfeID.dim() > refDim+1) return true;

		// case, that we are on element with higher dim than side (i.e. the volume element itself)
		if(lfeID.dim() == refDim && lfeID.dim() == dim)
		{
			typedef typename domain_traits<dim-1>::geometric_base_object Side;
			std::vector<Side*> vSide;

			CollectAssociated(vSide, *const_cast<MultiGrid*>(&(*domain.grid())), elem);

			for(size_t i = 0; i < vSide.size(); ++i)
			{
				CollectCornerCoordinates(vVertPos, *vSide[i], domain, true);

				MathVector<dim> center;
				VecSet(center, 0.0);
				for(size_t co = 0; co < vVertPos.size(); ++co)
					VecAppend(center, vVertPos[co]);
				VecScale(center, center, 1./(vVertPos.size()));
				vPos.push_back(center);
			}
			return true;
		}

		// other cases should never happen
		UG_THROW("Special case for Crouzeix-Raviart: case should not happen.");
	}
	if(lfeID.type() == LFEID::PIECEWISE_CONSTANT)
	{
		vPos.clear();
		if(lfeID.dim() != refDim) return true;

		MathVector<dim> center;
		VecSet(center, 0.0);
		for(size_t co = 0; co < vVertPos.size(); ++co)
			VecAppend(center, vVertPos[co]);
		VecScale(center, center, 1./(vVertPos.size()));

		vPos.push_back(center);
		return true;
	}

//	create a reference mapping
	ReferenceMapping<reference_element_type, dim> map(&(vVertPos[0]));

//	get local shape function set
	const LocalShapeFunctionSet<refDim>& lsfs
		= LocalShapeFunctionSetProvider::get<refDim>(roid, lfeID);

//	typedef local position type
	typedef typename LocalShapeFunctionSet<refDim>::position_type local_pos_type;

//	clear pos
	vPos.resize(lsfs.num_sh());

//	bool flag if position is exact, or no exact position available for shapes
	bool bExact = true;

//	loop all shape functions
	for(size_t sh = 0; sh < lsfs.num_sh(); ++sh)
	{
	//	get local position
		local_pos_type locPos;
		bExact &= lsfs.position(sh, locPos);

	//	map to global position
		map.local_to_global(vPos[sh], locPos);
	}

//	return if positions are given exactly
	return bExact;
};

template <typename TDomain>
bool DoFPosition(std::vector<MathVector<TDomain::dim> >& vPos, GeometricObject* elem, TDomain& domain, LFEID lfeID)
{
	switch(elem->base_object_id())
	{
		case VERTEX: return DoFPosition(vPos, static_cast<VertexBase*>(elem), domain, lfeID);
		case EDGE:   return DoFPosition(vPos, static_cast<EdgeBase*>(elem), domain, lfeID);
		case FACE:   return DoFPosition(vPos, static_cast<Face*>(elem), domain, lfeID);
		case VOLUME: return DoFPosition(vPos, static_cast<Volume*>(elem), domain, lfeID);
	}
	UG_THROW("Base Object type not found.");
}

template <typename TDomain>
bool DoFPosition(std::vector<MathVector<TDomain::dim> >& vPos, VertexBase* elem, TDomain& domain, LFEID lfeID)
{
	return  DoFPositionOnVertex(vPos, elem, domain, lfeID);
}

template <typename TDomain>
bool DoFPosition(std::vector<MathVector<TDomain::dim> >& vPos, EdgeBase* elem, TDomain& domain, LFEID lfeID)
{
	switch(elem->container_section())
	{
		case CSEDGE_EDGE: 			   return DoFPosition(vPos, static_cast<Edge*>(elem), domain, lfeID);
		case CSEDGE_CONSTRAINED_EDGE: return DoFPosition(vPos, static_cast<ConstrainedEdge*>(elem), domain, lfeID);
		case CSEDGE_CONSTRAINING_EDGE:return DoFPosition(vPos, static_cast<ConstrainingEdge*>(elem), domain, lfeID);
	}
	UG_THROW("Edge type not found.");
}

template <typename TDomain>
bool DoFPosition(std::vector<MathVector<TDomain::dim> >& vPos, Face* elem, TDomain& domain, LFEID lfeID)
{
	switch(elem->container_section())
	{
		case CSFACE_TRIANGLE: return DoFPosition(vPos, static_cast<Triangle*>(elem), domain, lfeID);
		case CSFACE_CONSTRAINED_TRIANGLE: return DoFPosition(vPos, static_cast<ConstrainedTriangle*>(elem), domain, lfeID);
		case CSFACE_CONSTRAINING_TRIANGLE: return DoFPosition(vPos, static_cast<ConstrainingTriangle*>(elem), domain, lfeID);
		case CSFACE_QUADRILATERAL: return DoFPosition(vPos, static_cast<Quadrilateral*>(elem), domain, lfeID);
		case CSFACE_CONSTRAINED_QUADRILATERAL: return DoFPosition(vPos, static_cast<ConstrainedQuadrilateral*>(elem), domain, lfeID);
		case CSFACE_CONSTRAINING_QUADRILATERAL: return DoFPosition(vPos, static_cast<ConstrainingQuadrilateral*>(elem), domain, lfeID);
	}
	UG_THROW("Face type not found.");
}

template <typename TDomain>
bool DoFPosition(std::vector<MathVector<TDomain::dim> >& vPos, Volume* elem, TDomain& domain, LFEID lfeID)
{
	switch(elem->container_section())
	{
		case CSVOL_TETRAHEDRON: return DoFPosition(vPos, static_cast<Tetrahedron*>(elem), domain, lfeID);
		case CSVOL_PYRAMID: return DoFPosition(vPos, static_cast<Pyramid*>(elem), domain, lfeID);
		case CSVOL_PRISM: return DoFPosition(vPos, static_cast<Prism*>(elem), domain, lfeID);
		case CSVOL_HEXAHEDRON: return DoFPosition(vPos, static_cast<Hexahedron*>(elem), domain, lfeID);
	}
	UG_THROW("Volume type not found.");
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

template <typename TDomain, typename TAlgebra>
GridFunction<TDomain, TAlgebra>::
GridFunction(SmartPtr<approximation_space_type> approxSpace,
             SmartPtr<DoFDistribution> spDoFDistr, bool bManage)
 : m_spDD(spDoFDistr), m_spApproxSpace(approxSpace)
{
	if(!m_spDD.valid()) UG_THROW("DoF Distribution is null.");

	if(bManage)
		m_spDD->manage_grid_function(*this);

	check_algebra();
	resize_values(num_indices());
#ifdef UG_PARALLEL
//	set layouts
	this->set_layouts(m_spDD->layouts());

//	set storage type
	this->set_storage_type(PST_UNDEFINED);
#endif
};

template <typename TDomain, typename TAlgebra>
GridFunction<TDomain, TAlgebra>::
GridFunction(SmartPtr<approximation_space_type> approxSpace, bool bManage)
	: m_spDD(approxSpace->surface_dof_distribution()),
	  m_spApproxSpace(approxSpace)
{
	if(!m_spDD.valid()) UG_THROW("DoF Distribution is null.");

	if(bManage)
		m_spDD->manage_grid_function(*this);

	m_bManaged = bManage;

	check_algebra();
	resize_values(num_indices());
#ifdef UG_PARALLEL
//	set layouts
	this->set_layouts(m_spDD->layouts());

//	set storage type
	this->set_storage_type(PST_UNDEFINED);
#endif
};

template <typename TDomain, typename TAlgebra>
GridFunction<TDomain, TAlgebra>::
GridFunction(SmartPtr<approximation_space_type> approxSpace, int level, bool bManage)
	: m_spDD(approxSpace->surface_dof_distribution(level)),
	  m_spApproxSpace(approxSpace)
{
	if(!m_spDD.valid()) UG_THROW("DoF Distribution is null.");

	if(bManage)
		m_spDD->manage_grid_function(*this);

	check_algebra();
	resize_values(num_indices());
#ifdef UG_PARALLEL
//	set layouts
	this->set_layouts(m_spDD->layouts());

//	set storage type
	this->set_storage_type(PST_UNDEFINED);
#endif
};

template <typename TDomain, typename TAlgebra>
GridFunction<TDomain, TAlgebra>::
GridFunction(SmartPtr<approximation_space_type> approxSpace, const GridLevel& gl, bool bManage)
	: m_spDD(approxSpace->dof_distribution(gl)),
	  m_spApproxSpace(approxSpace)
{
	if(!m_spDD.valid()) UG_THROW("DoF Distribution is null.");

	if(bManage)
		m_spDD->manage_grid_function(*this);

	check_algebra();
	resize_values(num_indices());
#ifdef UG_PARALLEL
//	set layouts
	this->set_layouts(m_spDD->layouts());

//	set storage type
	this->set_storage_type(PST_UNDEFINED);
#endif
};

template <typename TDomain, typename TAlgebra>
void
GridFunction<TDomain, TAlgebra>::check_algebra()
{
//	get blocksize of algebra
	const int blockSize = algebra_type::blockSize;

//	a)	If blocksize fixed and > 1, we need grouping.
	if(blockSize > 1 && !this->m_spDD->grouped())
	{
		UG_THROW("Fixed block algebra needs grouped dofs.");
	}
//	b) 	If blocksize flexible, we group
	else if (blockSize == AlgebraType::VariableBlockSize
			&& !this->m_spDD->grouped())
	{
		UG_THROW("Variable block algebra needs grouped dofs.");
	}
//	c)	If blocksize == 1, we do not group. This will allow us to handle
//		this case for any problem.
	else if (blockSize == 1 && this->m_spDD->grouped())
	{
		UG_THROW("block 1x1 algebra needs non-grouped dofs.");
	}
}

template <typename TDomain, typename TAlgebra>
template <typename TElem>
bool
GridFunction<TDomain, TAlgebra>::
dof_positions(TElem* elem, size_t fct, std::vector<MathVector<dim> >& vPos) const
{
	return DoFPosition(vPos, elem, *domain(),
	                   this->local_finite_element_id(fct),
	                   this->dim(fct));
};

template <typename TDomain,typename TAlgebra>
template <typename TElem>
bool
GridFunction<TDomain, TAlgebra>::
inner_dof_positions(TElem* elem, size_t fct, std::vector<MathVector<dim> >& vPos) const
{
	return InnerDoFPosition(vPos, elem, *domain(),
	                        this->local_finite_element_id(fct),
	                        this->dim(fct));
};

template <typename TDomain, typename TAlgebra>
void
GridFunction<TDomain, TAlgebra>::
clone_pattern(const this_type& v)
{
// 	copy approximation space
	m_spApproxSpace = v.m_spApproxSpace;

//	assign dof distribution (resizes vector)
	this->m_spDD = v.m_spDD;

//	resize the vector
	resize_values(v.size());

#ifdef UG_PARALLEL
//	set layouts
	this->set_layouts(v.layouts());

//	copy storage type
	this->set_storage_type(v.get_storage_mask());
#endif
};

template <typename TDomain, typename TAlgebra>
void
GridFunction<TDomain, TAlgebra>::
resize_values(size_t s, number defaultValue)
{
//	remember old values
	const size_t oldSize = vector_type::size();

//	resize vector
	vector_type::resize_sloppy(s);

//	set vector to zero-values
	for(size_t i = oldSize; i < s; ++i)
		this->operator[](i) = defaultValue;
}

template <typename TDomain, typename TAlgebra>
void
GridFunction<TDomain, TAlgebra>::
permute_values(const std::vector<size_t>& vIndNew)
{
//	check sizes
	if(vIndNew.size() != this->size())
		UG_THROW("GridFunction::permute_values: For a permutation the"
				" index set must have same cardinality as vector.");

// \todo: avoid tmp vector, only copy values into new vector and use that one
//	create tmp vector
	vector_type vecTmp; vecTmp.resize(this->size());
#ifdef UG_PARALLEL
//	copy storage type
	vecTmp.set_storage_type(this->get_storage_mask());
#endif

//	loop indices and copy values
	for(size_t i = 0; i < vIndNew.size(); ++i)
		vecTmp[vIndNew[i]] = this->operator[](i);

//	copy tmp vector into this vector
	this->assign(vecTmp);
}

template <typename TDomain, typename TAlgebra>
void
GridFunction<TDomain, TAlgebra>::
copy_values(const std::vector<std::pair<size_t, size_t> >& vIndexMap,bool bDisjunct)
{
//	disjunct case
	if(bDisjunct)
		for(size_t i = 0; i < vIndexMap.size(); ++i)
			this->operator[](vIndexMap[i].second)
				= this->operator[](vIndexMap[i].first);
	else {
		typedef typename vector_type::value_type value_type;
		std::vector<value_type> values;
		values.resize(vIndexMap[vIndexMap.size()-1].first);
		for(size_t i = 0; i < vIndexMap.size(); ++i){ 
			const size_t index = vIndexMap[i].first;
			if (index>=values.size()) values.resize(index+1);
			values[index] = this->operator[](index);
		}
		for(size_t i = 0; i < vIndexMap.size(); ++i)
			this->operator[](vIndexMap[i].second)
				= values[vIndexMap[i].first];
	}
}

template <typename TDomain, typename TAlgebra>
void GridFunction<TDomain, TAlgebra>::assign(const vector_type& v)
{
//	check size
	if(v.size() != vector_type::size())
		UG_THROW("GridFunction: Assigned vector has incorrect size.");

//	assign vector
	*(dynamic_cast<vector_type*>(this)) = v;

#ifdef UG_PARALLEL
//	set layouts
	this->set_layouts(v.layouts());

//	copy storage type
	this->set_storage_type(v.get_storage_mask());
#endif
}

template <typename TDomain, typename TAlgebra>
void GridFunction<TDomain, TAlgebra>::assign(const this_type& v)
{
// 	copy approximation space
	m_spApproxSpace = v.m_spApproxSpace;

//	assign dof distribution (resizes vector)
	this->m_spDD = v.m_spDD;

	if(v.m_bManaged==true)
		this->m_spDD->manage_grid_function(*this);

//	resize the vector
	resize_values(v.size());

//  copy values
	*(dynamic_cast<vector_type*>(this)) = *dynamic_cast<const vector_type*>(&v);

#ifdef UG_PARALLEL
//	set layouts
	this->set_layouts(v.layouts());

//	copy storage type
	this->set_storage_type(v.get_storage_mask());
#endif
}

template <typename TDomain, typename TAlgebra>
GridFunction<TDomain, TAlgebra>*
GridFunction<TDomain, TAlgebra>::virtual_clone_without_values() const
{
	GridFunction<TDomain, TAlgebra>* v
		= new GridFunction<TDomain, TAlgebra>(m_spApproxSpace, this->m_spDD);
	v->resize_values(this->size());
#ifdef UG_PARALLEL
	v->set_layouts(this->layouts());
	v->set_storage_type(PST_UNDEFINED);
#endif
	return v;
}


template <typename TDomain, typename TAlgebra>
void GridFunction<TDomain, TAlgebra>::
add_transfer(SmartPtr<ILocalTransferAlgebra<TAlgebra> > spTransfer)
{
	spTransfer->set_vector(this);
	m_spDD->add_transfer(spTransfer);
}

template <typename TDomain, typename TAlgebra>
void GridFunction<TDomain, TAlgebra>::add_transfer(SmartPtr<ILocalTransfer> spTransfer)
{
	m_spDD->add_transfer(spTransfer);
}

template <typename TDomain, typename TAlgebra>
void GridFunction<TDomain, TAlgebra>::remove_transfer(SmartPtr<ILocalTransfer> spTransfer)
{
	m_spDD->remove_transfer(spTransfer);
}

template <typename TDomain, typename TAlgebra>
void GridFunction<TDomain, TAlgebra>::clear_transfers()
{
	m_spDD->clear_transfers();
}

} // end namespace ug

#endif /* __H__UG__LIB_DISC__FUNCTION_SPACE__GRID_FUNCTION_IMPL__ */
