// created by Sebastian Reiter
// s.b.reiter@gmail.com
// 29.11.2012 (d,m,y)

#include <sstream>
#include "common/static_assert.h"
#include "common/util/table.h"
#include "distribution.h"
#include "distributed_grid.h"
#include "lib_grid/algorithms/selection_util.h"
#include "lib_grid/algorithms/subset_util.h"
#include "lib_grid/algorithms/attachment_util.h"
#include "parallelization_util.h"
#include "lib_grid/file_io/file_io.h"

//#define LG_DISTRIBUTION_DEBUG

using namespace std;

namespace ug{

enum InterfaceStates{
	IS_UNASSIGNED = 0,
	IS_NORMAL = 1,
	IS_VMASTER = 1<<1,
	IS_VSLAVE = 1<<2,
	IS_DUMMY = 1<<3
};


struct TargetProcInfo
{
	TargetProcInfo()	{}
	TargetProcInfo(int pID, byte intfcState) :
		procID(pID), interfaceState(intfcState) {}

	int procID;
	byte interfaceState; // or-combinations of constants from InterfaceStates
};

typedef Attachment<vector<TargetProcInfo> >	ADistInfo;

///	Automatically attaches ADistInfo to all elements of a grid.
/**	On destruction, the attached dist info attachments are removed again.
 * You may access the dist-info in an element through the get method.
 * The get method returns a reference to the attached DistInfo object.
 *
 * Make sure that the given grid is valid while the DistInfoSupplier exists.
 */
class DistInfoSupplier{
	public:
		DistInfoSupplier(Grid& grid) : m_grid(grid)
		{
			m_grid.attach_to_all(m_aDistInfo);
			m_aaDistInfoVRT.access(grid, m_aDistInfo);
			m_aaDistInfoEDGE.access(grid, m_aDistInfo);
			m_aaDistInfoFACE.access(grid, m_aDistInfo);
			m_aaDistInfoVOL.access(grid, m_aDistInfo);
		}

		~DistInfoSupplier()
		{
			m_grid.detach_from_all(m_aDistInfo);
		}

		vector<TargetProcInfo>& get(VertexBase* vrt)	{return m_aaDistInfoVRT[vrt];}
		vector<TargetProcInfo>& get(EdgeBase* edge)		{return m_aaDistInfoEDGE[edge];}
		vector<TargetProcInfo>& get(Face* face)			{return m_aaDistInfoFACE[face];}
		vector<TargetProcInfo>& get(Volume* vol)		{return m_aaDistInfoVOL[vol];}
		vector<TargetProcInfo>& get(GeometricObject* obj)
		{
			int objType = obj->base_object_id();
			switch(objType){
				case VERTEX:	return get(static_cast<VertexBase*>(obj));
				case EDGE:		return get(static_cast<EdgeBase*>(obj));
				case FACE:		return get(static_cast<Face*>(obj));
				case VOLUME:	return get(static_cast<Volume*>(obj));
				default:	UG_THROW("Unknown geometric object base type."); break;
			}
		}

		ADistInfo dist_info_attachment()	{return m_aDistInfo;}

	private:
	//	copy construction unsupported.
		DistInfoSupplier(const DistInfoSupplier& di) : m_grid(di.m_grid) {}

		Grid& 		m_grid;
		ADistInfo	m_aDistInfo;
		Grid::AttachmentAccessor<VertexBase, ADistInfo> m_aaDistInfoVRT;
		Grid::AttachmentAccessor<EdgeBase, ADistInfo> m_aaDistInfoEDGE;
		Grid::AttachmentAccessor<Face, ADistInfo> m_aaDistInfoFACE;
		Grid::AttachmentAccessor<Volume, ADistInfo> m_aaDistInfoVOL;
};


////////////////////////////////////////////////////////////////////////////////
///	Communicates the distribution infos through existing interfaces
/**	Distribution infos are packed into the send buffer for each node and are
 * either merged with existing entries or existing entries are simply overwritten.
 * The merge/overwrite behavior can be chosen through the member method enable_merge.
 */
template <class TLayout>
class ComPol_SynchronizeDistInfos : public pcl::ICommunicationPolicy<TLayout>
{
	public:
		typedef TLayout							Layout;
		typedef typename Layout::Type			GeomObj;
		typedef typename Layout::Element		Element;
		typedef typename Layout::Interface		Interface;
		typedef typename Interface::iterator	InterfaceIter;

		ComPol_SynchronizeDistInfos(DistInfoSupplier& distInfos, bool merge) :
			m_distInfos(distInfos), m_mergeEnabled(merge)	{}

		virtual ~ComPol_SynchronizeDistInfos()	{}

		void enable_merge(bool enable)	{m_mergeEnabled = enable;}
		bool merge_enabled()			{return m_mergeEnabled;}

		virtual int
		get_required_buffer_size(Interface& interface)		{return -1;}

	///	write target processes and move-flag
		virtual bool
		collect(ug::BinaryBuffer& buff, Interface& intfc)
		{
			for(InterfaceIter iter = intfc.begin(); iter != intfc.end(); ++iter){
				Element elem = intfc.get_element(iter);
				Serialize(buff, m_distInfos.get(elem));
			}
			return true;
		}

	///	read target processes and move-flag
		virtual bool
		extract(ug::BinaryBuffer& buff, Interface& intfc)
		{
			if(m_mergeEnabled){
				vector<TargetProcInfo> tpInfo;
				for(InterfaceIter iter = intfc.begin(); iter != intfc.end(); ++iter){
					tpInfo.clear();
					Deserialize(buff, tpInfo);

					Element elem = intfc.get_element(iter);
					vector<TargetProcInfo>& tpInfoDest = m_distInfos.get(elem);
					size_t initialInfoSize = tpInfoDest.size();

					for(size_t i_src = 0; i_src < tpInfo.size(); ++i_src){
						int procID = tpInfo[i_src].procID;
						bool gotOne = false;

					//	we only have to check entries up to initialInfoSize, since
					//	all following entries have been added during this operation.
					//	Since there are no double entries in tpInfo, there's no
					//	need to check against those new entries in tpInfoDest.
						for(size_t i = 0; i < initialInfoSize; ++i){
							if(procID == tpInfoDest[i].procID){
								tpInfoDest[i].interfaceState
												|= tpInfo[i_src].interfaceState;
								gotOne = true;
								break;
							}
						}

						if(!gotOne)
							tpInfoDest.push_back(tpInfo[i_src]);
					}
				}
			}
			else{
				for(InterfaceIter iter = intfc.begin(); iter != intfc.end(); ++iter){
					Element elem = intfc.get_element(iter);
					vector<TargetProcInfo>& tpInfo = m_distInfos.get(elem);
					tpInfo.clear();
					Deserialize(buff, tpInfo);
				}
			}

			return true;
		}

