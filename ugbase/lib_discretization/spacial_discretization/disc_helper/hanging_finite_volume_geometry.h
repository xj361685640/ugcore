/*
 * hanging_finite_volume_geometry.h
 *
 *  Created on: 08.12.2009
 *      Author: andreasvogel
 */

#ifndef __H__LIB_DISCRETIZATION__SPACIAL_DISCRETIZATION__DISC_HELPER__HANGING_FINITE_VOLUME_GEOMETRY__
#define __H__LIB_DISCRETIZATION__SPACIAL_DISCRETIZATION__DISC_HELPER__HANGING_FINITE_VOLUME_GEOMETRY__

// extern libraries
#include <cmath>
#include <vector>

// other ug4 modules
#include "common/common.h"
#include "lib_grid/lib_grid.h"
#include "lib_algebra/lib_algebra.h"

// library intern includes
#include "../../reference_element/reference_element.h"
#include "../../local_shape_function_set/local_shape_function_set_factory.h"

namespace ug{

//// Some Help Functions //////
///////////////////////////////

template <int ref_dim, int world_dim>
number VolumeOfSCV(const std::vector<MathVector<world_dim> >& vPoints);

template <>
number VolumeOfSCV<1,1>(const std::vector<MathVector<1> >& vPoints)
{
	UG_ASSERT(vPoints.size() == 2, "Must be a line.");

	return fabs(vPoints[0][0] - vPoints[1][0]);
}

template <>
number VolumeOfSCV<1,2>(const std::vector<MathVector<2> >& vPoints)
{
	UG_ASSERT(vPoints.size() == 2, "Must be a line.");

	return VecDistance(vPoints[0], vPoints[1]);
}

template <>
number VolumeOfSCV<2,2>(const std::vector<MathVector<2> >& vPoints)
{
	UG_ASSERT(vPoints.size() == 4, "Must be a quadrilateral.");

	const number tmp = (vPoints[3][1]-vPoints[1][1])*(vPoints[2][0]-vPoints[0][0])
				-(vPoints[3][0]-vPoints[1][0])*(vPoints[2][1]-vPoints[0][1]);
	return 0.5 * fabs( tmp );
}

////////////////////////////////

template <int ref_dim, int world_dim>
void NormalOnSCVF(MathVector<world_dim>& outNormal, const std::vector<MathVector<world_dim> >& vPoints)
{
	UG_ASSERT(0, "Not implemented.");
}

template <>
void NormalOnSCVF<2,2>(MathVector<2>& outNormal, const std::vector<MathVector<2> >& vPoints)
{
	UG_ASSERT(vPoints.size() == 4, "Must be a quadrilateral.");

	MathVector<2> diff = vPoints[1]; // center of element
	diff -= vPoints[0]; // edge midpoint

	outNormal[0] = diff[1];
	outNormal[1] = -diff[0];
}

/////////////////////////////////////////
/////////////////////////////////////////

template <	typename TElem,
			int TWorldDim>
class HFVGeometry {
	private:
		typedef typename reference_element_traits<TElem>::reference_element_type ref_elem_type;
	public:
		static const int dim = ref_elem_type::dim;
		static const int world_dim = TWorldDim;

	public:
		HFVGeometry()
		{
			m_vSCV.resize(ref_elem_type::num_corners);
			for(size_t i = 0; i < (size_t) ref_elem_type::num_corners; ++i)
			{
				static const ref_elem_type refElem;
				m_vSCV[i].nodeId = i;
				m_vSCV[i].isHanging = false;
				m_vSCV[i].localPosition[0] = refElem.corner(i);
			}

			// compute center
			m_localCenter = 0.0;
			for(size_t i = 0; i < m_vSCV.size(); ++i)
			{
				m_localCenter += m_vSCV[i].localPosition[0];
			}
			m_localCenter *= 1./(m_vSCV.size());
		}

