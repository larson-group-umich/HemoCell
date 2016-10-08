#include "ficsion.h"

typedef double T;
#define DESCRIPTOR descriptors::ForcedMRTD3Q19Descriptor



// ----------------------- Copy from neighbour ------------------------------------

template<typename Tp>
class CopyFromNeighbor : public BoxProcessingFunctional3D_S<Tp> {
public:
    CopyFromNeighbor(Array<plint, 3> offset_) : offset(offset_) { };

    virtual void process(Box3D domain, ScalarField3D<Tp> &field1);

    virtual CopyFromNeighbor<Tp> *clone() const;

    virtual void getTypeOfModification(std::vector<modif::ModifT> &modified) const;

    virtual BlockDomain::DomainT appliesTo() const;

private:
    Array<plint, 3> offset;
};

template<typename Tp>
void CopyFromNeighbor<Tp>::process(
        Box3D domain, ScalarField3D<Tp> &field1) {
    for (plint iX = domain.x0; iX <= domain.x1; ++iX) {
        for (plint iY = domain.y0; iY <= domain.y1; ++iY) {
            for (plint iZ = domain.z0; iZ <= domain.z1; ++iZ) {
                field1.get(iX, iY, iZ) = field1.get(iX + offset[0], iY + offset[1], iZ + offset[2]);
            }
        }
    }
}

template<typename Tp>
CopyFromNeighbor<Tp> *CopyFromNeighbor<Tp>::clone() const {
    return new CopyFromNeighbor<Tp>(*this);
}

template<typename Tp>
void CopyFromNeighbor<Tp>::getTypeOfModification(std::vector<modif::ModifT> &modified) const {
    modified[0] = modif::allVariables;
}

template<typename Tp>
BlockDomain::DomainT CopyFromNeighbor<Tp>::appliesTo() const {
    return BlockDomain::bulk;
}


// ---------------------- Read in STL geometry ---------------------------------

void getFlagMatrixFromSTL(std::string meshFileName, plint extendedEnvelopeWidth, plint refDirLength, plint refDir,
                          VoxelizedDomain3D<T> *&voxelizedDomain, MultiScalarField3D<int> *&flagMatrix) {
    plint extraLayer = 0;  	// Make the bounding box larger; for visualization purposes
    						//   only. For the simulation, it is OK to have extraLayer=0.
    plint blockSize = -1; 	// Zero means: no sparse representation.
    plint borderWidth = 1;  // Because the Guo boundary condition acts in a one-cell layer.
    
    // Requirement: margin>=borderWidth.
    plint margin = 1;  // Extra margin of allocated cells around the obstacle.

    TriangleSet<T> *triangleSet = new TriangleSet<T>(meshFileName, DBL);

    DEFscaledMesh<T> *defMesh =
            new DEFscaledMesh<T>(*triangleSet, refDirLength, refDir, margin, extraLayer);
    TriangleBoundary3D<T> boundary(*defMesh);
    delete defMesh;
    boundary.getMesh().inflate();

    voxelizedDomain = new VoxelizedDomain3D<T>(
            boundary, voxelFlag::inside, extraLayer, borderWidth, extendedEnvelopeWidth, blockSize);
    
    // Print out some info
    pcout << "(main) Voxelisation is done. Resulting domain parameters are: " << endl;
    pcout << getMultiBlockInfo(voxelizedDomain->getVoxelMatrix()) << std::endl;


    flagMatrix = new MultiScalarField3D<int>((MultiBlock3D &) voxelizedDomain->getVoxelMatrix());

    setToConstant(*flagMatrix, voxelizedDomain->getVoxelMatrix(),
                  voxelFlag::inside, flagMatrix->getBoundingBox(), 1);
    setToConstant(*flagMatrix, voxelizedDomain->getVoxelMatrix(),
                  voxelFlag::innerBorder, flagMatrix->getBoundingBox(), 1);


    Box3D domainBox = flagMatrix->getBoundingBox();
    plint nx = domainBox.getNx();
    plint ny = domainBox.getNy();
    plint nz = domainBox.getNz();

    Box3D domain(0, 1, 0, ny - 1, 0, nz - 1);
    applyProcessingFunctional(new CopyFromNeighbor<int>(Array<plint, 3>(1, 0, 0)), domain, *flagMatrix);

    domain = Box3D(nx - 2, nx - 1, 0, ny - 1, 0, nz - 1);
    applyProcessingFunctional(new CopyFromNeighbor<int>(Array<plint, 3>(-1, 0, 0)), domain, *flagMatrix);


    // Output result for debugging
    //VtkImageOutput3D<T> vtkOut("test.vtk");
    //vtkOut.writeData<float>(*copyConvert<int, double>(*extractSubDomain(*flagMatrix, flagMatrix->getBoundingBox())), "flag", 1.);

}


