----------------------------------------------------------
--
--   Lua - Script to perform the Laplace-Problem
--
--   Author: Andreas Vogel
--
----------------------------------------------------------

-- make sure that ug_util is in the right path.
-- currently only the path in which you start your application is valid.
ug_load_script("ug_util.lua")

-- choose algebra
InitAlgebra(CPUAlgebraChooser());

-- constants
dim = 2

if dim == 2 then
	--gridName = "unit_square_tri.ugx"
	gridName = "unit_square_quads_8x8.ugx"
end
if dim == 3 then
	gridName = "unit_cube_hex.ugx"
	--gridName = "unit_cube_tets_regular.ugx"
end

numPreRefs = 0
numRefs = 3

--ugargc -> anzahl an ugargvs -- TODO: Check, ob ugshell mit genau (mindestens) 2 Prozessen gestartet wurde!
if ugargv[1] ~= nil then
	print(ugargv[1])
end

--------------------------------
-- User Data Functions (begin)
--------------------------------
	function ourDiffTensor2d(x, y, t)
		return	1, 0, 
				0, 1
	end
	
	function ourVelocityField2d(x, y, t)
		return	0, 0
	end
	
	function ourReaction2d(x, y, t)
		return	0
	end
	
	function ourRhs2d(x, y, t)
		local s = 2*math.pi
		return	s*s*(math.sin(s*x) + math.sin(s*y))
		--return -2*y
		--return 0;
	end
	
	function ourNeumannBnd2d(x, y, t)
		--local s = 2*math.pi
		--return -s*math.cos(s*x)
		return true, -2*x*y
	end
	
	function ourDirichletBnd2d(x, y, t)
		local s = 2*math.pi
		return true, math.sin(s*x) + math.sin(s*y)
		--return true, x
		--return true, 2.5
	end

	function ourDiffTensor3d(x, y, z, t)
		return	1, 0, 0,
				0, 1, 0,
				0, 0, 1
	end
	
	function ourVelocityField3d(x, y, z, t)
		return	0, 0, 0
	end
	
	function ourReaction3d(x, y, z, t)
		return	0
	end
	
	function ourRhs3d(x, y, z, t)
		--local s = 2*math.pi
		--return	s*s*(math.sin(s*x) + math.sin(s*y) + math.sin(s*z))
		return 0;
	end
	
	function ourNeumannBnd3d(x, y, t)
		--local s = 2*math.pi
		--return -s*math.cos(s*x)
		return true, -2*x*y*z
	end
	
	function ourDirichletBnd3d(x, y, z, t)
		--local s = 2*math.pi
		--return true, math.sin(s*x) + math.sin(s*y) + math.sin(s*z)
		return true, x
	end
--------------------------------
-- User Data Functions (end)
--------------------------------

-- create Instance of a Domain
print("Create Domain.")
dom = utilCreateDomain(dim)

-- load domain
print("Load Domain from File.")
if utilLoadDomain(dom, gridName) == false then
   print("Loading Domain failed.")
   exit()
end

-- create Refiner
print("Create Refiner")
if numPreRefs > numRefs then
	print("numPreRefs must be smaller/equal than numRefs");
	exit();
end

refiner = GlobalMultiGridRefiner()
refiner:assign_grid(dom:get_grid())
for i=1,numPreRefs do
	refiner:refine()
end

if utilDistributeDomain(dom) == false then
	print("Error while Distributing Grid.")
	exit()
end

print("Refine Parallel Grid")
for i=numPreRefs+1,numRefs do
	utilGlobalRefineParallelDomain(dom)
end

-- get subset handler
sh = dom:get_subset_handler()
if sh:num_subsets() ~= 2 then 
	print("Domain must have 2 Subsets for this problem.")
	exit()
end
sh:set_subset_name("Inner", 0)
sh:set_subset_name("DirichletBoundary", 1)
--sh:set_subset_name("NeumannBoundary", 2)

-- write grid to file for test purpose
utilSaveDomain(dom, "refined_grid.ugx")