	protected:
		DistInfoSupplier& 	m_distInfos;
		bool				m_mergeEnabled;
};


////////////////////////////////////////////////////////////////////////////////
template <class TElem>
static void SynchronizeDistInfos(MultiGrid& mg, DistInfoSupplier& distInfos)
{
	typedef typename GridLayoutMap::Types<TElem>::Layout	ElemLayout;
	GridLayoutMap& glm = mg.distributed_grid_manager()->grid_layout_map();
	pcl::InterfaceCommunicator<ElemLayout>	com;
	ComPol_SynchronizeDistInfos<ElemLayout>	compolSync(distInfos, true);

	compolSync.enable_merge(true);
	com.exchange_data(glm, INT_H_SLAVE, INT_H_MASTER, compolSync);
	com.communicate();

	compolSync.enable_merge(false);
	com.exchange_data(glm, INT_H_MASTER, INT_H_SLAVE, compolSync);
	com.communicate();

	compolSync.enable_merge(true);
	com.exchange_data(glm, INT_V_SLAVE, INT_V_MASTER, compolSync);
	com.communicate();

	compolSync.enable_merge(false);
	com.exchange_data(glm, INT_V_MASTER, INT_V_SLAVE, compolSync);
	com.communicate();
}

#ifdef LG_DISTRIBUTION_DEBUG
////////////////////////////////////////////////////////////////////////////////
static void SaveDistSelectorToFile(MGSelector& msel, const char* filename)
{
//	create a subset handler which holds different subsets for the different selection states
	MultiGrid& mg = *msel.multi_grid();
	SubsetHandler sh(mg);

	for(size_t lvl = 0; lvl < msel.num_levels(); ++lvl){
		for(MGSelector::traits<Volume>::iterator iter = msel.begin<Volume>(lvl);
			iter != msel.end<Volume>(lvl); ++iter)
		{
			sh.assign_subset(*iter, msel.get_selection_status(*iter));
		}

		for(MGSelector::traits<Face>::iterator iter = msel.begin<Face>(lvl);
			iter != msel.end<Face>(lvl); ++iter)
		{
			sh.assign_subset(*iter, msel.get_selection_status(*iter));
		}

		for(MGSelector::traits<EdgeBase>::iterator iter = msel.begin<EdgeBase>(lvl);
			iter != msel.end<EdgeBase>(lvl); ++iter)
		{
			sh.assign_subset(*iter, msel.get_selection_status(*iter));
		}

		for(MGSelector::traits<VertexBase>::iterator iter = msel.begin<VertexBase>(lvl);
			iter != msel.end<VertexBase>(lvl); ++iter)
		{
			sh.assign_subset(*iter, msel.get_selection_status(*iter));
		}
	}

	const char* subsetNames[] = {"unassigned", "normal", "vmaster", "normal+vmaster",
								 "vslave", "normal+vslave", "vmaster+vslave",
								 "normal+vmaster+vslave", "dummy", "normal+dummy",
								 "vmaster+dummy", "normal+vmaster+dummy", "vslave+dummy",
								 "normal+vslave+dummy", "vmaster+vslave+dummy",
								 "normal+vmaster+vslave+dummy"};

	UG_ASSERT(sh.num_subsets() <= 16, "Not enough subset names specified in internal list.");
	for(int i = 0; i < sh.num_subsets(); ++i)
		sh.subset_info(i).name = subsetNames[i];

	AssignSubsetColors(sh);
	EraseEmptySubsets(sh);
	SaveGridHierarchyTransformed(mg, sh, filename, 0.1);
}

////////////////////////////////////////////////////////////////////////////////
static void SaveDistInfosToFile(MultiGrid& mg, DistInfoSupplier& infoSupplier,
								const char* filename)
{
//	create a subset handler which holds different subsets for the different selection states
	SubsetHandler sh(mg);

	for(MultiGrid::traits<Volume>::iterator iter = mg.begin<Volume>();
		iter != mg.end<Volume>(); ++iter)
	{
		vector<TargetProcInfo>& infos = infoSupplier.get(*iter);
		byte state = 0;
		for(size_t i = 0; i < infos.size(); ++i)
			state |= infos[i].interfaceState;
		sh.assign_subset(*iter, state);
	}

	for(MultiGrid::traits<Face>::iterator iter = mg.begin<Face>();
		iter != mg.end<Face>(); ++iter)
	{
		vector<TargetProcInfo>& infos = infoSupplier.get(*iter);
		byte state = 0;
		for(size_t i = 0; i < infos.size(); ++i)
			state |= infos[i].interfaceState;
		sh.assign_subset(*iter, state);
	}

	for(MultiGrid::traits<EdgeBase>::iterator iter = mg.begin<EdgeBase>();
		iter != mg.end<EdgeBase>(); ++iter)
	{
		vector<TargetProcInfo>& infos = infoSupplier.get(*iter);
		byte state = 0;
		for(size_t i = 0; i < infos.size(); ++i)
			state |= infos[i].interfaceState;
		sh.assign_subset(*iter, state);
	}

	for(MultiGrid::traits<VertexBase>::iterator iter = mg.begin<VertexBase>();
		iter != mg.end<VertexBase>(); ++iter)
	{
		vector<TargetProcInfo>& infos = infoSupplier.get(*iter);
		byte state = 0;
		for(size_t i = 0; i < infos.size(); ++i)
			state |= infos[i].interfaceState;
		sh.assign_subset(*iter, state);
	}

	const char* subsetNames[] = {"unassigned", "normal", "vmaster", "normal+vmaster",
								 "vslave", "normal+vslave", "vmaster+vslave",
								 "normal+vmaster+vslave", "dummy", "normal+dummy",
								 "vmaster+dummy", "normal+vmaster+dummy", "vslave+dummy",
								 "normal+vslave+dummy", "vmaster+vslave+dummy",
								 "normal+vmaster+vslave+dummy"};

	UG_ASSERT(sh.num_subsets() <= 16, "Not enough subset names specified in internal list.");
	for(int i = 0; i < sh.num_subsets(); ++i)
		sh.subset_info(i).name = subsetNames[i];

	AssignSubsetColors(sh);
	EraseEmptySubsets(sh);
	SaveGridHierarchyTransformed(mg, sh, filename, 0.1);
}

template <class TElem>
static void WriteDistInfosToTextFile(MultiGrid& mg, DistInfoSupplier& infoSupplier,
									 const char* filename)
{
	typedef typename MultiGrid::traits<TElem>::iterator TElemIter;

	Table<std::stringstream> table(mg.num<TElem>() + 1, 3);
	table(0, 0) << "lvl";	table(0, 1) << "center";	table(0, 2) << "interface states";

	int row = 1;
	for(size_t lvl = 0; lvl < mg.num_levels(); ++lvl){
		for(TElemIter iter = mg.begin<TElem>(lvl); iter != mg.end<TElem>(lvl);
			++iter, ++row)
		{
			TElem* e = *iter;

			table(row, 0) << lvl;
			table(row, 1) << GetGeometricObjectCenter(mg, e);

			vector<TargetProcInfo>& infos = infoSupplier.get(e);

			for(size_t i = 0; i < infos.size(); ++i){
				table(row, 2) << "p" << infos[i].procID << ": ";
				byte is = infos[i].interfaceState;
				if(is & IS_NORMAL)	table(row, 2) << "normal ";
				if(is & IS_VMASTER)	table(row, 2) << "vmaster ";
				if(is & IS_VSLAVE)	table(row, 2) << "vslave ";
				if(is & IS_DUMMY)	table(row, 2) << "dummy ";
			}

			table(row, 2) << "| ";
		}
	}

	ofstream out(filename);
	if(!out){
		UG_THROW("Couldn't open file " << filename << " for output.");
	}
	out << table;
	out.close();
}

#endif //LG_DISTRIBUTION_DEBUG

////////////////////////////////////////////////////////////////////////////////
///	Recursively selects unselected sides.
template <class TElem>
static void SelectUnselectedSides(MGSelector& msel, TElem* e,
								  ISelector::status_t status = ISelector::SELECTED)
{
	UG_ASSERT(msel.multi_grid(), "");
	MultiGrid& mg = *msel.multi_grid();

	typedef typename TElem::side TSide;
	typename MultiGrid::traits<TSide>::secure_container sides;

	mg.associated_elements(sides, e);
	for(size_t i = 0; i < sides.size(); ++i){
		if(!msel.is_selected(sides[i])){
			msel.select(sides[i], status);
			if(TElem::HAS_SIDES)
				SelectUnselectedSides(msel, sides[i], status);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
/**	selects unselected constrained elements of all selected constraining elements
 * and associated unselected low-dim elems.*/
static void SelectUnselectedConstrainedElements(MGSelector& msel,
								ISelector::status_t status = ISelector::SELECTED)
{
//	constraining triangles
	{
		typedef ConstrainingTriangle TElem;
		typedef MGSelector::traits<TElem>::iterator TIter;
		for(size_t lvl = 0; lvl < msel.num_levels(); ++lvl){
			for(TIter iter = msel.begin<TElem>(lvl);
				iter != msel.end<TElem>(lvl); ++iter)
			{
				ConstrainingFace* e = *iter;
				for(size_t i = 0; i < e->num_constrained_vertices(); ++i){
					VertexBase* cd = e->constrained_vertex(i);
					if(!msel.is_selected(cd)){
						msel.select(cd, status);
					}
				}
				for(size_t i = 0; i < e->num_constrained_edges(); ++i){
					EdgeBase* cd = e->constrained_edge(i);
					if(!msel.is_selected(cd)){
						msel.select(cd, status);
						SelectUnselectedSides(msel, cd, status);
					}
				}
				for(size_t i = 0; i < e->num_constrained_faces(); ++i){
					Face* cd = e->constrained_face(i);
					if(!msel.is_selected(cd)){
						msel.select(cd, status);
						SelectUnselectedSides(msel, cd, status);
					}
				}
			}
		}
	}

//	constraining quadrilaterals
	{
		typedef ConstrainingQuadrilateral TElem;
		typedef MGSelector::traits<TElem>::iterator TIter;
		for(size_t lvl = 0; lvl < msel.num_levels(); ++lvl){
			for(TIter iter = msel.begin<TElem>(lvl);
				iter != msel.end<TElem>(lvl); ++iter)
			{
				ConstrainingFace* e = *iter;
				for(size_t i = 0; i < e->num_constrained_vertices(); ++i){
					VertexBase* cd = e->constrained_vertex(i);
					if(!msel.is_selected(cd)){
						msel.select(cd, status);
					}
				}
				for(size_t i = 0; i < e->num_constrained_edges(); ++i){
					EdgeBase* cd = e->constrained_edge(i);
					if(!msel.is_selected(cd)){
						msel.select(cd, status);
						SelectUnselectedSides(msel, cd, status);
					}
				}
				for(size_t i = 0; i < e->num_constrained_faces(); ++i){
					Face* cd = e->constrained_face(i);
					if(!msel.is_selected(cd)){
						msel.select(cd, status);
						SelectUnselectedSides(msel, cd, status);
					}
				}
			}
		}
	}

//	constraining edges
	{
		typedef ConstrainingEdge TElem;
		typedef MGSelector::traits<TElem>::iterator TIter;
		for(size_t lvl = 0; lvl < msel.num_levels(); ++lvl){
			for(TIter iter = msel.begin<TElem>(lvl);
				iter != msel.end<TElem>(lvl); ++iter)
			{
				ConstrainingEdge* e = *iter;
				for(size_t i = 0; i < e->num_constrained_vertices(); ++i){
					VertexBase* cd = e->constrained_vertex(i);
					if(!msel.is_selected(cd)){
						msel.select(cd, status);
					}
				}
				for(size_t i = 0; i < e->num_constrained_edges(); ++i){
					EdgeBase* cd = e->constrained_edge(i);
					if(!msel.is_selected(cd)){
						msel.select(cd, status);
						SelectUnselectedSides(msel, cd, status);
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
/**	selects unselected constraining elements of all selected constrained elements
 * and associated unselected low-dim elems.*/
static void SelectUnselectedConstrainingElements(MGSelector& msel,
								ISelector::status_t status = ISelector::SELECTED)
{
//	constrained triangles
	{
		typedef ConstrainedTriangle TElem;
		typedef MGSelector::traits<TElem>::iterator TIter;
		for(size_t lvl = 0; lvl < msel.num_levels(); ++lvl){
			for(TIter iter = msel.begin<TElem>(lvl);
				iter != msel.end<TElem>(lvl); ++iter)
			{
				ConstrainedFace* e = *iter;
				if(GeometricObject* cg = e->get_constraining_object()){
					if(!msel.is_selected(cg)){
						msel.select(cg, status);
						UG_ASSERT(dynamic_cast<ConstrainingFace*>(cg),
								  "constraining object of a face has to be a "
								  "ConstrainingFace!");
						SelectUnselectedSides(msel, static_cast<Face*>(cg), status);
					}
				}
			}
		}
	}

//	constrained quadrilaterals
	{
		typedef ConstrainedQuadrilateral TElem;
		typedef MGSelector::traits<TElem>::iterator TIter;
		for(size_t lvl = 0; lvl < msel.num_levels(); ++lvl){
			for(TIter iter = msel.begin<TElem>(lvl);
				iter != msel.end<TElem>(lvl); ++iter)
			{
				ConstrainedFace* e = *iter;
				if(GeometricObject* cg = e->get_constraining_object()){
					if(!msel.is_selected(cg)){
						msel.select(cg, status);
						UG_ASSERT(dynamic_cast<Face*>(cg),
								  "constraining object of a face has to be a "
								  "Face!");
						SelectUnselectedSides(msel, static_cast<Face*>(cg), status);
					}
				}
			}
		}
	}

//	constrained edges
	{
		typedef ConstrainedEdge TElem;
		typedef MGSelector::traits<TElem>::iterator TIter;
		for(size_t lvl = 0; lvl < msel.num_levels(); ++lvl){
			for(TIter iter = msel.begin<TElem>(lvl);
				iter != msel.end<TElem>(lvl); ++iter)
			{
				ConstrainedEdge* e = *iter;
				if(GeometricObject* cg = e->get_constraining_object()){
					if(!msel.is_selected(cg)){
						msel.select(cg, status);
						switch(cg->base_object_id()){
						case EDGE:
							SelectUnselectedSides(msel, static_cast<EdgeBase*>(cg), status);
							break;
						case FACE:
							SelectUnselectedSides(msel, static_cast<Face*>(cg), status);
							break;
						}
					}
				}
			}
		}
	}

//	constrained vertices
	{
		typedef ConstrainedVertex TElem;
		typedef MGSelector::traits<TElem>::iterator TIter;
		for(size_t lvl = 0; lvl < msel.num_levels(); ++lvl){
			for(TIter iter = msel.begin<TElem>(lvl);
				iter != msel.end<TElem>(lvl); ++iter)
			{
				ConstrainedVertex* e = *iter;
				if(GeometricObject* cg = e->get_constraining_object()){
					if(!msel.is_selected(cg)){
						msel.select(cg, status);
						switch(cg->base_object_id()){
						case EDGE:
							SelectUnselectedSides(msel, static_cast<EdgeBase*>(cg), status);
							break;
						case FACE:
							SelectUnselectedSides(msel, static_cast<Face*>(cg), status);
							break;
						}
					}
				}
			}
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
/**	The method operates on selected entries only. Make sure that all elements
 * of type TElem which are being sent to a process are selected.
 *
 * If a selected element has no children and if it is a vertical master, it will
 * be marked as vertical master again.
 *
 * If a selected element has unselected children, then those children will be
 * selected as vertical master.
 *
 * This method only works correctly if called for the elements of highest dimension.
 */
template <class TElem>
static void AssignVerticalMasterAndSlaveStates(MGSelector& msel)
{
	UG_ASSERT(msel.multi_grid(), "Selector has to operate on a MultiGrid");
	MultiGrid& mg = *msel.multi_grid();
	DistributedGridManager& distGridMgr = *mg.distributed_grid_manager();

//	we start on the highest level and go downwards to avoid side
//	effects from repeated selection adjustment.
	typedef typename MGSelector::traits<TElem>::iterator TIter;
	for(int lvl = (int)msel.num_levels() - 1; lvl >= 0 ; --lvl){
		for(TIter iter = msel.begin<TElem>(lvl);
			iter != msel.end<TElem>(lvl); ++iter)
		{
			TElem* e = *iter;
		//	assign vertical master states first
			size_t numChildren = mg.num_children<TElem>(e);
			GeometricObject* parent = mg.get_parent(e);
			bool parentIsSelected = (lvl == 0);
			if(parent)
				parentIsSelected = msel.is_selected(parent);

			if(numChildren){
				for(size_t i = 0; i < numChildren; ++i){
					TElem* c = mg.get_child<TElem>(e, i);
					if(!msel.is_selected(c))
						msel.select(c, IS_VMASTER);
				}
			}
			else{
				if(parentIsSelected && distGridMgr.contains_status(e, ES_V_MASTER)){
					msel.select(e, IS_VMASTER);
					continue;
				}
			}

		//	and now slave states
			if(parent){
				if(!msel.is_selected(parent))
					msel.select(e, IS_VSLAVE);
			}
			else{
				if(distGridMgr.contains_status(e, ES_V_SLAVE))
					msel.select(e, IS_VSLAVE);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
/**	VSlaves will be ignored.*/
template <class TElem>
static void SelectUnselectedRootElementsAsVMasters(MGSelector& msel)
{
	typedef typename Grid::traits<TElem>::iterator TIter;

	UG_ASSERT(msel.multi_grid(), "Selector has to operate on a MultiGrid");
	MultiGrid& mg = *msel.multi_grid();
	DistributedGridManager& distGridMgr = *mg.distributed_grid_manager();

	for(TIter iter = mg.begin<TElem>(0); iter != mg.end<TElem>(0); ++iter){
		if(!msel.is_selected(*iter)){
			if(!distGridMgr.contains_status(*iter, ES_V_SLAVE))
				msel.select(*iter, IS_VMASTER);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
/**	VMasters will be ignored.*/
template <class TElem>
static void SelectSelectedRootElementsAsVSlaves(MGSelector& msel)
{
	typedef typename Grid::traits<TElem>::iterator TIter;

	UG_ASSERT(msel.multi_grid(), "Selector has to operate on a MultiGrid");
	MultiGrid& mg = *msel.multi_grid();
	DistributedGridManager& distGridMgr = *mg.distributed_grid_manager();

	for(TIter iter = msel.begin<TElem>(0); iter != msel.end<TElem>(0); ++iter){
		if(!distGridMgr.contains_status(*iter, ES_V_MASTER))
			msel.select(*iter, IS_VSLAVE);
	}
}

////////////////////////////////////////////////////////////////////////////////
static void SelectElementsForTargetPartition(MGSelector& msel,
								SubsetHandler& shPartition, int partitionIndex,
								bool partitionForLocalProc,
								bool createVerticalInterfaces)
{
//	elements which do not have parents (so called root-elements), and which are
//	not v-slaves have to have a copy on the local proc.
//	If they are not contained in the partition for the local proc, we'll add a
//	copy on the local proc and make it the v-master copy.
//	Note that this should only affect elements in the base-level.
//	The assignment is performed in SelectUnselectedRootElementsAsVMasters.
//todo	assert that only elements in the base-level do not have parents
//		(regarding the global grid)

	UG_ASSERT(msel.multi_grid(), "Selector has to operate on a MultiGrid");
	MultiGrid& mg = *msel.multi_grid();

	if(mg.num<Volume>() > 0){
		SelectSubsetElements<Volume>(msel, shPartition, partitionIndex, IS_NORMAL);
		if(createVerticalInterfaces){
			if(partitionForLocalProc)
				SelectUnselectedRootElementsAsVMasters<Volume>(msel);
			else
				SelectSelectedRootElementsAsVSlaves<Volume>(msel);
		}
	}
	else if(mg.num<Face>() > 0){
		SelectSubsetElements<Face>(msel, shPartition, partitionIndex, IS_NORMAL);
		if(createVerticalInterfaces){
			if(partitionForLocalProc)
				SelectUnselectedRootElementsAsVMasters<Face>(msel);
			else
				SelectSelectedRootElementsAsVSlaves<Face>(msel);
		}
	}
	else if(mg.num<EdgeBase>() > 0){
		SelectSubsetElements<EdgeBase>(msel, shPartition, partitionIndex, IS_NORMAL);
		if(createVerticalInterfaces){
			if(partitionForLocalProc)
				SelectUnselectedRootElementsAsVMasters<EdgeBase>(msel);
			else
				SelectSelectedRootElementsAsVSlaves<EdgeBase>(msel);
		}
	}
	else if(mg.num<VertexBase>() > 0){
		SelectSubsetElements<VertexBase>(msel, shPartition, partitionIndex, IS_NORMAL);
		if(createVerticalInterfaces){
			if(partitionForLocalProc)
				SelectUnselectedRootElementsAsVMasters<VertexBase>(msel);
			else
				SelectSelectedRootElementsAsVSlaves<VertexBase>(msel);
		}
	}

	if(mg.num<Volume>() > 0){
		if(createVerticalInterfaces)
			AssignVerticalMasterAndSlaveStates<Volume>(msel);
		AssignSelectionStateToSides<Volume>(msel, true);
	}
	else if(mg.num<Face>() > 0){
		if(createVerticalInterfaces)
			AssignVerticalMasterAndSlaveStates<Face>(msel);
		AssignSelectionStateToSides<Face>(msel, true);
	}
	else if(mg.num<EdgeBase>() > 0){
		if(createVerticalInterfaces)
			AssignVerticalMasterAndSlaveStates<EdgeBase>(msel);
		AssignSelectionStateToSides<EdgeBase>(msel, true);
	}
	else if(mg.num<VertexBase>() > 0){
		if(createVerticalInterfaces)
			AssignVerticalMasterAndSlaveStates<VertexBase>(msel);
	//	no sides to assign...
	}

//	select associated constraining elements first, since they may reference
//	additional unselected constrained elements.
	SelectUnselectedConstrainingElements(msel, IS_DUMMY);
	SelectUnselectedConstrainedElements(msel, IS_DUMMY);
}

////////////////////////////////////////////////////////////////////////////////
template <class TElem>
static void AddTargetProcToDistInfos(MGSelector& msel,
									DistInfoSupplier& distInfos, int targetProc)
{
	typedef typename Grid::traits<TElem>::iterator	TElemIter;

	for(size_t lvl = 0; lvl < msel.num_levels(); ++lvl){
		for(TElemIter iter = msel.begin<TElem>(lvl);
			iter != msel.end<TElem>(lvl); ++iter)
		{
			TElem* e = *iter;
			distInfos.get(e).push_back(
					TargetProcInfo(targetProc, msel.get_selection_status(e)));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
static void FillDistInfos(MultiGrid& mg, SubsetHandler& shPartition, MGSelector& msel,
						DistInfoSupplier& distInfos, std::vector<int>* processMap,
						const pcl::ProcessCommunicator& procComm,
						bool createVerticalInterfaces)
{
	for(int i_part = 0; i_part < shPartition.num_subsets(); ++i_part){

		int targetProc = i_part;
		if(processMap)
			targetProc = (*processMap)[i_part];

		bool localPartition = (targetProc == pcl::GetProcRank());

		msel.clear();
		SelectElementsForTargetPartition(msel, shPartition, i_part,
									 localPartition, createVerticalInterfaces);

	//DEBUG:	temporarily save selection to a file
		#ifdef LG_DISTRIBUTION_DEBUG
		{
			stringstream ss;
			ss << "dist_selection_for_partition_" << i_part << ".ugx";
			SaveDistSelectorToFile(msel, ss.str().c_str());
		}
		#endif

		AddTargetProcToDistInfos<Volume>(msel, distInfos, targetProc);
		AddTargetProcToDistInfos<Face>(msel, distInfos, targetProc);
		AddTargetProcToDistInfos<EdgeBase>(msel, distInfos, targetProc);
		AddTargetProcToDistInfos<VertexBase>(msel, distInfos, targetProc);
	}

#ifdef LG_DISTRIBUTION_DEBUG
	{
		stringstream ss;
		ss << "dist_infos_vrt_before_sync_p" << pcl::GetProcRank() << ".txt";
		WriteDistInfosToTextFile<VertexBase>(mg, distInfos, ss.str().c_str());
	}
#endif

	SynchronizeDistInfos<VertexBase>(mg, distInfos);
	SynchronizeDistInfos<EdgeBase>(mg, distInfos);
	SynchronizeDistInfos<Face>(mg, distInfos);
	SynchronizeDistInfos<Volume>(mg, distInfos);

#ifdef LG_DISTRIBUTION_DEBUG
	{
		stringstream ss;
		ss << "dist_infos_vrt_after_sync_p" << pcl::GetProcRank() << ".txt";
		WriteDistInfosToTextFile<VertexBase>(mg, distInfos, ss.str().c_str());
	}
#endif

}

////////////////////////////////////////////////////////////////////////////////
/**	Based on the list of target processes given by DistInfoSupplier, layouts and
 * interfaces are generated in the given GridLayoutMap.
 *
 * \todo	Think about caching interfaces to speed up this method.
 */
template <class TElem>
static void CreateLayoutsFromDistInfos(MultiGrid& mg, GridLayoutMap& glm,
										DistInfoSupplier& distInfos,
										AGeomObjID& aGID)
{
	typedef typename MultiGrid::traits<TElem>::iterator	TIter;
	typedef typename GridLayoutMap::Types<TElem>::Interface	TInterface;


	int localProcID = pcl::GetProcRank();

	for(size_t lvl = 0; lvl < mg.num_levels(); ++lvl){
		for(TIter iter = mg.begin<TElem>(lvl); iter != mg.end<TElem>(lvl); ++iter)
		{
			TElem* e = *iter;
			vector<TargetProcInfo>& di = distInfos.get(e);

			if(di.size() < 2)
				continue;

		//	get the process with the lowest rank, on which a normal copy of this
		//	element lies (ignore pure vertical masters)
		//	this lowest rank is required to decide, which process a horizontal
		//	master should reside on
			int minProc = pcl::GetNumProcesses();
			int minVSlaveProc = pcl::GetNumProcesses();
			bool isVMaster = false;
			bool isVSlave = false;
			bool isDummy = false;
			//bool isNormal = false;
			bool createNormalHInterface = false;
			int numVSlaveProcs = 0;
			for(size_t i = 0; i < di.size(); ++i){
				TargetProcInfo& tpi = di[i];
				if(tpi.interfaceState & IS_VMASTER){
					if(tpi.procID == localProcID){
						isVMaster = true;
					}
				}
				if(tpi.interfaceState & IS_VSLAVE){
					if(tpi.procID == localProcID)
						isVSlave = true;
					if(tpi.procID < minVSlaveProc)
						minVSlaveProc = tpi.procID;
					++numVSlaveProcs;
				}
				if(tpi.interfaceState & (IS_NORMAL)){
					createNormalHInterface = true;
					//if(tpi.procID == localProcID)
					//	isNormal = true;
				}
				if(tpi.interfaceState & (IS_DUMMY)){
					createNormalHInterface = true;
					if(tpi.procID == localProcID)
						isDummy = true;
				}
				if(tpi.procID < minProc)
					minProc = tpi.procID;
			}

			if(isVMaster && isVSlave){
			//	The element received a copy of a v-slave and a copy of a v-master.
			//	adjacent normal full-dimensional elements should thus exist and a
			//	horizontal interface has to be built.
				createNormalHInterface = true;
			}

			for(size_t i = 0; i < di.size(); ++i){
				TargetProcInfo& tpi = di[i];
				if(tpi.procID == localProcID)
					continue;

			//	add entry to vertical interface if necessary
				if(isVSlave && (tpi.interfaceState & IS_VMASTER)){
					UG_ASSERT(!isDummy, "A dummy element should never lie in a v-interface");
					//UG_ASSERT(!isVMaster, "A v-slave element should never also be a v-master");
					glm.get_layout<TElem>(INT_V_SLAVE).
						interface(tpi.procID, lvl).push_back(e);
				}

				if(isVMaster && (tpi.interfaceState & IS_VSLAVE)){
					UG_ASSERT(!isDummy, "A dummy element should never lie in a v-interface");
					//UG_ASSERT(!isVSlave, "A v-master element should never also be a v-slave");
					glm.get_layout<TElem>(INT_V_MASTER).
						interface(tpi.procID, lvl).push_back(e);
				}

			//	add entry to horizontal interface if necessary
				if(createNormalHInterface){
					if(localProcID == minProc){
					//	horizontal master
						glm.get_layout<TElem>(INT_H_MASTER).
							interface(tpi.procID, lvl).push_back(e);
					}
					else if(tpi.procID == minProc){
					//	horizontal slave
						glm.get_layout<TElem>(INT_H_SLAVE).
							interface(tpi.procID, lvl).push_back(e);
					}
				}
				else if(numVSlaveProcs > 1){
				//	we still have to build a horizontal interface, this time
				//	however only between vertical slaves
					if(tpi.interfaceState & IS_VSLAVE){
						if(!isVMaster){
							if(localProcID == minVSlaveProc){
							//	horizontal master
								glm.get_layout<TElem>(INT_H_MASTER).
									interface(tpi.procID, lvl).push_back(e);
							}
							else if(tpi.procID == minVSlaveProc){
							//	horizontal slave
								glm.get_layout<TElem>(INT_H_SLAVE).
									interface(tpi.procID, lvl).push_back(e);
							}
						}
					}
				}
			}
		}
	}

//	Now sort the interface entries in the different layouts
	CompareByAttachment<TElem, AGeomObjID> gidCmp(mg, aGID);
	if(glm.has_layout<TElem>(INT_H_MASTER))
		glm.get_layout<TElem>(INT_H_MASTER).sort_interface_entries(gidCmp);
	if(glm.has_layout<TElem>(INT_H_SLAVE))
		glm.get_layout<TElem>(INT_H_SLAVE).sort_interface_entries(gidCmp);
	if(glm.has_layout<TElem>(INT_V_MASTER))
		glm.get_layout<TElem>(INT_V_MASTER).sort_interface_entries(gidCmp);
	if(glm.has_layout<TElem>(INT_V_SLAVE))
		glm.get_layout<TElem>(INT_V_SLAVE).sort_interface_entries(gidCmp);
}

////////////////////////////////////////////////////////////////////////////////
bool DistributeGrid(MultiGrid& mg,
					SubsetHandler& shPartition,
					GridDataSerializationHandler& serializer,
					GridDataSerializationHandler& deserializer,
					bool createVerticalInterfaces,
					std::vector<int>* processMap,
					const pcl::ProcessCommunicator& procComm)
{
	UG_STATIC_ASSERT(IS_DUMMY < 256, RedistributeGrid_IS_DUMMY_too_big);

	UG_DLOG(LIB_GRID, 1, "dist-start: DistributeGrid\n");
	const char* errprefix = "ERROR in DistributeGrid: ";

	if(!mg.is_parallel()){
		UG_THROW(errprefix << "Can't distribute a serial grid! Compile ug with -DPARALLEL=ON");
	}

	DistributedGridManager& distGridMgr = *mg.distributed_grid_manager();
	GridLayoutMap& glm = distGridMgr.grid_layout_map();

//	The selector will be of frequent use to speed up some algorithms
	MGSelector msel(mg);

//	Since we will change huge parts of the underlying grid and the grid-layout-map,
//	we'll disable auto-insertion of elements in the distributed-grid-manager.
//	This means we carefully have to take care of all interface changes.
	distGridMgr.enable_interface_management(false);

////////////////////////////////
//	GLOBAL IDS
	PCL_PROFILE(redist_CreateGlobalIDs);
//todo:	only create global ids if they aren't already present
	CreateAndDistributeGlobalIDs<VertexBase>(mg, glm);
	CreateAndDistributeGlobalIDs<EdgeBase>(mg, glm);
	CreateAndDistributeGlobalIDs<Face>(mg, glm);
	CreateAndDistributeGlobalIDs<Volume>(mg, glm);
	MultiElementAttachmentAccessor<AGeomObjID> aaID(mg, aGeomObjID);
	PCL_PROFILE_END();

	#ifdef LG_DISTRIBUTION_DEBUG
	{
		UG_LOG("DEBUG: WRITING GLOBAL VERTEX IDS TO FILE\n");
		stringstream ss;
		ss << "global_ids_vrt_p" << pcl::GetProcRank() << ".txt";
		WriteDebugValuesToFile<VertexBase>(ss.str().c_str(), mg, aGeomObjID, false);
	}
	{
		UG_LOG("DEBUG: WRITING GLOBAL EDGE IDS TO FILE\n");
		stringstream ss;
		ss << "global_ids_edge_p" << pcl::GetProcRank() << ".txt";
		WriteDebugValuesToFile<EdgeBase>(ss.str().c_str(), mg, aGeomObjID, false);
	}
	{
		UG_LOG("DEBUG: WRITING GLOBAL FACE IDS TO FILE\n");
		stringstream ss;
		ss << "global_ids_face_p" << pcl::GetProcRank() << ".txt";
		WriteDebugValuesToFile<Face>(ss.str().c_str(), mg, aGeomObjID, false);
	}
	#endif

////////////////////////////////
//	FILL THE DISTRIBUTION INFOS (INVOLVES COMMUNICATION...)
	DistInfoSupplier distInfos(mg);
	FillDistInfos(mg, shPartition, msel, distInfos, processMap, procComm,
				  createVerticalInterfaces);

//	DEBUG: output distInfos...
	#ifdef LG_DISTRIBUTION_DEBUG
	{
		stringstream ss;
		ss << "dist_infos_before_distribution_p_" << pcl::GetProcRank() << ".ugx";
		SaveDistInfosToFile(mg, distInfos, ss.str().c_str());
	}
	#endif

////////////////////////////////
//	COMMUNICATE INVOLVED PROCESSES
	UG_DLOG(LIB_GRID, 2, "dist-DistributeGrid: CommunicateInvolvedProcesses\n");
//	each process has to know with which other processes it
//	has to communicate.
	vector<int> sendToRanks, recvFromRanks, sendPartitionInds;

//	for each subset which is not emtpy we'll have to send data to
//	the associated process.
	for(int si = 0; si < shPartition.num_subsets(); ++si){
		if(!shPartition.empty(si)){
			int toProc = si;
		//	if a process map exists, we'll use the associated process
			if(processMap)
				toProc = processMap->at(si);

			sendToRanks.push_back(toProc);
			sendPartitionInds.push_back(si);
		}
	}

	pcl::CommunicateInvolvedProcesses(recvFromRanks, sendToRanks, procComm);

////////////////////////////////
//	SERIALIZE THE GRID, THE GLOBAL IDS AND THE DISTRIBUTION INFOS.
	AInt aLocalInd;
	mg.attach_to_all(aLocalInd);
	MultiElementAttachmentAccessor<AInt> aaInt(mg, aLocalInd);

//	out and sendSegSizes will be used to distribute the grid.
	BinaryBuffer out;
	vector<int> outSegSizes;

//	the magic number is used for debugging to make sure that the stream is read correctly
	int magicNumber1 = 75234587;
	int magicNumber2 = 560245;

	ADistInfo aDistInfo = distInfos.dist_info_attachment();
	GeomObjAttachmentSerializer<VertexBase, ADistInfo>	distInfoSerializerVRT(mg, aDistInfo);
	GeomObjAttachmentSerializer<EdgeBase, ADistInfo>	distInfoSerializerEDGE(mg, aDistInfo);
	GeomObjAttachmentSerializer<Face, ADistInfo>		distInfoSerializerFACE(mg, aDistInfo);
	GeomObjAttachmentSerializer<Volume, ADistInfo>		distInfoSerializerVOL(mg, aDistInfo);

	GridDataSerializationHandler distInfoSerializer;
	distInfoSerializer.add(&distInfoSerializerVRT);
	distInfoSerializer.add(&distInfoSerializerEDGE);
	distInfoSerializer.add(&distInfoSerializerFACE);
	distInfoSerializer.add(&distInfoSerializerVOL);

//	now perform the serialization
	for(size_t i_to = 0; i_to < sendPartitionInds.size(); ++i_to){
		int partInd = sendPartitionInds[i_to];
		bool localPartition = (sendToRanks[i_to] == pcl::GetProcRank());

	//	the last size is required to calculate the size of the new segment
		size_t oldSize = out.write_pos();

	//	write a magic number for debugging purposes
		out.write((char*)&magicNumber1, sizeof(int));

	//	select the elements of the current partition
		msel.clear();
		SelectElementsForTargetPartition(msel, shPartition, partInd,
									 localPartition, createVerticalInterfaces);

		SerializeMultiGridElements(mg, msel.get_geometric_objects(), aaInt, out, &aaID);


	//	serialize associated data
		distInfoSerializer.write_infos(out);
		distInfoSerializer.serialize(out, msel.get_geometric_objects());
		serializer.write_infos(out);
		serializer.serialize(out, msel.get_geometric_objects());

	//	write a magic number for debugging purposes
		out.write((char*)&magicNumber2, sizeof(int));

	//	size of the segment we just wrote to out
		outSegSizes.push_back((int)(out.write_pos() - oldSize));
	}
	PCL_PROFILE_END();


////////////////////////////////
//	COMMUNICATE SERIALIZED DATA
	UG_DLOG(LIB_GRID, 2, "dist-DistributeGrid: Distribute data\n");
	PCL_PROFILE(dist_CommunicateSerializedData);
//	now distribute the packs between involved processes
	BinaryBuffer in;
	vector<int> inSegSizes(recvFromRanks.size());

	procComm.distribute_data(in, GetDataPtr(inSegSizes),
							GetDataPtr(recvFromRanks), (int)recvFromRanks.size(),
							out.buffer(), GetDataPtr(outSegSizes),
							GetDataPtr(sendToRanks), (int)sendToRanks.size());

	PCL_PROFILE_END();

////////////////////////////////
//	INTERMEDIATE CLEANUP
	UG_DLOG(LIB_GRID, 2, "dist-DistributeGrid: Intermediate cleanup\n");
	PCL_PROFILE(dist_ClearLocalGrid);
//	we'll erase everything and deserialize even the local grid from stream
	mg.clear_geometry();
	glm.clear();
	PCL_PROFILE_END();

////////////////////////////////
//	DESERIALIZE INCOMING GRIDS
	vector<VertexBase*>	vrts;
	vector<EdgeBase*> edges;
	vector<Face*> faces;
	vector<Volume*> vols;

	for(size_t i = 0; i < recvFromRanks.size(); ++i){
		UG_DLOG(LIB_GRID, 2, "Deserializing from rank " << recvFromRanks[i] << "\n");

	//	read the magic number and make sure that it matches our magicNumber
		int tmp = 0;
		in.read((char*)&tmp, sizeof(int));
		if(tmp != magicNumber1){
			UG_THROW("ERROR in RedistributeGrid: "
					 "Magic number mismatch before deserialization.\n");
		}

		DeserializeMultiGridElements(mg, in, &vrts, &edges, &faces, &vols, &aaID);

	//	deserialize the associated data (global ids have already been deserialized)
		distInfoSerializer.read_infos(in);
		distInfoSerializer.deserialize(in, vrts.begin(), vrts.end());
		distInfoSerializer.deserialize(in, edges.begin(), edges.end());
		distInfoSerializer.deserialize(in, faces.begin(), faces.end());
		distInfoSerializer.deserialize(in, vols.begin(), vols.end());

		deserializer.read_infos(in);
		deserializer.deserialize(in, vrts.begin(), vrts.end());
		deserializer.deserialize(in, edges.begin(), edges.end());
		deserializer.deserialize(in, faces.begin(), faces.end());
		deserializer.deserialize(in, vols.begin(), vols.end());

	//	read the magic number and make sure that it matches our magicNumber
		tmp = 0;
		in.read((char*)&tmp, sizeof(int));
		if(tmp != magicNumber2){
			UG_THROW("ERROR in RedistributeGrid: "
					 "Magic number mismatch after deserialization.\n");
		}
	}

//	DEBUG: output distInfos...
	#ifdef LG_DISTRIBUTION_DEBUG
	{
		stringstream ss;
		ss << "dist_infos_after_distribution_p_" << pcl::GetProcRank() << ".ugx";
		SaveDistInfosToFile(mg, distInfos, ss.str().c_str());
	}
	#endif

////////////////////////////////
//	CREATE LAYOUTS
	CreateLayoutsFromDistInfos<VertexBase>(mg, glm, distInfos, aGeomObjID);
	CreateLayoutsFromDistInfos<EdgeBase>(mg, glm, distInfos, aGeomObjID);
	CreateLayoutsFromDistInfos<Face>(mg, glm, distInfos, aGeomObjID);
	CreateLayoutsFromDistInfos<Volume>(mg, glm, distInfos, aGeomObjID);


////////////////////////////////
//	UPDATE THE DISTRIBUTED GRID MANAGER
	UG_DLOG(LIB_GRID, 2, "dist-DistributeGrid: Update DistributedGridManager\n");
	PCL_PROFILE(redist_UpdateDistGridManager);
	glm.remove_empty_interfaces();
	distGridMgr.enable_interface_management(true);
	distGridMgr.grid_layouts_changed(false);
	PCL_PROFILE_END();

	mg.detach_from_all(aLocalInd);

	UG_DLOG(LIB_GRID, 1, "dist-stop: DistributeGrid\n");
	return true;
}

}// end of namespace