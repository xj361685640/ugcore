// created by Andreas Vogel, Sebastian Reiter
// s.b.reiter@googlemail.com
// 10.02.2011 (m,d,y)

#include <iostream>
#include <sstream>
#include <vector>

#include "registry/registry.h"
#include "bridge.h"

#include "common/profiler/profiler.h"
#include "lib_grid/lib_grid.h"

#include "lib_disc/domain.h"
#include "lib_disc/domain_util.h"

#include "lib_disc/parallelization/domain_distribution.h"

#ifdef UG_PARALLEL
	#include "lib_grid/parallelization/load_balancing.h"
	#include "lib_grid/parallelization/parallelization.h"
#endif

// ONLY TEMPORARY
#include "lib_grid/visualization/grid_visualization.h"

using namespace std;

namespace ug{

//	This method is only a temporary test method and will be replaced by
//	a more sophisticated approach
template <typename TDomain>
static void MinimizeMemoryFootprint(TDomain& dom)
{
	dom.get_grid().set_options(GRIDOPT_VERTEXCENTRIC_INTERCONNECTION
							 | GRIDOPT_AUTOGENERATE_SIDES);
}

template <typename TDomain>
static bool LoadDomain(TDomain& domain, const char* filename)
{
#ifdef UG_PARALLEL
	if(pcl::GetProcRank() != 0)
		return true;
#endif

	if(!LoadGridFromFile(domain.get_grid(), domain.get_subset_handler(),
						 filename, domain.get_position_attachment()))
	{
		UG_LOG("Cannot load grid.\n");
		return false;
	}

	return true;
}

template <typename TDomain>
static bool LoadAndRefineDomain(TDomain& domain, const char* filename,
								int numRefs)
{
#ifdef UG_PARALLEL
	if(pcl::GetProcRank() != 0)
		return true;
#endif

	if(!LoadGridFromFile(domain.get_grid(), domain.get_subset_handler(),
						 filename, domain.get_position_attachment()))
	{
		UG_LOG("Cannot load grid.\n");
		return false;
	}

	GlobalMultiGridRefiner ref(domain.get_grid());
	for(int i = 0; i < numRefs; ++i)
		ref.refine();

	return true;
}

template <typename TDomain>
static bool SaveDomain(TDomain& domain, const char* filename)
{
	return SaveGridToFile(domain.get_grid(), domain.get_subset_handler(),
						  filename, domain.get_position_attachment());
}

template <typename TDomain>
static bool SavePartitionMap(PartitionMap& pmap, TDomain& domain,
							 const char* filename)
{
	if(&domain.get_grid() != pmap.get_partition_handler().get_assigned_grid())
	{
		UG_LOG("WARNING in SavePartitionMap: The given partition map was not"
				" created for the given domain. Aborting...\n");
		return false;
	}

	return SavePartitionMapToFile(pmap, filename, domain.get_position_attachment());
}


template <typename TDomain>
static bool TestDomainInterfaces(TDomain* dom)
{
	#ifdef UG_PARALLEL
		return TestGridLayoutMap(dom->get_grid(),
					dom->get_distributed_grid_manager()->grid_layout_map());
	#endif
	return true;
}

//	ONLY TEMPORARY!!! DO NOT USE THIS METHOD!!!
template <typename TDomain>
static void TestDomainVisualization(TDomain& dom)
{
	GridVisualization<number, int, typename TDomain::position_attachment_type> gridVis;
	gridVis.set_grid(dom.get_grid(), dom.get_position_attachment());
	gridVis.update_visuals();

//	log vertex positions
	UG_LOG("GridVisualization:\n");
	UG_LOG("Vertices (" << gridVis.num_vertices() << "):");
	const number* pos = gridVis.vertex_positions();
	for(int i = 0; i < gridVis.num_vertices(); ++i){
		UG_LOG(" (" <<  pos[3*i] << ", " << pos[3*i+1] << ", " << pos[3*i+2] << ")");
	}
	UG_LOG("\n\n");

//	iterate over all visuals
	for(int i_vis = 0; i_vis < gridVis.num_visuals(); ++i_vis){
		UG_LOG("Visual " << i_vis << ":\n");
	//	log the triangles of this visual (triple of vertex indices.)
		UG_LOG("Triangles:");
		const int* tris = gridVis.triangle_list(i_vis);
		for(int i = 0; i < gridVis.num_triangles(i_vis); ++i){
			UG_LOG(" [" <<  tris[3*i] << ", " << tris[3*i+1] << ", " << tris[3*i+2] << "]");
		}
		UG_LOG("\n\n");

	//	log the triangle normals of this visual
		UG_LOG("Tri-Normals:");
		const number* norms = gridVis.face_normals(i_vis);

		for(int i = 0; i < gridVis.num_triangles(i_vis); ++i){
			UG_LOG(" (" <<  norms[3*i] << ", " << norms[3*i+1] << ", " << norms[3*i+2] << ")");
		}
		UG_LOG("\n\n");
	}
}


namespace bridge{

template <typename TDomain>
static bool RegisterDomainInterface_(Registry& reg, string grp)
{
//	the dimension suffix
	string dimSuffix = GetDomainSuffix<TDomain>();

//	the dimension tag
	string dimTag = GetDomainTag<TDomain>();

//	Domain
	{
		string name = string("Domain").append(dimSuffix);
		reg.add_class_<TDomain>(name, grp)
			.add_constructor()
			.add_method("get_subset_handler|hide=true", static_cast<MGSubsetHandler& (TDomain::*)()>(&TDomain::get_subset_handler))
			.add_method("get_grid|hide=true", static_cast<MultiGrid& (TDomain::*)()>(&TDomain::get_grid))
			.add_method("get_dim|hide=true", static_cast<int (TDomain::*)() const>(&TDomain::get_dim));

		reg.add_class_to_group(name, "Domain", dimTag);
	}

// 	LoadDomain
	reg.add_function("LoadDomain", &LoadDomain<TDomain>, grp,
					"Success", "Domain # Filename | load-dialog | endings=[\"ugx\"]; description=\"*.ugx-Files\" # Number Refinements",
					"Loads a domain", "No help");

//	LoadAndRefineDomain
	reg.add_function("LoadAndRefineDomain", &LoadAndRefineDomain<TDomain>, grp,
					"Success", "Domain # Filename # NumRefines | load-dialog | endings=[\"ugx\"]; description=\"*.ugx-Files\" # Number Refinements",
					"Loads a domain and performs global refinement", "No help");
//	SaveDomain
	reg.add_function("SaveDomain", &SaveDomain<TDomain>, grp,
					"Success", "Domain # Filename|save-dialog",
					"Saves a domain", "No help");

//	SavePartitionMap
	reg.add_function("SavePartitionMap", &SavePartitionMap<TDomain>, grp,
					"Success", "PartitionMap # Domain # Filename|save-dialog",
					"Saves a partition map", "No help");

//	DistributeDomain
	reg.add_function("DistributeDomain", static_cast<bool (*)(TDomain&)>(
					 &DistributeDomain<TDomain>), grp);

	reg.add_function("DistributeDomain", static_cast<bool (*)(TDomain&, PartitionMap&)>(
					 &DistributeDomain<TDomain>), grp);

//	todo: remove this
	{
		string name = string("DistributeDomain").append(dimSuffix);
		reg.add_function(name.c_str(), static_cast<bool (*)(TDomain&)>(
						 &DistributeDomain<TDomain>), grp);
	}

	reg.add_function("PartitionDomain_Bisection",
					 &PartitionDomain_Bisection<TDomain>, grp);

	reg.add_function("PartitionDomain_MetisKWay",
					 &PartitionDomain_MetisKWay<TDomain>, grp);

	reg.add_function("RedistributeDomain",
					 &RedistributeDomain<TDomain>, grp);

//	debugging
	reg.add_function("TestDomainInterfaces", &TestDomainInterfaces<TDomain>, grp);

//	ONLY TEMPORARY
	reg.add_function("TestDomainVisualization", &TestDomainVisualization<TDomain>, grp);
	reg.add_function("MinimizeMemoryFootprint", &MinimizeMemoryFootprint<TDomain>, grp);

	return true;
}

///	methods that are only available for 2d and 3d are registered here
template <typename TDomain>
static bool RegisterDomainInterface_2d_3d(Registry& reg, string grp)
{
	reg.add_function("PartitionDomain_RegularGrid",
					 &PartitionDomain_RegularGrid<TDomain>, grp);

	return true;
}

bool RegisterDomainInterface(Registry& reg, string parentGroup)
{
	bool bSuccess = true;

	string grp = parentGroup; grp.append("/Domain");

#ifdef UG_DIM_1
	bSuccess &= RegisterDomainInterface_<Domain<1, MultiGrid, MGSubsetHandler> >(reg, grp);
#endif
#ifdef UG_DIM_2
	bSuccess &= RegisterDomainInterface_<Domain<2, MultiGrid, MGSubsetHandler> >(reg, grp);
	bSuccess &= RegisterDomainInterface_2d_3d<Domain<2, MultiGrid, MGSubsetHandler> >(reg, grp);
#endif
#ifdef UG_DIM_3
	bSuccess &= RegisterDomainInterface_<Domain<3, MultiGrid, MGSubsetHandler> >(reg, grp);
	bSuccess &= RegisterDomainInterface_2d_3d<Domain<3, MultiGrid, MGSubsetHandler> >(reg, grp);
#endif
	return bSuccess;
}

}// end of namespace
}// end of namespace