-- create function pattern
print("Create Function Pattern")
pattern = P1ConformFunctionPattern()
pattern:set_subset_handler(sh)
AddP1Function(pattern, "c", dim)
pattern:lock()

-- create Approximation Space
print("Create ApproximationSpace")
approxSpace = utilCreateApproximationSpaceWithoutInit(dom, pattern)

-- get number of processes
numProcs = NumProcesses()

--please make sure that numProcs / numSubdomains is a power of 2.
numSubdomains = numProcs/4

print( "NumProcs is " .. numProcs .. ", NumSubDomains is " .. numSubdomains )

-- create subdomain info
print("Create domainDecompInfo")
domainDecompInfo = StandardDomainDecompositionInfo()
domainDecompInfo:set_num_subdomains(numSubdomains)

-- The following is outdated (since 30012011): No subdomains and layouts are built
--calling 'BuildDomainDecompositionLayoutsTest2d', see below (after creation of grid functions)!
--if dim == 2 then
--	EnableDomainDecomposition2d(approxSpace, domainDecompInfo) -- second argument: domain decomp infos (in particular: number of subdomains)
--elseif dim == 3 then
--	EnableDomainDecomposition3d(approxSpace, domainDecompInfo) -- second argument: domain decomp infos (in particular: number of subdomains)
--end

approxSpace:init()
approxSpace:print_statistic()
approxSpace:print_layout_statistic()

-------------------------------------------
--  Setup User Functions
-------------------------------------------
print ("Setting up Assembling")

-- Diffusion Tensor setup
	if dim == 2 then
		diffusionMatrix = utilCreateLuaUserMatrix("ourDiffTensor2d", dim)
	elseif dim == 3 then
		diffusionMatrix = utilCreateLuaUserMatrix("ourDiffTensor3d", dim)
	end
	--diffusionMatrix = utilCreateConstDiagUserMatrix(1.0, dim)

-- Velocity Field setup
	if dim == 2 then
		velocityField = utilCreateLuaUserVector("ourVelocityField2d", dim)
	elseif dim == 3 then
		velocityField = utilCreateLuaUserVector("ourVelocityField3d", dim)
	end 
	--velocityField = utilCreateConstUserVector(0.0, dim)

-- Reaction setup
	if dim == 2 then
		reaction = utilCreateLuaUserNumber("ourReaction2d", dim)
	elseif dim == 3 then
		reaction = utilCreateLuaUserNumber("ourReaction3d", dim)
	end
	--reaction = utilCreateConstUserNumber(0.0, dim)

-- rhs setup
	if dim == 2 then
		rhs = utilCreateLuaUserNumber("ourRhs2d", dim)
	elseif dim == 3 then
		rhs = utilCreateLuaUserNumber("ourRhs3d", dim)
	end
	--rhs = utilCreateConstUserNumber(0.0, dim)

-- neumann setup
	if dim == 2 then
		neumann = utilCreateLuaBoundaryNumber("ourNeumannBnd2d", dim)
	elseif dim == 3 then
		neumann = utilCreateLuaBoundaryNumber("ourNeumannBnd3d", dim)
	end
	--neumann = utilCreateConstUserNumber(0.0, dim)

-- dirichlet setup
	if dim == 2 then
		dirichlet = utilCreateLuaBoundaryNumber("ourDirichletBnd2d", dim)
	elseif dim == 3 then
		dirichlet = utilCreateLuaBoundaryNumber("ourDirichletBnd3d", dim)
	end
	--dirichlet = utilCreateConstBoundaryNumber(3.2, dim)
	
-----------------------------------------------------------------
--  Setup FV Convection-Diffusion Element Discretization
-----------------------------------------------------------------

elemDisc = utilCreateFV1ConvDiff(approxSpace, "c", "Inner")
elemDisc:set_upwind_amount(0.0)
elemDisc:set_diffusion_tensor(diffusionMatrix)
elemDisc:set_velocity_field(velocityField)
elemDisc:set_reaction(reaction)
elemDisc:set_rhs(rhs)

