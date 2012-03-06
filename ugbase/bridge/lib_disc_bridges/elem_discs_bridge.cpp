/*
 * elem_discs_bridge.cpp
 *
 *  Created on: 20.05.2011
 *      Author: andreasvogel
 */

// extern headers
#include <iostream>
#include <sstream>
#include <string>

// include bridge
#include "../bridge.h"
#include "registry/registry.h"

// lib_disc includes
#include "lib_disc/domain.h"

#include "lib_disc/spatial_disc/elem_disc/thermohaline_flow/thermohaline_flow.h"
#include "lib_disc/spatial_disc/disc_util/conv_shape_interface.h"
#include "lib_disc/spatial_disc/disc_util/conv_shape.h"

#include "lib_disc/spatial_disc/elem_disc/density_driven_flow/density_driven_flow.h"
#include "lib_disc/spatial_disc/elem_disc/convection_diffusion/convection_diffusion.h"
#include "lib_disc/spatial_disc/elem_disc/constant_equation/constant_equation.h"

#include "lib_disc/spatial_disc/elem_disc/neumann_boundary/neumann_boundary.h"
#include "lib_disc/spatial_disc/elem_disc/inner_boundary/inner_boundary.h"
#include "lib_disc/spatial_disc/elem_disc/inner_boundary/FV1CalciumERElemDisc.h"
#include "lib_disc/spatial_disc/elem_disc/linear_elasticity/fe1_linear_elasticity.h"

using namespace std;