// --------------------- main --------------------------------------------------

int main(int argc, char *argv[]) {

	if(argc < 2)
	{
		cout << "Usage: " << argv[0] << " <configuration.xml>" << endl;
		return -1;
	}

    plbInit(&argc, &argv);
    global::timer("ficsion_init").start();

    global::directories().setOutputDir("./tmp/");
    global::directories().setLogOutDir("./log/");
    global::directories().setInputDir("./");

    global::IOpolicy().activateParallelIO(true);
    global::IOpolicy().setStlFilesHaveLowerBound(false);
//    global::IOpolicy().setLowerBoundForStlFiles(-1.);

    std::string outputDir = global::directories().getOutputDir();
    std::string inputDir = global::directories().getInputDir();
    std::string logOutDir = global::directories().getLogOutDir();

    mkpath((outputDir + "/hdf5/").c_str(), 0777);
    mkpath(logOutDir.c_str(), 0777);


    // ------------------------ Variable declarations here -----------------------------------------

    bool checkpointed = 0;
    plint initIter = 0;
    T Re;
    T dx, dt, dm, dNewton;
    T tau, nu_lbm, u_lbm_max;
    T nu_p, rho_p;
    T cell_dt;
    plint cellStep;
    plint nx, ny, nz;
    T hematocrit;
    T shellDensity, eqLengthRatio, radius;
    T k_WLC, k_rep, k_rest, k_elastic, k_bend, k_volume, k_surface, k_shear, k_stretch, eta_m;
    plint minNumOfTriangles;
    plint ibmKernel, ibmScheme;
    plint tmax, tmeas;
    plint refDir, refDirN;
    std::string meshFileName;
    plint warmup;
    plint maxPackIter;
    string particlePosFile;


    // ------------------------- Read in config file ------------------------------------------------

    string paramXmlFileName;
    global::argv(1).read(paramXmlFileName);

    pcout << "(main) reading " <<  paramXmlFileName <<"..." << std::endl;

    XMLreader documentXML(paramXmlFileName);

    // Check if it is a fresh start or a checkpointed run
    std::string firstField = (*(documentXML.getChildren(
            documentXML.getFirstId())[0])).getName(); // VERY COMPLICATED! Hope I could find sth easier!
    if (firstField == "ficsion") { checkpointed = 0; }
    else { checkpointed = 1; }

    XMLreaderProxy document = checkpointed ? documentXML["Checkpoint"]["ficsion"] : documentXML["ficsion"];

    // ---- Particle material properties
  	document["parameters"]["Re"].read(Re);
    document["parameters"]["warmup"].read(warmup);
    document["parameters"]["maxPackIter"].read(maxPackIter);

    document["cellModel"]["shellDensity"].read(shellDensity);
    document["cellModel"]["kWLC"].read(k_WLC);
    document["cellModel"]["eqLengthRatio"].read(eqLengthRatio);
    document["cellModel"]["kRep"].read(k_rep);
    document["cellModel"]["kElastic"].read(k_elastic);
    document["cellModel"]["kBend"].read(k_bend);
    document["cellModel"]["kVolume"].read(k_volume);
    document["cellModel"]["kSurface"].read(k_surface);
    document["cellModel"]["etaM"].read(eta_m);
    document["cellModel"]["kRest"].read(k_rest);
    document["cellModel"]["kShear"].read(k_shear);
    document["cellModel"]["kStretch"].read(k_stretch);
  
    document["ibm"]["ibmKernel"].read(ibmKernel);
    document["ibm"]["ibmScheme"].read(ibmScheme);
    document["ibm"]["radius"].read(radius);
    document["ibm"]["minNumOfTriangles"].read(minNumOfTriangles);

    document["domain"]["geometry"].read(meshFileName);
    document["domain"]["rhoP"].read(rho_p);
    document["domain"]["nuP"].read(nu_p);
    document["domain"]["dx"].read(dx);
    document["domain"]["dt"].read(dt);
    document["domain"]["refDir"].read(refDir);
    document["domain"]["refDirN"].read(refDirN);
    document["domain"]["timeStepSize"].read(cellStep);

    document["sim"]["tmax"].read(tmax);
    document["sim"]["tmeas"].read(tmeas);
    document["sim"]["hematocrit"].read(hematocrit);
    document["sim"]["particlePosFile"].read(particlePosFile);


    hematocrit /= 100;

    // ---------------------------- Read in geometry (STL) ------------------------------------------------

	pcout << "(main) reading geometry stl..." << std::endl;

    plint extendedEnvelopeWidth = 2;  // Depends on the requirements of the ibmKernel. 4 or even 2 might be enough (also depends on dx)

    plb::MultiScalarField3D<int> *flagMatrix = 0;
    VoxelizedDomain3D<T> *voxelizedDomain = 0;
    getFlagMatrixFromSTL(meshFileName, extendedEnvelopeWidth, refDirN, refDir, voxelizedDomain, flagMatrix);
    Box3D domainBox = flagMatrix->getBoundingBox();
    
    nx = domainBox.getNx();
    ny = domainBox.getNy();
    nz = domainBox.getNz();

    nu_lbm = nu_p * dt / (dx*dx); 
    u_lbm_max = Re * nu_lbm / ny;

    tau = 3.0 * nu_lbm + 0.5;
    dm = rho_p * (dx * dx * dx);
    dNewton = (dm * dx / (dt * dt));
	//kBT = kBT_p / ( dNewton * dx );
    //shearRate = shearRate_p * dt;
    //stretchForceScalar = stretchForce_p / dNewton;

    pcout << "(main) dx = " << dx << ", " <<
    "dt = " << dt << ", " <<
    "cell_dt" << cellStep * dt <<
    "dm = " << dm << ", " <<
    "dN = " << dNewton <<
    std::endl;

    pcout << "(main) tau = " << tau << " Re = " << Re << " u_lb_max = " << u_lbm_max << " nu_lb = " << nu_lbm << endl;
    pcout << "(main) Re corresponds to u_max = " << (Re * nu_p)/(ny*dx) << " [m/s]" << endl;

    // Do a general check for suspiciosly off values.
    checkParameterSanity(nu_lbm, u_lbm_max);
    
    //pcout << "(main) dx = " << parameters.getDeltaX() << " dt = " << parameters.getDeltaT() << endl;
    //pcout << "(main) tau = " << parameters.getTau() << " Re = " << parameters.getRe() << " u_lb = " << parameters.getLatticeU() << " nu_lb = " << parameters.getLatticeNu() << endl;
    //cout << "(main) Re corresponds to u_max = " << parameters.getPhysicalU() << " [m/s]" << endl;
    

    // ------------------------ Init lattice --------------------------------

    pcout << "(main) init lattice structure..."  << std::endl;
    MultiBlockLattice3D<T, DESCRIPTOR> lattice(
            defaultMultiBlockPolicy3D().getMultiBlockManagement(nx, ny, nz, extendedEnvelopeWidth),
            defaultMultiBlockPolicy3D().getBlockCommunicator(),
            defaultMultiBlockPolicy3D().getCombinedStatistics(),
            defaultMultiBlockPolicy3D().getMultiCellAccess<T, DESCRIPTOR>(),
            new GuoExternalForceMRTdynamics<T, DESCRIPTOR>(1.0/tau));


    defineDynamics(lattice, *flagMatrix, lattice.getBoundingBox(), new BounceBack<T, DESCRIPTOR>(1.), 0);

    lattice.periodicity().toggleAll(true);
    initializeAtEquilibrium(lattice, lattice.getBoundingBox(), 1., Array<T, 3>(0., 0., 0.));


    // ----------------------- Define external driving force ---------------

    //T poiseuilleForce = 8 * (nu_lbm * nu_lbm) * Re / (ny * ny * ny);
    T poiseuilleForce = 8 * nu_lbm * (u_lbm_max*0.5) / (refDirN/2.0) / (refDirN/2.0);
    if (tmeas == 0) {
        tmeas = plint(ny * ny * 1.0 / (4 * nu_lbm * Re)) / 40;
    }

    setExternalVector(lattice, lattice.getBoundingBox(),
                      DESCRIPTOR<T>::ExternalField::forceBeginsAt,
                      Array<T, DESCRIPTOR<T>::d>(poiseuilleForce, 0.0, 0.0));


    lattice.initialize();

   
    // ----------------------- Init cell models --------------------------

    pcout << "(main) init cell structures..."  << std::endl;
    T persistenceLengthFine = 7.5e-9; // In meters
    k_rest = 0;

    std::vector<ConstitutiveModel<T, DESCRIPTOR> *> cellModels;
    std::vector<CellField3D<T, DESCRIPTOR> *> cellFields;
    std::vector<T> eqVolumes;

    
    // ----------------------- Init RBCs ---------------------------------

    pcout << "(main)   init RBC structures..."  << std::endl;
    Array<T,3> eulerAngles(0., 0., 0.);

    plint shape = 1; 	// shape: Sphere[0], RBC from sphere[1], Cell(defined)[2], RBC from file [3] RBC from Octahedron [4] Sphere from Octahedron [5]
	std::string cellPath = " "; // If particle is loaded from stl file.

    TriangleBoundary3D<T> RBCCells = constructMeshElement(shape, radius / dx, minNumOfTriangles, dx, cellPath, eulerAngles);
    TriangularSurfaceMesh<T> meshElement = RBCCells.getMesh();
    MeshMetrics<T> meshmetric(meshElement);
    meshmetric.write();
    eqVolumes.push_back(meshmetric.getVolume());
    plint numVerticesPerCell = meshElement.getNumVertices();
    /* The Maximum length of two vertices should be less than 2.0 LU (or not)*/
    cellModels.push_back(
            new ShapeMemoryModel3D<T, DESCRIPTOR>(shellDensity, k_rest, k_shear, k_bend, k_stretch, k_WLC, k_elastic,
                                                  k_volume, k_surface, eta_m,
                                                  persistenceLengthFine, eqLengthRatio, dx, dt, dm, meshElement));
    cellFields.push_back(new CellField3D<T, DESCRIPTOR>(lattice, meshElement, hematocrit, cellModels[0], ibmKernel, "RBC"));

    
    // ----------------------- Init platelets ----------------------------
    // TODO - Collect material properties, don't just derive them from RBC

    pcout << "(main)   init PLT structures..."  << std::endl;
    T pltRadius = 1.15e-6 / dx;
    T aspectRatio = 1.0 / 2.3;//(2 * pltRadius);
    TriangleBoundary3D<T> PLTCells = constructMeshElement(6, pltRadius, (plint)ceil(minNumOfTriangles/3.0), dx, cellPath, eulerAngles, aspectRatio);
    TriangularSurfaceMesh<T> pltMeshElement = PLTCells.getMesh();
    eqVolumes.push_back(MeshMetrics<T>(pltMeshElement).getVolume());
    cellModels.push_back(
            new ShapeMemoryModel3D<T, DESCRIPTOR>(shellDensity, k_rest, k_shear, k_bend * 5, k_stretch, k_WLC * 5.0,
                                                  k_elastic, k_volume, k_surface, eta_m,
                                                  persistenceLengthFine, eqLengthRatio, dx, dt, dm, pltMeshElement));
    cellFields.push_back(new CellField3D<T, DESCRIPTOR>(lattice, pltMeshElement, 0.0025 * hematocrit,
                                                        cellModels[cellModels.size() - 1], ibmKernel, "PLT"));


    // ---------------------- Initialise particle positions if it is not a checkpointed run ---------------

    FcnCheckpoint<T, DESCRIPTOR> checkpointer(documentXML);
    checkpointer.load(documentXML, lattice, cellFields, initIter);

    if (not checkpointer.wasCheckpointed()) {
        pcout << "(main) initializing particle positions from " << particlePosFile << "..." << std::endl;

        //orderedPositionMultipleCellField3D(cellFields);
        //randomPositionMultipleCellField3D(cellFields, hematocrit, dx, maxPackIter);
        readPositionsBloodCellField3D(cellFields, dx, particlePosFile.c_str());
       
        checkpointer.save(lattice, cellFields, initIter);
    }

    // ---------------------- Set integration scheme and time step amplification for cell fields ---------------

    for (pluint iCell = 0; iCell < cellFields.size(); ++iCell) {
        cellFields[iCell]->setParticleUpdateScheme(ibmScheme, (T)cellStep);
    }

    pcout << "(main) material model integration step: " << cellStep <<  " lt, update scheme: " << ibmScheme << endl;

    pcout << "(main) material model integration step: " << (T)cellStep <<  " lt, update scheme: " << ibmScheme << endl;


    plint domainVol = computeSum(*flagMatrix);
    pcout << "(main) Number of fluid cells: " << domainVol << " / " << nx*ny*nz << std::endl;
    for (pluint iCell = 0; iCell < cellFields.size(); ++iCell) {
        plint nCells = cellFields[iCell]->getNumberOfCells_Global();
        pcout << "(main) Species: " << iCell << endl;
        pcout << "(main)   Volume ratio [x100%]: " << nCells * eqVolumes[iCell] * 100.0 / (domainVol) << std::endl;
        pcout << "(main)   nCells (global) = " << nCells << ", pid: " << global::mpi().getRank();
        pcout << ", Volume = " << eqVolumes[iCell] << std::endl;
    }


    // ------------------------- Sync all quantities ----------------------

    pcout << "(main) synchronising quantities..."  << std::endl;
    SyncRequirements everyCCR(allReductions);
    for (pluint iCell = 0; iCell < cellFields.size(); ++iCell) {
        cellFields[iCell]->synchronizeCellQuantities(everyCCR);
    }
    

    // -------------------------- Initial output --------------------------

    pcout << "(main) saving initial output..."  << std::endl;
    global::timer("HDFOutput").start();
    bool invertXZ_for_XDMF = true;
    writeHDF5(lattice, dx, dt, initIter, invertXZ_for_XDMF);
    for (pluint iCell = 0; iCell < cellFields.size(); ++iCell) {
        writeCellField3D_HDF5(*cellFields[iCell], dx, dt, initIter);
        writeCell3D_HDF5(*cellFields[iCell], dx, dt, initIter);
    }
    global::timer("HDFOutput").stop();


    // ------------------------- Create boundary particles ----------------
    
    pcout << "(main) creating boundary particles..."  << std::endl;
    MultiParticleField3D<DenseParticleField3D<T, DESCRIPTOR> > *boundaryParticleField3D =
            createBoundaryParticleField3D(lattice);
    writeParticleField3D_HDF5(*boundaryParticleField3D, dx, dt, 0, "BoundaryParticles");

    
    // ------------------------- Add particle-wall repulsion force -------------
    // F > 0 means repulsion. F < 0 is used for adhesion,

    //T k_int = 0.00025, DeltaX=1.0, R=1.0, k=1.5;
    // was 1.5e-12
    T k_int = 2.0e-12 / dNewton, DeltaX=1.5e-6 / dx, R=1.5e-6 / dx, k=1.5;	// Scale force with simulation units
    if (DeltaX > 2.0) DeltaX = 2.0; if (R > 2.0) R = 2.0;  // Should not have effect further than 2 lu -> might go out of domain
    PowerLawForce<T> repWP(k_int, DeltaX, R, k);

    // ------------------------- Add particle-particle repulsion force ---------

    k_int = 1.0e-13 / dNewton; DeltaX=0.75e-6 / dx; T R2=0.75e-6 / dx; k=1.5;
    if (DeltaX > 2.0) DeltaX = 2.0; if (R2 > 2.0) R2 = 2.0;
    PowerLawForce<T> repPP(k_int, DeltaX, R2, k);


	// ------------------------- Warming up fluid domain ------------------    

    if (initIter == 0)
    {
        pcout << "(main) fresh start: warming up fluid domain for "  << warmup << " iterations..." << std::endl;
        for (plint itrt = 0; itrt < warmup; ++itrt) { cellFields[0]->setFluidExternalForce(poiseuilleForce); lattice.collideAndStream(); }
    }


	// ------------------------ Starting main loop --------------------------
	pcout << std::endl << "(main) starting simulation at " << initIter << " of tmax=" << tmax << " iterations (" << tmax * dt << " s)..." << std::endl;
    SimpleFicsionProfiler simpleProfiler(tmeas);
    simpleProfiler.writeInitial(nx, ny, nz, -1, numVerticesPerCell);

    
    global::timer("mainLoop").start();
    for (plint iter = initIter; iter < tmax + 1; iter+=cellStep) {

        // #1# Membrane Model + Repulsion of surfaces
        for (pluint iCell = 0; iCell < cellFields.size(); ++iCell) {
            cellFields[iCell]->applyConstitutiveModel();
            cellFields[iCell]->applyWallCellForce(repWP, R, boundaryParticleField3D);
            cellFields[iCell]->applyCellCellForce(repPP, R2);
        }

        // Particle-particle force between RBCs and PLTs
        cellFields[1]->applyDifferentCellForce(repPP, R2, &cellFields[0]->getParticleField3D());

        
        // Inner iteration cycle of fluid
        for(int innerIt = 0; innerIt < cellStep; innerIt++){
            // #2# IBM Spreading
            cellFields[0]->setFluidExternalForce(poiseuilleForce);
            for (pluint iCell = 0; iCell < cellFields.size(); ++iCell) {
                cellFields[iCell]->spreadForceIBM();
            }

            // #3# LBM
            global::timer("LBM").start();
            lattice.collideAndStream();
            global::timer("LBM").stop();
        }

        // Gather velocities from the fluid
        for (pluint iCell = 0; iCell < cellFields.size(); ++iCell) {
            // #4# IBM Interpolation
            cellFields[iCell]->interpolateVelocityIBM();
            // #5# Position Update
            cellFields[iCell]->advanceParticles();
        }

        // #6# Output
        if ((iter % tmeas) == 0) {
            SyncRequirements everyCCR(allReductions);
            for (pluint iCell = 0; iCell < cellFields.size(); ++iCell) {
                cellFields[iCell]->synchronizeCellQuantities(everyCCR);
            }
            global::timer("HDFOutput").start();
            bool invertXZ_for_XDMF = true;
            writeHDF5(lattice, dx, dt, iter, invertXZ_for_XDMF);
            for (pluint iCell = 0; iCell < cellFields.size(); ++iCell) {
                writeCellField3D_HDF5(*cellFields[iCell], dx, dt, iter);
                writeCell3D_HDF5(*cellFields[iCell], dx, dt, iter);
            }
            global::timer("HDFOutput").stop();
            if ((iter % (2 * tmeas)) == 0) {
                global::timer("Checkpoint").start();
                checkpointer.save(lattice, cellFields, iter);
                global::timer("Checkpoint").stop();
            }
            T dtIteration = global::timer("mainLoop").stop();
            simpleProfiler.writeIteration(iter * cellStep);
            pcout << "(main) Iteration:" << iter << "(" << iter * dt << " s)" <<"; time / it = " << dtIteration / tmeas;
            pcout << std::endl;
        } else {
            for (pluint iCell = 0; iCell < cellFields.size(); ++iCell) {
                cellFields[iCell]->synchronizeCellQuantities();
            }
        }
    }

    for (pluint iCell = 0; iCell < cellFields.size(); ++iCell) {
        delete cellFields[iCell];
    }

    simpleProfiler.writeIteration(tmax + 1);
    pcout << "(main) simulation finished. :)" << std::endl;
}