-----------------------------------------------------------------
--  Setup Neumann Boundary
-----------------------------------------------------------------

--neumannDisc = utilCreateNeumannBoundary(approxSpace, "Inner")
--neumannDisc:add_boundary_value(neumann, "c", "NeumannBoundary")

-----------------------------------------------------------------
--  Setup Dirichlet Boundary
-----------------------------------------------------------------

dirichletBND = utilCreateDirichletBoundary(approxSpace)
dirichletBND:add_boundary_value(dirichlet, "c", "DirichletBoundary")

-------------------------------------------
--  Setup Domain Discretization
-------------------------------------------

domainDisc = DomainDiscretization()
domainDisc:add_elem_disc(elemDisc)
--domainDisc:add_elem_disc(neumannDisc)
domainDisc:add_post_process(dirichletBND)

-------------------------------------------
--  Algebra
-------------------------------------------
print ("Setting up Algebra Solver")

-- create operator from discretization
linOp = AssembledLinearOperator()
linOp:export_rhs(true)
linOp:set_discretization(domainDisc)
linOp:set_dof_distribution(approxSpace:get_surface_dof_distribution())

-- get grid function
u = approxSpace:create_surface_function("u", true)
b = approxSpace:create_surface_function("b", true)

-- New creation of subdomains and layouts (since 30012011):
-- test one to many interface creation
for i=0,NumProcesses()-1 do
	print("subdom of proc " .. i .. ": " .. domainDecompInfo:map_proc_id_to_subdomain_id(i))
end

BuildDomainDecompositionLayoutsTest2d(u, domainDecompInfo);
-- OneToManyTests2d(u)


-- set initial value
u:set(0.0)

-- init Operator
print ("Assemble Operator ... ")
linOp:init()
print ("done")

-- set dirichlet values in start iterate
linOp:set_dirichlet_values(u)
b:assign(linOp:get_rhs())

-- write matrix for test purpose
SaveMatrixForConnectionViewer(u, linOp, "Stiffness.mat")
SaveVectorForConnectionViewer(b, "Rhs.mat")

-- debug writer
dbgWriter = utilCreateGridFunctionDebugWriter(dim)
dbgWriter:set_reference_grid_function(u)

-- create algebraic Preconditioner
jac = Jacobi()
jac:set_damp(0.8)
gs = GaussSeidel()
sgs = SymmetricGaussSeidel()
bgs = BackwardGaussSeidel()
ilu = ILU()
ilu2 = ILU()
ilu3 = ILU()
ilut = ILUT()

-- create GMG ---
-----------------

	-- Base Solver
	baseConvCheck = StandardConvergenceCheck()
	baseConvCheck:set_maximum_steps(500)
	baseConvCheck:set_minimum_defect(1e-8)
	baseConvCheck:set_reduction(1e-30)
	baseConvCheck:set_verbose_level(false)
	-- base = LapackLUSolver()
	base = LinearSolver()
	base:set_convergence_check(baseConvCheck)
	base:set_preconditioner(jac)
	
	-- Transfer and Projection
	transfer = utilCreateP1Prolongation(approxSpace)
	transfer:set_dirichlet_post_process(dirichletBND)
	projection = utilCreateP1Projection(approxSpace)
	
	-- Gemoetric Multi Grid
	gmg = utilCreateGeometricMultiGrid(approxSpace)
	gmg:set_discretization(domainDisc)
	gmg:set_surface_level(numRefs)
	gmg:set_base_level(0)
	gmg:set_base_solver(base)
	gmg:set_smoother(jac)
	gmg:set_cycle_type(1)
	gmg:set_num_presmooth(3)
	gmg:set_num_postsmooth(3)
	gmg:set_prolongation(transfer)
	gmg:set_projection(projection)

-- create AMG ---
-----------------

	if false then
	amg = AMGPreconditioner()
	amg:set_nu1(2)
	amg:set_nu2(2)
	amg:set_gamma(1)
	amg:set_presmoother(jac)
	amg:set_postsmoother(jac)
	amg:set_base_solver(base)
	--amg:set_debug(u)
	end