namespace ug
{

namespace bridge
{

////////////////////////////////////////////////////////////////////////////////
//	Registering for all implementations of IElemDisc
////////////////////////////////////////////////////////////////////////////////

template <typename TDomain>
void Register__Domain(Registry& reg, string grp)
{
//	dimension of domain
	static const int dim = TDomain::dim;

//	suffix and tag
	string dimSuffix = GetDomainSuffix<dim>();
	string dimTag = GetDomainTag<dim>();

	string approxGrp = grp; approxGrp.append("/ApproximationSpace");

//  ApproximationSpace
	{
		typedef ApproximationSpace<TDomain> T;
		typedef IApproximationSpace TBase;
		string name = string("ApproximationSpace").append(dimSuffix);
		reg.add_class_<T, TBase>(name, approxGrp)
			.template add_constructor<void (*)(SmartPtr<TDomain>)>("Domain")
			.add_method("domain", static_cast<SmartPtr<TDomain> (T::*)()>(&T::domain))
			.add_method("surface_view", static_cast<ConstSmartPtr<SurfaceView> (T::*)() const>(&T::surface_view))
			.set_construct_as_smart_pointer(true);
		reg.add_class_to_group(name, "ApproximationSpace", dimTag);
	}

	string elemGrp = grp; elemGrp.append("/SpatialDisc/ElemDisc");

//	DomainElemDisc base class
	{
		typedef IDomainElemDisc<TDomain> T;
		typedef IElemDisc TBase;
		string name = string("IDomainElemDisc").append(dimSuffix);
		reg.add_class_<T, TBase >(name, elemGrp);
		reg.add_class_to_group(name, "IDomainElemDisc", dimTag);
	}

//	Neumann Boundary
	{
		typedef boost::function<bool (number& value, const MathVector<dim>& x, number time)> BNDNumberFunctor;
		typedef boost::function<void (MathVector<dim>& value, const MathVector<dim>& x, number time)> VectorFunctor;
		typedef FV1NeumannBoundaryElemDisc<TDomain> T;
		typedef IDomainElemDisc<TDomain> TBase;
		string name = string("FV1NeumannBoundary").append(dimSuffix);
		reg.add_class_<T, TBase >(name, elemGrp)
			.template add_constructor<void (*)(const char*)>("Function(s)")
			.add_method("add", static_cast<void (T::*)(BNDNumberFunctor&, const char*, const char*)>(&T::add))
			.add_method("add", static_cast<void (T::*)(VectorFunctor&, const char*, const char*)>(&T::add));
		reg.add_class_to_group(name, "FV1NeumannBoundary", dimTag);
	}

//	Inner Boundaries
	{
		typedef FV1InnerBoundaryElemDisc<TDomain> T;
		typedef IDomainElemDisc<TDomain> TBase;
		string name = string("FV1InnerBoundary").append(dimSuffix);
		reg.add_class_<T, TBase >(name, elemGrp);
		reg.add_class_to_group(name, "FV1InnerBoundary", dimTag);
	
		typedef FV1CalciumERElemDisc<TDomain> T1;
		typedef FV1InnerBoundaryElemDisc<TDomain> TBase1;
		name = string("FV1InnerBoundaryCalciumER").append(dimSuffix);
		reg.add_class_<T1, TBase1>(name, elemGrp)
			.template add_constructor<void (*)(const char*, const char*)>("Function(s)#Subset(s)");
		reg.add_class_to_group(name, "FV1InnerBoundaryCalciumER", dimTag);
	
	}

//	Constant Equation Finite Volume
	{
		typedef FVConstantEquationElemDisc<TDomain> T;
		typedef IDomainElemDisc<TDomain> TBase;
		string name = string("FV1ConstantEquation").append(dimSuffix);
		reg.add_class_<T, TBase >(name, elemGrp)
			.template add_constructor<void (*)(const char*,const char*)>("Function(s)#Subset(s)")
			.add_method("set_velocity", &T::set_velocity)
			.add_method("set_source", &T::set_source)
			.add_method("set_mass_scale", &T::set_mass_scale)
			.add_method("value", &T::value)
			.add_method("gradient", &T::gradient);
		reg.add_class_to_group(name, "FV1ConstantEquation", dimTag);
	}

//	Convection Diffusion
	{
		typedef ConvectionDiffusionElemDisc<TDomain> T;
		typedef IDomainElemDisc<TDomain> TBase;
		string name = string("ConvectionDiffusion").append(dimSuffix);
		reg.add_class_<T, TBase >(name, elemGrp)
			.template add_constructor<void (*)(const char*,const char*)>("Function(s)#Subset(s)")
			.add_method("set_disc_scheme", &T::set_disc_scheme, "", "Disc Scheme|selection|value=[\"fe\",\"fv\",\"fv1\"]")
			.add_method("set_quad_order", &T::set_quad_order)
			.add_method("set_quad_order_scvf", &T::set_quad_order_scvf)
			.add_method("set_quad_order_scv", &T::set_quad_order_scv)
			.add_method("set_diffusion_tensor", &T::set_diffusion, "", "Diffusion")
			.add_method("set_velocity_field", &T::set_velocity, "", "Velocity Field")
			.add_method("set_reaction_rate", &T::set_reaction_rate, "", "Reaction Rate")
			.add_method("set_source", &T::set_source, "", "Source")
			.add_method("set_mass_scale", &T::set_mass_scale, "", "Mass Scale")
			.add_method("set_upwind", &T::set_upwind)
			.add_method("value", &T::value)
			.add_method("gradient", &T::gradient);
		reg.add_class_to_group(name, "ConvectionDiffusion", dimTag);
	}

//	Density Driven Flow
	{
		typedef DensityDrivenFlowElemDisc<TDomain> T2;
		typedef IDomainElemDisc<TDomain> TBase;
		string name = string("DensityDrivenFlow").append(dimSuffix);
		reg.add_class_<T2, TBase >(name, elemGrp)
			.template add_constructor<void (*)(const char*,const char*)>("Function(s)#Subset(s)")
			.add_method("set_upwind", &T2::set_upwind,
						"", "Upwind (no, part, full)")
			.add_method("set_boussinesq_transport", &T2::set_boussinesq_transport,
						"", "Boussinesq Transport")
			.add_method("set_boussinesq_flow", &T2::set_boussinesq_flow,
						"", "Boussinesq Flow")
			.add_method("set_porosity", &T2::set_porosity,
						"", "Porosity")
			.add_method("set_gravity", &T2::set_gravity,
						"", "Gravity")
			.add_method("set_permeability", &T2::set_permeability,
						"", "Permeability")
			.add_method("set_viscosity", &T2::set_viscosity,
						"", "Viscosity")
			.add_method("set_molecular_diffusion", &T2::set_molecular_diffusion,
						"", "Molecular Diffusion")
			.add_method("set_density", &T2::set_density,
						"", "Density")
			.add_method("set_consistent_gravity", &T2::set_consistent_gravity,
						"", "Consistent Gravity")
			.add_method("get_darcy_velocity", &T2::get_darcy_velocity)
			.add_method("get_brine", &T2::get_brine);
		reg.add_class_to_group(name, "DensityDrivenFlow", dimTag);
	}

//	Thermohaline Flow
	{
		typedef ThermohalineFlowElemDisc<TDomain> T2;
		typedef IDomainElemDisc<TDomain> TBase;
		string name = string("FV1ThermohalineFlow").append(dimSuffix);
		reg.add_class_<T2, TBase >(name, elemGrp)
			.template add_constructor<void (*)(const char*,const char*)>("Function(s)#Subset(s)")
			.add_method("set_upwind", &T2::set_upwind,
						"", "Upwind (no, part, full)")
			.add_method("set_upwind_energy", &T2::set_upwind_energy,
						"", "Upwind (no, part, full)")
			.add_method("set_boussinesq_transport", &T2::set_boussinesq_transport,
						"", "Boussinesq Transport")
			.add_method("set_boussinesq_flow", &T2::set_boussinesq_flow,
						"", "Boussinesq Flow")
			.add_method("set_boussinesq_density", &T2::set_boussinesq_density,
						"", "Boussinesq Density")
			.add_method("set_porosity", &T2::set_porosity,
						"", "Porosity")
			.add_method("set_gravity", &T2::set_gravity,
						"", "Gravity")
			.add_method("set_permeability", &T2::set_permeability,
						"", "Permeability")
			.add_method("set_viscosity", &T2::set_viscosity,
						"", "Viscosity")
			.add_method("set_thermal_conductivity", &T2::set_thermal_conductivity,
						"", "Molecular Diffusion")
			.add_method("set_molecular_diffusion", &T2::set_molecular_diffusion,
						"", "Molecular Diffusion")
			.add_method("set_density", &T2::set_density,
						"", "Density")
			.add_method("set_consistent_gravity", &T2::set_consistent_gravity,
						"", "Consistent Gravity")
			.add_method("set_heat_capacity_fluid", &T2::set_heat_capacity_fluid)
			.add_method("set_heat_capacity_solid", &T2::set_heat_capacity_solid)
			.add_method("set_mass_density_solid", &T2::set_mass_density_solid)
			.add_method("get_darcy_velocity", &T2::get_darcy_velocity)
			.add_method("get_pressure_grad", &T2::get_pressure_grad)
			.add_method("get_temperature", &T2::get_temperature)
			.add_method("get_brine", &T2::get_brine);
		reg.add_class_to_group(name, "FV1ThermohalineFlow", dimTag);
	}

/////////////////////////////////////////////////////////////////////////////
// Convection Shapes
/////////////////////////////////////////////////////////////////////////////

	string upGrp = grp; upGrp.append("/SpatialDisc/Upwind");

//	IConvectionShapes
	{
		typedef IConvectionShapes<dim> T;
		string name = string("IConvectionShapes").append(dimSuffix);
		reg.add_class_<T>(name, upGrp);
		reg.add_class_to_group(name, "IConvectionShapes", dimTag);
	}

//	ConvectionShapesNoUpwind
	{
		typedef ConvectionShapesNoUpwind<dim> T;
		typedef IConvectionShapes<dim> TBase;
		string name = string("NoUpwind").append(dimSuffix);
		reg.add_class_<T, TBase>(name, upGrp)
			.add_constructor();
		reg.add_class_to_group(name, "NoUpwind", dimTag);
	}

//	ConvectionShapesFullUpwind
	{
		typedef ConvectionShapesFullUpwind<dim> T;
		typedef IConvectionShapes<dim> TBase;
		string name = string("FullUpwind").append(dimSuffix);
		reg.add_class_<T, TBase>(name, upGrp)
			.add_constructor();
		reg.add_class_to_group(name, "FullUpwind", dimTag);
	}

//	ConvectionShapesWeightedUpwind
	{
		typedef ConvectionShapesWeightedUpwind<dim> T;
		typedef IConvectionShapes<dim> TBase;
		string name = string("WeightedUpwind").append(dimSuffix);
		reg.add_class_<T, TBase>(name, upGrp)
			.add_method("set_weight", &T::set_weight)
			.add_constructor();
		reg.add_class_to_group(name, "WeightedUpwind", dimTag);
	}

//	ConvectionShapesPartialUpwind
	{
		typedef ConvectionShapesPartialUpwind<dim> T;
		typedef IConvectionShapes<dim> TBase;
		string name = string("PartialUpwind").append(dimSuffix);
		reg.add_class_<T, TBase>(name, upGrp)
			.add_constructor();
		reg.add_class_to_group(name, "PartialUpwind", dimTag);
	}
}


bool RegisterElemDiscs(Registry& reg, string parentGroup)
{
	{
		typedef IApproximationSpace T;
	reg.add_class_<T>("IApproximationSpace")
		.add_method("print_statistic", static_cast<void (T::*)(int) const>(&T::print_statistic))
		.add_method("print_statistic", static_cast<void (T::*)() const>(&T::print_statistic))
		.add_method("print_layout_statistic", static_cast<void (T::*)(int) const>(&T::print_layout_statistic))
		.add_method("print_layout_statistic", static_cast<void (T::*)() const>(&T::print_layout_statistic))
		.add_method("print_local_dof_statistic", static_cast<void (T::*)(int) const>(&T::print_local_dof_statistic))
		.add_method("print_local_dof_statistic", static_cast<void (T::*)() const>(&T::print_local_dof_statistic))
		.add_method("init_levels", &T::init_levels)
		.add_method("init_surfaces", &T::init_surfaces)
		.add_method("init_top_surface", &T::init_top_surface)
		.add_method("defragment", &T::defragment)
		.add_method("clear", &T::clear)
		.add_method("add_fct", static_cast<void (T::*)(const char*, const char*, int, const char*)>(&T::add_fct),
					"", "Name # Type|selection|value=[\"Lagrange\",\"DG\"] # Order # Subsets", "Adds a function to the Function Pattern",
					"currently no help available")
		.add_method("add_fct", static_cast<void (T::*)(const char*, const char*, int)>(&T::add_fct),
					"", "Name # Type|selection|value=[\"Lagrange\",\"DG\"] # Order", "Adds a function to the Function Pattern",
					"currently no help available");

	}

//	get group string
	string grp = parentGroup; grp.append("/Discretization");
	try
	{
#ifdef UG_DIM_1
		Register__Domain<Domain1d>(reg, grp);
#endif
#ifdef UG_DIM_2
			Register__Domain<Domain2d>(reg, grp);
#endif
#ifdef UG_DIM_3
			Register__Domain<Domain3d>(reg, grp);
#endif
	}
	catch(UG_REGISTRY_ERROR_RegistrationFailed ex)
	{
		UG_LOG("### ERROR in RegisterElemDiscs: "
				"Registration failed (using name " << ex.name << ").\n");
		UG_THROW_FATAL("Registration failed.");
	}

	return true;
}

}//	end of namespace ug
}//	end of namespace interface