		bool update(TElem* elem, Grid& mg, MathVector<world_dim> corners[])
		{
			static const ref_elem_type refElem;

			// reset to natural nodes
			m_vSCV.resize(ref_elem_type::num_corners);

			// remember global position of nodes
			for(size_t i = 0; i < (size_t)ref_elem_type::num_corners; ++i)
				m_vSCV[i].globalPosition[0] = corners[i];

			// compute center
			m_globalCenter = 0.0;
			for(size_t i = 0; i < m_vSCV.size(); ++i)
			{
				m_globalCenter += (m_vSCV[i].globalPosition)[0];
			}
			m_globalCenter *= 1./(m_vSCV.size());

			// get natural edges
			std::vector<EdgeBase*> vEdges;
			CollectEdges(vEdges, mg, elem);

			// setup vector with needed edges (i.e. Edge and ConstrainedEdge)
			m_vSCVF.clear();
			for(size_t i = 0; i < vEdges.size(); ++i)
			{
				// natural ids of end of edge
				const size_t left = refElem.id(1, i, 0, 0);
				const size_t right = refElem.id(1, i, 0, 1);

				// choose weather to insert to or one new edge
				switch(vEdges[i]->shared_pipe_section())
				{
				case SPSEDGE_CONSTRAINED_EDGE:
				case SPSEDGE_EDGE:
					// classic case: Just set corner ids
					m_vSCVF.resize(m_vSCVF.size() + 1);
					m_vSCVF.back().nodeId[0] = left;
					m_vSCVF.back().nodeId[1] = right;
					break;

				case SPSEDGE_CONSTRAINING_EDGE:
					// insert hanging node in list of nodes
					const size_t newNodeId = m_vSCV.size();
					m_vSCV.resize(newNodeId + 1);
					m_vSCV.back().nodeId = newNodeId;
					m_vSCV.back().isHanging = true;
					VecInterpolateLinear(	m_vSCV[i].localPosition[0],
											m_vSCV[left].localPosition[0],
											m_vSCV[right].localPosition[0],
											0.5);
					VecInterpolateLinear(	m_vSCV[i].globalPosition[0],
											m_vSCV[left].globalPosition[0],
											m_vSCV[right].globalPosition[0],
											0.5);
					m_vSCV[i].interpolNodeId[0] = left;
					m_vSCV[i].interpolNodeId[1] = right;


					// insert to edges with nodeIds
					m_vSCVF.resize(m_vSCVF.size() + 1);
					m_vSCVF.back().nodeId[0] = left;
					m_vSCVF.back().nodeId[1] = newNodeId;

					m_vSCVF.resize(m_vSCVF.size() + 1);
					m_vSCVF.back().nodeId[0] = newNodeId;
					m_vSCVF.back().nodeId[1] = right;
					break;

				default: UG_LOG("Cannot detect type of edge.\n"); return false;
				}
			}

			// compute midpoints of edges
			for(size_t i = 0; i < m_vSCVF.size(); ++i)
			{
				const size_t left = m_vSCVF[i].nodeId[0];
				const size_t right = m_vSCVF[i].nodeId[1];

				VecInterpolateLinear(	m_vSCVF[i].localPosition[0],
										m_vSCV[left].localPosition[0],
										m_vSCV[right].localPosition[0],
										0.5);
				VecInterpolateLinear(	m_vSCVF[i].globalPosition[0],
										m_vSCV[left].globalPosition[0],
										m_vSCV[right].globalPosition[0],
										0.5);

				// write edge midpoints to as corners of scv
				m_vSCV[left].localPosition[1] = m_vSCVF[i].localPosition[0];
				m_vSCV[right].localPosition[3] = m_vSCVF[i].localPosition[0];
				m_vSCV[left].globalPosition[1] = m_vSCVF[i].globalPosition[0];
				m_vSCV[right].globalPosition[3] = m_vSCVF[i].globalPosition[0];
			}

			// compute scvf
			for(size_t i = 0; i < m_vSCVF.size(); ++i)
			{
				VecInterpolateLinear(	m_vSCVF[i].globalIP,
										m_vSCVF[i].globalPosition[0],
										m_globalCenter,
										0.5);
				VecInterpolateLinear(	m_vSCVF[i].localIP,
										m_vSCVF[i].localPosition[0],
										m_localCenter,
										0.5);

				m_vSCVF[i].globalPosition[1] = m_globalCenter;
				m_vSCVF[i].localPosition[1] = m_localCenter;

				// normal
				NormalOnSCVF<dim, world_dim>(m_vSCVF[i].Normal, m_vSCVF[i].globalPosition);
			}

			// compute size of scv
			for(size_t i = 0; i < m_vSCV.size(); ++i)
			{
				m_vSCV[i].vol = VolumeOfSCV<dim, world_dim>(m_vSCV[i].globalPosition);
			}

			// compute shapes and derivatives
			m_mapping.update(corners);

			for(size_t i = 0; i < m_vSCVF.size(); ++i)
			{
				if(!m_mapping.jacobian_transposed_inverse(m_vSCVF[i].localIP, m_vSCVF[i].JtInv))
					{UG_LOG("Cannot compute jacobian transposed.\n"); return false;}
				if(!m_mapping.jacobian_det(m_vSCVF[i].localIP, m_vSCVF[i].detj))
					{UG_LOG("Cannot compute jacobian determinate.\n"); return false;}

				const LocalShapeFunctionSet<ref_elem_type>& TrialSpace =
						LocalShapeFunctionSetFactory::inst().get_local_shape_function_set<ref_elem_type>(LSFS_LAGRANGEP1);

				m_vSCVF[i].vShape.resize(m_vSCV.size());
				m_vSCVF[i].localGrad.resize(m_vSCV.size());
				m_vSCVF[i].globalGrad.resize(m_vSCV.size());
				for(size_t sh = 0 ; sh < m_vSCV.size(); ++sh)
				{
					if(!(m_vSCV[sh].isHanging))
					{
						if(!TrialSpace.evaluate(sh, m_vSCVF[i].localIP, (m_vSCVF[i].vShape)[sh]))
							{UG_LOG("Cannot evaluate local shape.\n"); return false;}
						if(!TrialSpace.evaluate_grad(sh, m_vSCVF[i].localIP, (m_vSCVF[i].localGrad)[sh]))
							{UG_LOG("Cannot evaluate local grad.\n"); return false;}
						MatVecMult((m_vSCVF[i].globalGrad)[sh], m_vSCVF[i].JtInv, (m_vSCVF[i].localGrad)[sh]);
					}
					else
					{
						// this is ok, since all hanging nodes come last
						(m_vSCVF[i].vShape)[sh] = 0.5*((m_vSCVF[i].vShape)[m_vSCVF[i].nodeId[0]]
						                                 +(m_vSCVF[i].vShape)[m_vSCVF[i].nodeId[1]]);
						VecInterpolateLinear(	(m_vSCVF[i].localGrad)[sh],
												(m_vSCVF[i].localGrad)[m_vSCVF[i].nodeId[0]],
												(m_vSCVF[i].localGrad)[m_vSCVF[i].nodeId[1]],
												0.5);
						VecInterpolateLinear(	(m_vSCVF[i].globalGrad)[sh],
												(m_vSCVF[i].globalGrad)[m_vSCVF[i].nodeId[0]],
												(m_vSCVF[i].globalGrad)[m_vSCVF[i].nodeId[1]],
												0.5);
					}
				}

			}

			return true;
		}