-- exact Solver
exactSolver = LU()

-- create Convergence Check
convCheck = StandardConvergenceCheck()
convCheck:set_maximum_steps(100)
convCheck:set_minimum_defect(1e-11)
convCheck:set_reduction(1e-12)
convCheck:set_verbose_level(false)

-- create Convergence Check
convCheck2 = StandardConvergenceCheck()
convCheck2:set_maximum_steps(100)
convCheck2:set_minimum_defect(1e-11)
convCheck2:set_reduction(1e-12)
convCheck2:set_verbose_level(false)

-- create Convergence Check
convCheck3 = StandardConvergenceCheck()
convCheck3:set_maximum_steps(100)
convCheck3:set_minimum_defect(1e-11)
convCheck3:set_reduction(1e-12)
convCheck3:set_verbose_level(false)

-- create Linear Solver
linSolver = LinearSolver()
linSolver:set_preconditioner(gmg)
linSolver:set_convergence_check(convCheck)

-- create CG Solver
cgSolver = CG()
cgSolver:set_preconditioner(ilu)
cgSolver:set_convergence_check(convCheck)

cg2Solver = CG()
cg2Solver:set_preconditioner(ilu2)
cg2Solver:set_convergence_check(convCheck2)

cg3Solver = CG()
cg3Solver:set_preconditioner(ilu3)
cg3Solver:set_convergence_check(convCheck3)

-- create BiCGStab Solver
bicgstabSolver = BiCGStab()
bicgstabSolver:set_preconditioner(jac)
bicgstabSolver:set_convergence_check(convCheck)

-- create Convergence Check
fetiConvCheck = StandardConvergenceCheck()
fetiConvCheck:set_maximum_steps(50)
fetiConvCheck:set_minimum_defect(1e-9)
fetiConvCheck:set_reduction(1e-12)

-- create FETI Solver
fetiSolver = FETI()
fetiSolver:set_convergence_check(fetiConvCheck)
fetiSolver:set_dual_dirichlet_solver(cg3Solver)
fetiSolver:set_neumann_solver(cgSolver)
fetiSolver:set_dirichlet_solver(cg2Solver)
fetiSolver:set_domain_decomp_info(domainDecompInfo)

-- Apply Solver
ApplyLinearSolver(linOp, u, b, fetiSolver)

-- Output
WriteGridFunctionToVTK(u, "Solution")

-- check
print( "domainDecompInfo:get_num_subdomains(): " .. domainDecompInfo:get_num_subdomains())
print( "domainDecompInfo:get_num_procs_per_subdomain(): " .. domainDecompInfo:get_num_procs_per_subdomain())

--localSchurComplement = LocalSchurComplement()
--localSchurComplement:set_matrix(linOp)
--localSchurComplement:set_dirichlet_solver(cgSolver)
--localSchurComplement:set_debug(dbgWriter)
--localSchurComplement:init()

--WriteGridFunctionToVTK(b, "b_ass")

-- Apply Solver
--ApplyLinearSolver(linOp, u, b, fetiSolver)

-- Apply local Schur complement
-- Sowas wie 'ApplyLinearOperator(linOp, u, b, localSchurComplement)' gibts noch nicht (ist offenbar auch nicht gedacht, vom Skript aus aufgerufen zu werden)
-- compute S times b =: u

-- manually set type to additive __without__ changing values in vector
-- this is a somehow forbidden action for real simulations but good for testing
--b:set_storage_type_by_string("consistent");

--b:change_storage_type_by_string("consistent");

--WriteGridFunctionToVTK(b, "b_consistent")

-- applies the Schur complement built from matrix operator 'linOp' to 'b' and returns the result 'S times b' in 'u'
--localSchurComplement:apply(u, b)

-- Output
--WriteGridFunctionToVTK(b, "b")
--WriteGridFunctionToVTK(u, "S_times_b")