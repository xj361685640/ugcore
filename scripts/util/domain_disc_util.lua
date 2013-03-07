--[[!
\file scripts/util/domain_disc_util.lua
\brief	functions to create DomainDiscs using a string disc-type identifier
]]--


--!	Returns a ConvectionDiffusion Element-Disc of the requested type
--! @return Returns the domain discreatization
--! @param fcts (String) names of symbolic functions 
--! @param subsets (String) names of symbolic subsets 
--! @param discType (String) discretizatin scheme 
function ConvectionDiffusion(fcts, subsets, discType)
	if discType == nil then discType = "fv1" end
	if 		discType == "fv1"  then return ConvectionDiffusionFV1(fcts, subsets)
	elseif  discType == "fe"   then return ConvectionDiffusionFE(fcts, subsets)
	elseif  discType == "fvcr" then return ConvectionDiffusionFVCR(fcts, subsets)
	elseif  discType == "fv"   then return ConvectionDiffusionFV(fcts, subsets)
	else 
		print("ConvectionDiffusion: no disc type '"..discType.."' available. Aborting")
		exit();
	end
end

--!	Returns a ConstantEquation Element-Disc of the requested type
--! @return Returns the domain discreatization
--! @param fcts (String) names of symbolic functions 
--! @param subsets (String) names of symbolic subsets 
--! @param discType (String) discretizatin scheme 
function ConstantEquation(fcts, subsets, discType)
	if discType == nil then discType = "fv1" end
	if 		discType == "fv1"  then return ConstantEquationFV1(fcts, subsets)
	elseif  discType == "fe"   then return ConstantEquationFE(fcts, subsets)
	elseif  discType == "fv"   then return ConstantEquationFV(fcts, subsets)
	else 
		print("ConstantEquation: no disc type '"..discType.."' available. Aborting")
		exit();
	end
end

--!	Returns a NeumannBoundary Element-Disc of the requested type
--! @return Returns the domain discreatization
--! @param fcts (String) names of symbolic function 
--! @param discType (String) discretizatin scheme 
function NeumannBoundary(fcts, discType)
	if discType == nil then discType = "fv1" end
	if 		discType == "fv1"  then return NeumannBoundaryFV1(fcts)
	elseif  discType == "fv"   then return NeumannBoundaryFV(fcts)
	else 
		print("ConstantEquation: no disc type '"..discType.."' available. Aborting")
		exit();
	end
end

--!	Returns a DensityDrivenFlow Element-Disc of the requested type
--! @return Returns the domain discreatization
--! @param fcts (String) names of symbolic functions 
--! @param subsets (String) names of symbolic subsets 
--! @param discType (String) discretizatin scheme 
function DensityDrivenFlow(fcts, subsets, discType)
	if discType == nil then discType = "fv1" end
	if 		discType == "fv1"  then return DensityDrivenFlowFV1(fcts, subsets)
	elseif  discType == "fv"   then return DensityDrivenFlowFV(fcts, subsets)
	else 
		print("DensityDrivenFlow: no disc type '"..discType.."' available. Aborting")
		exit();
	end
end

--!	Returns a NavierStokes Element-Disc of the requested type
--! @return Returns the domain discreatization
--! @param fcts (String) names of symbolic functions 
--! @param subsets (String) names of symbolic subsets 
--! @param discType (String) discretizatin scheme 
function NavierStokes(fcts, subsets, discType)
	if discType == nil then discType = "fv1" end
	if 		discType == "fv1"  then return NavierStokesFV1(fcts, subsets)
	elseif  discType == "fe"   then return NavierStokesFE(fcts, subsets)
	elseif  discType == "fvcr" then return NavierStokesFVCR(fcts, subsets)
	elseif  discType == "fv"   then return NavierStokesFV(fcts, subsets)
	else 
		print("NavierStokes: no disc type '"..discType.."' available. Aborting")
		exit();
	end
end

--!	Returns a NavierStokesInflow Element-Disc of the requested type
--! @return Returns the domain discreatization
--! @param fcts (String) names of symbolic functions 
--! @param subsets (String) names of symbolic subsets 
--! @param discType (String) discretizatin scheme 
function NavierStokesInflow(spMaster)
	if spMaster == nil then
		print("NavierStokesInflow: master disc is not available. Aborting")
		exit();
	end

	discType = spMaster:disc_type();	
	if 		discType == "fv1"  then return NavierStokesInflowFV1(spMaster)
	elseif  discType == "fvcr" then return NavierStokesInflowFVCR(spMaster)
	elseif  discType == "fv"   then return NavierStokesInflowFV(spMaster)
	else 
		print("NavierStokesInflow: no disc type '"..discType.."' available. Aborting")
		exit();
	end
end

--!	Returns a NavierStokesNoNormalStressOutflow Element-Disc of the requested type
--! @return Returns the domain discreatization
--! @param fcts (String) names of symbolic functions 
--! @param subsets (String) names of symbolic subsets 
--! @param discType (String) discretizatin scheme 
function NavierStokesNoNormalStressOutflow(spMaster)
	if spMaster == nil then
		print("NavierStokesNoNormalStressOutflow: master disc is not available. Aborting")
		exit();
	end

	discType = spMaster:disc_type();	
	if 		discType == "fv1"  then return NavierStokesNoNormalStressOutflowFV1(spMaster)
	elseif  discType == "fvcr"   then return NavierStokesNoNormalStressOutflowFVCR(spMaster)
	else 
		print("NavierStokesNoNormalStressOutflow: no disc type '"..discType.."' available. Aborting")
		exit();
	end
end