	public:
		class SCVF;
		class SCV;

	public:
		inline size_t num_scvf() const {return m_vSCVF.size();};
		inline const SCVF& scvf(size_t i) const {return m_vSCVF[i];}

		size_t num_scv() const {return m_vSCV.size();}
		inline const SCV& scv(size_t i) const {return m_vSCV[i];}

	public:
		class SCVF
		{
			friend class HFVGeometry<TElem, TWorldDim>;

			public:
				SCVF()
				{
					 int num = 2*dim;
					if(dim == 1) num = 1;
					localPosition.resize(num);
					globalPosition.resize(num);
				}

				inline size_t from() const {return nodeId[0];}
				inline size_t to() const {return nodeId[1];}
				inline const MathVector<dim>& local_ip() const {return localIP;}
				inline const MathVector<world_dim>& global_ip() const {return globalIP;}
				inline const MathVector<world_dim>& normal() const {return Normal;} // includes area

				inline const MathMatrix<dim,world_dim>& JTInv() const {return JtInv;}
				inline number detJ() const {return detj;}

				inline size_t num_sh() const {return vShape.size();}
				inline number shape(size_t i) const {return vShape[i];}
				inline const MathVector<world_dim>& local_grad(size_t i) const {return localGrad[i];}
				inline const MathVector<world_dim>& global_grad(size_t i) const {return globalGrad[i];}

			private:
				// edge part
				size_t nodeId[2]; // node ids of associated edge
				// ordering is:
				// 2D: edgeMidPoint, CenterOfElement
				// 3D: edgeMidPoint, Side one, CenterOfElement, Side two
				std::vector<MathVector<dim> > localPosition; // local corners of scvf
				std::vector<MathVector<world_dim> > globalPosition; // global corners of scvf

				// scvf part
				MathVector<dim> localIP; // local integration point
				MathVector<world_dim> globalIP; // global intergration point
				MathVector<world_dim> Normal; // normal (incl. area)

				// shapes and derivatives
				std::vector<number> vShape; // shapes at ip
				std::vector<MathVector<dim> > localGrad; // local grad at ip
				std::vector<MathVector<world_dim> > globalGrad; // global grad at ip
				MathMatrix<world_dim,dim> JtInv; // Jacobian transposed at ip
				number detj; // Jacobian det at ip
		};

		class SCV
		{
			friend class HFVGeometry<TElem, TWorldDim>;

			public:
				SCV()
				{
					//TODO :this is 2^dim, except for prism, where we have 9
					localPosition.resize(2*dim);
					globalPosition.resize(2*dim);
				}
				inline size_t node_id() const {return nodeId;}
				inline const MathVector<world_dim>& local_ip() const {return localPosition[0];}
				inline const MathVector<world_dim>& global_ip() const {return globalPosition[0];}
				inline number volume() const {return vol;}

			private:
				size_t nodeId; // node id of associated node
				// The ordering is: Corner, ...
				std::vector<MathVector<dim> > localPosition; // local position of node
				std::vector<MathVector<world_dim> > globalPosition; // global position of node
				bool isHanging; // true, if node is a hanging node
				size_t interpolNodeId[2]; // corners from which is interpolated (only if hanging)
				number vol;
		};

	private:
		MathVector<dim> m_localCenter;
		MathVector<world_dim> m_globalCenter;

		std::vector<SCVF> m_vSCVF;
		std::vector<SCV> m_vSCV;

		std::vector<std::vector<MathVector<world_dim> > > m_vMidPointGlobal;
		std::vector<std::vector<MathVector<dim> > > m_vMidPointLocal;

		ReferenceMapping<ref_elem_type, world_dim> m_mapping;
};

}

#endif /* __H__LIB_DISCRETIZATION__SPACIAL_DISCRETIZATION__DISC_HELPER__HANGING_FINITE_VOLUME_GEOMETRY__ */
