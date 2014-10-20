#ifndef CELL_FIELD_FUNCTIONALS_3D_HH
#define CELL_FIELD_FUNCTIONALS_3D_HH
#include "cellFieldFunctionals3D.h"


/* ******** PositionCellParticles3D *********************************** */

template<typename T, template<typename U> class Descriptor>
void PositionCellParticles3D<T,Descriptor>::processGenericBlocks (
        Box3D domain, std::vector<AtomicBlock3D*> blocks )
{
#ifdef PLB_MPI_PARALLEL
    //  Get the number of processes.
    int  p = MPI::COMM_WORLD.Get_size();
    //  Get the individual process ID.
     int id = MPI::COMM_WORLD.Get_rank();
#else
     int p=1;
     int id = 0;
#endif

    PLB_PRECONDITION( blocks.size()==2 );
    ParticleField3D<T,Descriptor>& particleField =
        *dynamic_cast<ParticleField3D<T,Descriptor>*>(blocks[0]);
    BlockLattice3D<T,Descriptor>& fluid =
        *dynamic_cast<BlockLattice3D<T,Descriptor>*>(blocks[1]);

    cellOrigins.clear();

    Dot3D relativePosition = fluid.getLocation();
    Array<T,3> relativeCoordinate(relativePosition.x+2.0, relativePosition.y+2.0, relativePosition.z+2.0);
    plint DeltaX = domain.x1 - domain.x0;
    plint DeltaY = domain.y1 - domain.y0;
    plint DeltaZ = domain.z1 - domain.z0;
    relativeCoordinate += Array<T,3>(DeltaX/2.0, DeltaY/2.0, DeltaZ/2.0);
    cellOrigins.push_back(  relativeCoordinate );

    plint nVertices = elementaryMesh.getNumVertices();
    for (pluint iCO=0; iCO < cellOrigins.size(); ++iCO) {
        Array<T,3> & cellOrigin = cellOrigins[iCO];
        plint cellId = iCO + id*100 ;
        cout << "cellId " << cellId << std::endl;
        for (plint iVertex=0; iVertex < nVertices; ++iVertex) {
            Array<T,3> vertex = cellOrigin + elementaryMesh.getVertex(iVertex);
            particleField.addParticle(particleField.getBoundingBox(), new ImmersedCellParticle3D<T,Descriptor>(iVertex, vertex, cellId) );
        }
    }
}

template<typename T, template<typename U> class Descriptor>
PositionCellParticles3D<T,Descriptor>* PositionCellParticles3D<T,Descriptor>::clone() const {
    return new PositionCellParticles3D<T,Descriptor>(*this);
}

template<typename T, template<typename U> class Descriptor>
void PositionCellParticles3D<T,Descriptor>::getTypeOfModification (
        std::vector<modif::ModifT>& modified ) const
{
    modified[0] = modif::dynamicVariables; // Particle field.
    modified[1] = modif::nothing; // Fluid field.
}

template<typename T, template<typename U> class Descriptor>
void PositionCellParticles3D<T,Descriptor>::getModificationPattern(std::vector<bool>& isWritten) const {
    isWritten[0] = true;  // Particle field.
    isWritten[1] = false;  // Particle field.
}

template<typename T, template<typename U> class Descriptor>
BlockDomain::DomainT PositionCellParticles3D<T,Descriptor>::appliesTo() const {
    return BlockDomain::bulk;
}

/* ******** RandomPositionCellParticlesForGrowth3D *********************************** */

template<typename T, template<typename U> class Descriptor>
void RandomPositionCellParticlesForGrowth3D<T,Descriptor>::processGenericBlocks (
        Box3D domain, std::vector<AtomicBlock3D*> blocks )
{
#ifdef PLB_MPI_PARALLEL
    //  Get the number of processes.
    int  p = MPI::COMM_WORLD.Get_size();
    //  Get the individual process ID.
     int id = MPI::COMM_WORLD.Get_rank();
#else
     int p=1;
     int id = 0;
#endif
    if (hematocrit > 1.0) { hematocrit /= 100; }
    PLB_PRECONDITION( hematocrit < 1.0 );

    srand (p * id);
    PLB_PRECONDITION( blocks.size()==2 );
    ParticleField3D<T,Descriptor>& particleField =
        *dynamic_cast<ParticleField3D<T,Descriptor>*>(blocks[0]);
    BlockLattice3D<T,Descriptor>& fluid =
        *dynamic_cast<BlockLattice3D<T,Descriptor>*>(blocks[1]);
//    fluid.get(1, 1, 1).getDynamics().isBoundary();
    ElementsOfTriangularSurfaceMesh<T> emptyEoTSM;
    TriangularSurfaceMesh<T> * mesh = copyTriangularSurfaceMesh(elementaryMesh, emptyEoTSM);

    Array<T,2> xRange, yRange, zRange;
    mesh->computeBoundingBox (xRange, yRange, zRange);
    T dx = xRange[1] - xRange[0], dy = yRange[1] - yRange[0], dz = zRange[1] - zRange[0];
    T maxSide = max( max(dx, dy), dz);
    // Bring mesh to the center
    Array<T,3> mvp(dx, dy, dz);
    Array<T,3> meshCenter = Array<T,3>(xRange[1] + xRange[0], yRange[1] + yRange[0], zRange[1] + zRange[0]) * 0.5;
    mesh->translate(-1.0 * meshCenter);
    MeshMetrics<T> mm(*mesh);
    T volume = mm.getVolume();

    plint step = plint( pow(volume/hematocrit, 1.0/3.0) ) - 1;
    plint DeltaX = domain.x1-domain.x0;
    plint DeltaY = domain.y1-domain.y0;
    plint DeltaZ = domain.z1-domain.z0;

    T Delta = 7;
    DeltaX -= Delta*2;
    DeltaY -= Delta*2;
    DeltaZ -= Delta*2;

    T pcOfDomainConsidered = plint((DeltaX-3)/step) * plint((DeltaY-3)/step) * plint((DeltaZ-3)/step) * step*step*step / (DeltaX*DeltaY*DeltaZ * 1.0);
    T rescaledHematocrit = hematocrit  / pcOfDomainConsidered;
    T prob = step * step * step * 1.0 / volume * rescaledHematocrit;

    T scale = step*1.0/maxSide;
    if (scale > 0.8) { scale = 0.8; };
    mesh->scale(scale);
    ratio = scale;

    // Access the position of the atomic-block inside the multi-block.
    Dot3D relativePosition = fluid.getLocation();
    Array<T,3> relativeCoordinate(relativePosition.x, relativePosition.y, relativePosition.z);
    relativeCoordinate += mvp * (0.5/ scale) + 2.0 + Delta;

    plint nVertices = mesh->getNumVertices();
    PLB_PRECONDITION( step <= DeltaX &&  step <= DeltaY && step <= DeltaZ );
    plint cellId = 0;
    // Loop through the domain and place cell depending on the
    for (plint iX=0; iX<(DeltaX-step-2 - Delta); iX += step) {
        for (plint iY=0; iY<(DeltaY-step-2- Delta); iY+=step) {
            for (plint iZ=0; iZ<(DeltaZ-step-2 - Delta); iZ+=step) {
                T rn = guessRandomNumber();
                if (rn <= prob) {
                    meshRandomRotation(mesh);
//                    Array<T,3> cntr = Array<T,3>(iX*1.0, iY*1.0, iZ*1.0) + relativeCoordinate;
//                    cout << "cntr = (" << cntr[0] << ", " << cntr[1] << ", " << cntr[2]<< ")" << std::endl;
                    for (plint iVertex=0; iVertex < nVertices; ++iVertex) {
                        Array<T,3> vertex = Array<T,3>(iX*1.0, iY*1.0, iZ*1.0) + relativeCoordinate + mesh->getVertex(iVertex);
                        particleField.addParticle(particleField.getBoundingBox(), new ImmersedCellParticle3D<T,Descriptor>(iVertex, vertex, cellId + 1000*id) );
                    }
                    cellId+=1;
                }
            }
        }
    }
    cout << p << " scale " << scale << " step " << step <<  " NCells " << cellId << " h " << cellId * volume * 1.0/ (DeltaX*DeltaY*DeltaZ) << std::endl;
}

template<typename T, template<typename U> class Descriptor>
RandomPositionCellParticlesForGrowth3D<T,Descriptor>* RandomPositionCellParticlesForGrowth3D<T,Descriptor>::clone() const {
    return new RandomPositionCellParticlesForGrowth3D<T,Descriptor>(*this);
}

template<typename T, template<typename U> class Descriptor>
void RandomPositionCellParticlesForGrowth3D<T,Descriptor>::getTypeOfModification (
        std::vector<modif::ModifT>& modified ) const
{
    modified[0] = modif::dynamicVariables; // Particle field.
    modified[1] = modif::nothing; // Fluid field.
}

template<typename T, template<typename U> class Descriptor>
void RandomPositionCellParticlesForGrowth3D<T,Descriptor>::getModificationPattern(std::vector<bool>& isWritten) const {
    isWritten[0] = true;  // Particle field.
    isWritten[1] = false;  // Particle field.
}

template<typename T, template<typename U> class Descriptor>
BlockDomain::DomainT RandomPositionCellParticlesForGrowth3D<T,Descriptor>::appliesTo() const {
    return BlockDomain::bulk;
}




/* ******** ComputeCellForce3D *********************************** */

template<typename T, template<typename U> class Descriptor>
ComputeCellForce3D<T,Descriptor>::ComputeCellForce3D (ConstitutiveModel<T,Descriptor>* cellModel_, std::map<plint, Cell3D<T,Descriptor>* > & cellIdToCell3D_)
: cellModel(cellModel_), cellIdToCell3D(cellIdToCell3D_) { }

template<typename T, template<typename U> class Descriptor>
ComputeCellForce3D<T,Descriptor>::ComputeCellForce3D (
            ComputeCellForce3D<T,Descriptor> const& rhs)
    : cellModel(rhs.cellModel), cellIdToCell3D(rhs.cellIdToCell3D)
{ }


template<typename T, template<typename U> class Descriptor>
void ComputeCellForce3D<T,Descriptor>::processGenericBlocks (
        Box3D domain, std::vector<AtomicBlock3D*> blocks )
{
    typename std::map<plint, Cell3D<T,Descriptor>*  >::iterator iter;
    for (iter  = cellIdToCell3D.begin(); iter != cellIdToCell3D.end(); ++iter) {
        cellModel->computeCellForce(iter->second);
    }
}

template<typename T, template<typename U> class Descriptor>
ComputeCellForce3D<T,Descriptor>*
    ComputeCellForce3D<T,Descriptor>::clone() const
{
    return new ComputeCellForce3D<T,Descriptor>(*this);
}

template<typename T, template<typename U> class Descriptor>
BlockDomain::DomainT ComputeCellForce3D<T,Descriptor>::appliesTo() const {
    return BlockDomain::bulk;
}

template<typename T, template<typename U> class Descriptor>
void ComputeCellForce3D<T,Descriptor>::getTypeOfModification (
        std::vector<modif::ModifT>& modified ) const
{
    modified[0] = modif::dynamicVariables; // Particle field.
}

template<typename T, template<typename U> class Descriptor>
void ComputeCellForce3D<T,Descriptor>::getModificationPattern(std::vector<bool>& isWritten) const {
    isWritten[0] = true;  // Particle field.
}



/* ******** FluidVelocityToImmersedCell3D *********************************** */

template<typename T, template<typename U> class Descriptor>
FluidVelocityToImmersedCell3D<T,Descriptor>::FluidVelocityToImmersedCell3D (
        plint ibmKernel_) : ibmKernel(ibmKernel_) {};

template<typename T, template<typename U> class Descriptor>
void FluidVelocityToImmersedCell3D<T,Descriptor>::processGenericBlocks (
        Box3D domain, std::vector<AtomicBlock3D*> blocks )
{
    PLB_PRECONDITION( blocks.size()==2 );
    ParticleField3D<T,Descriptor>& particleField =
        *dynamic_cast<ParticleField3D<T,Descriptor>*>(blocks[0]);
    BlockLattice3D<T,Descriptor>& fluid =
        *dynamic_cast<BlockLattice3D<T,Descriptor>*>(blocks[1]);
    /* Not used */
//    Dot3D offset = computeRelativeDisplacement(particleField, fluid);
    std::vector<Particle3D<T,Descriptor>*> particles;
    particleField.findParticles(domain, particles);
    std::vector<Dot3D> cellPos;
    std::vector<T> weights;
    std::vector<Cell<T,Descriptor>*> cells;
    for (pluint iParticle=0; iParticle<particles.size(); ++iParticle) {
        ImmersedCellParticle3D<T,Descriptor>* particle =
            dynamic_cast<ImmersedCellParticle3D<T,Descriptor>*> (particles[iParticle]);
        PLB_ASSERT( particle );
        Array<T,3> position(particle->getPosition());
        Array<T,3> velocity; velocity.resetToZero();
        cellPos = particle->getIBMcoordinates();
        weights = particle->getIBMweights();
        if (cellPos.size() == 0) {
            interpolationCoefficients(fluid, position, cellPos, weights, ibmKernel);
        }
        particle->get_v().resetToZero();
        for (pluint iCell=0; iCell < weights.size(); ++iCell) {
            velocity.resetToZero();
            Dot3D cellPosition = cellPos[iCell];
//            Dot3D cellPosition = cellPos[iCell] + offset;
            fluid.get(cellPosition.x, cellPosition.y, cellPosition.z).computeVelocity(velocity);
            particle->get_v() += weights[iCell] * velocity;
        }
        particle->get_vPrevious() = particle->get_v();
    }
}

template<typename T, template<typename U> class Descriptor>
FluidVelocityToImmersedCell3D<T,Descriptor>* FluidVelocityToImmersedCell3D<T,Descriptor>::clone() const {
    return new FluidVelocityToImmersedCell3D<T,Descriptor>(*this);
}

template<typename T, template<typename U> class Descriptor>
void FluidVelocityToImmersedCell3D<T,Descriptor>::getTypeOfModification (
        std::vector<modif::ModifT>& modified ) const
{
    modified[0] = modif::dynamicVariables; // Particle field.
    modified[1] = modif::nothing; // Fluid field.
}

template<typename T, template<typename U> class Descriptor>
void FluidVelocityToImmersedCell3D<T,Descriptor>::getModificationPattern(std::vector<bool>& isWritten) const {
    isWritten[0] = true;  // Particle field.
    isWritten[1] = false;  // Fluid field.
}


template<typename T, template<typename U> class Descriptor>
BlockDomain::DomainT FluidVelocityToImmersedCell3D<T,Descriptor>::appliesTo () const {
    return BlockDomain::bulk;
}


/* ******** ViscousPositionUpdate3D *********************************** */

template<typename T, template<typename U> class Descriptor>
ViscousPositionUpdate3D<T,Descriptor>::ViscousPositionUpdate3D (
        T ratio_) : ratio(ratio_) {};

template<typename T, template<typename U> class Descriptor>
ViscousPositionUpdate3D<T,Descriptor>::ViscousPositionUpdate3D (
        ViscousPositionUpdate3D<T,Descriptor> const& rhs) : ratio(rhs.ratio) {};

template<typename T, template<typename U> class Descriptor>
void ViscousPositionUpdate3D<T,Descriptor>::processGenericBlocks (
        Box3D domain, std::vector<AtomicBlock3D*> blocks )
{
    PLB_PRECONDITION( blocks.size()==2 );
    ParticleField3D<T,Descriptor>& particleField =
        *dynamic_cast<ParticleField3D<T,Descriptor>*>(blocks[0]);
    BlockLattice3D<T,Descriptor>& fluid =
        *dynamic_cast<BlockLattice3D<T,Descriptor>*>(blocks[1]);
    /* Not used */
//    Dot3D offset = computeRelativeDisplacement(particleField, fluid);
    std::vector<Particle3D<T,Descriptor>*> particles;
    particleField.findParticles(domain, particles);

    for (pluint iParticle=0; iParticle<particles.size(); ++iParticle) {
        ImmersedCellParticle3D<T,Descriptor>* particle =
            dynamic_cast<ImmersedCellParticle3D<T,Descriptor>*> (particles[iParticle]);
        PLB_ASSERT( particle );
        T dt = 1.0;
        particle->get_vPrevious() = particle->get_v() = ratio * 0.5 * particle->get_force() * dt * dt;
    }
}

template<typename T, template<typename U> class Descriptor>
ViscousPositionUpdate3D<T,Descriptor>* ViscousPositionUpdate3D<T,Descriptor>::clone() const {
    return new ViscousPositionUpdate3D<T,Descriptor>(*this);
}

template<typename T, template<typename U> class Descriptor>
void ViscousPositionUpdate3D<T,Descriptor>::getTypeOfModification (
        std::vector<modif::ModifT>& modified ) const
{
    modified[0] = modif::dynamicVariables; // Particle field.
    modified[1] = modif::nothing; // Fluid field.
}

template<typename T, template<typename U> class Descriptor>
void ViscousPositionUpdate3D<T,Descriptor>::getModificationPattern(std::vector<bool>& isWritten) const {
    isWritten[0] = true;  // Particle field.
    isWritten[1] = false;  // Fluid field.
}


template<typename T, template<typename U> class Descriptor>
BlockDomain::DomainT ViscousPositionUpdate3D<T,Descriptor>::appliesTo () const {
    return BlockDomain::bulk;
}



/* ******** ForceToFluid3D *********************************** */
template<typename T, template<typename U> class Descriptor>
ForceToFluid3D<T,Descriptor>::ForceToFluid3D (
        plint ibmKernel_) : ibmKernel(ibmKernel_) {};

template<typename T, template<typename U> class Descriptor>
void ForceToFluid3D<T,Descriptor>::processGenericBlocks (
        Box3D domain, std::vector<AtomicBlock3D*> blocks )
{
    PLB_PRECONDITION( blocks.size()==2 );
    ParticleField3D<T,Descriptor>& particleField =
        *dynamic_cast<ParticleField3D<T,Descriptor>*>(blocks[0]);
    BlockLattice3D<T,Descriptor>& fluid =
        *dynamic_cast<BlockLattice3D<T,Descriptor>*>(blocks[1]);
//    Dot3D offset = computeRelativeDisplacement(particleField, fluid); NOT USED

    std::vector<Particle3D<T,Descriptor>*> particles;
    particleField.findParticles(domain, particles);
    std::vector<Dot3D> cellPos;
    std::vector<T> weights;
    Cell<T,Descriptor>* cell;
    for (pluint iParticle=0; iParticle<particles.size(); ++iParticle) {
        ImmersedCellParticle3D<T,Descriptor>* particle =
            dynamic_cast<ImmersedCellParticle3D<T,Descriptor>*> (particles[iParticle]);
        PLB_ASSERT( particle );
        Array<T,3> position(particle->getPosition());
        cellPos = particle->getIBMcoordinates();
        weights = particle->getIBMweights();
        interpolationCoefficients(fluid, position, cellPos, weights, ibmKernel);
        Array<T,3> elasticForce = particle->get_force();
        // pcout << "elastic force: (" << elasticForce[0] << ", "<< elasticForce[1] << ", "<< elasticForce[2] << ")\n";
        for (pluint iCell = 0; iCell < weights.size(); ++iCell) {
            Dot3D cellPosition = cellPos[iCell];
            cell = &fluid.get(cellPosition.x, cellPosition.y, cellPosition.z);
            T *locForce = cell->getExternal(Descriptor<T>::ExternalField::forceBeginsAt);
            for (pluint iA = 0; iA < 3; ++iA) {
                locForce[iA] += (weights[iCell])*elasticForce[iA];
            }
        }
    }
}

template<typename T, template<typename U> class Descriptor>
ForceToFluid3D<T,Descriptor>* ForceToFluid3D<T,Descriptor>::clone() const {
    return new ForceToFluid3D<T,Descriptor>(*this);
}

template<typename T, template<typename U> class Descriptor>
void ForceToFluid3D<T,Descriptor>::getTypeOfModification (
        std::vector<modif::ModifT>& modified ) const
{
    modified[0] = modif::nothing; // Particle field.
    modified[1] = modif::staticVariables; // Fluid field.
}

template<typename T, template<typename U> class Descriptor>
void ForceToFluid3D<T,Descriptor>::getModificationPattern(std::vector<bool>& isWritten) const {
    isWritten[0] = false;  // Particle field.
    isWritten[1] = true;  // Fluid field.
}


template<typename T, template<typename U> class Descriptor>
BlockDomain::DomainT ForceToFluid3D<T,Descriptor>::appliesTo () const {
    return BlockDomain::bulkAndEnvelope;
}


/* ******** FillCellMap *********************************** */
template<typename T, template<typename U> class Descriptor>
void FillCellMap<T,Descriptor>::processGenericBlocks (
        Box3D domain, std::vector<AtomicBlock3D*> blocks )
{
    PLB_PRECONDITION( blocks.size()==1 );
    ParticleField3D<T,Descriptor>& particleField =
        *dynamic_cast<ParticleField3D<T,Descriptor>*>(blocks[0]);
    typename std::map<plint, Cell3D<T,Descriptor>* >::iterator iter;
    for (iter  = cellIdToCell3D.begin(); iter != cellIdToCell3D.end(); ++iter) {
        delete (iter->second);
    }
    cellIdToCell3D.clear();

    std::vector<Particle3D<T,Descriptor>*> found;
    particleField.findParticles(particleField.getBoundingBox(), found); // Gets the whole domain.

    for (pluint iParticle=0; iParticle<found.size(); ++iParticle) {
        ImmersedCellParticle3D<T,Descriptor>* particle = 
                dynamic_cast<ImmersedCellParticle3D<T,Descriptor>*> (found[iParticle]);

        plint cellId = particle->get_cellId();
        if (cellIdToCell3D.count(cellId) == 0) {
            cellIdToCell3D[cellId] = new Cell3D<T,Descriptor>(mesh, cellId);
        }
        cellIdToCell3D[cellId]->push_back(particle);
    }

    for (iter  = cellIdToCell3D.begin(); iter != cellIdToCell3D.end(); ++iter) {
        (iter->second)->close();
    }
}



template<typename T, template<typename U> class Descriptor>
FillCellMap<T,Descriptor>::FillCellMap (
	TriangularSurfaceMesh<T> & mesh_,
	std::map<plint, Cell3D<T,Descriptor>* > & cellIdToCell3D_)
: mesh(mesh_), cellIdToCell3D(cellIdToCell3D_) { }


template<typename T, template<typename U> class Descriptor>
FillCellMap<T,Descriptor>::~FillCellMap() { }


template<typename T, template<typename U> class Descriptor>
FillCellMap<T,Descriptor>::FillCellMap (
            FillCellMap<T,Descriptor> const& rhs)
    : mesh(rhs.mesh), cellIdToCell3D(rhs.cellIdToCell3D) { }


template<typename T, template<typename U> class Descriptor>
FillCellMap<T,Descriptor>*
    FillCellMap<T,Descriptor>::clone() const
{   return new FillCellMap<T,Descriptor>(*this);    }


template<typename T, template<typename U> class Descriptor>
BlockDomain::DomainT FillCellMap<T,Descriptor>::appliesTo() const 
{    return BlockDomain::bulk;   } // This changes in processGenericBlocks with getBoundingBox();


template<typename T, template<typename U> class Descriptor>
void FillCellMap<T,Descriptor>::getTypeOfModification (
        std::vector<modif::ModifT>& modified ) const
{    modified[0] = modif::nothing;  } // Particle field.

template<typename T, template<typename U> class Descriptor>
void FillCellMap<T,Descriptor>::getModificationPattern(std::vector<bool>& isWritten) const {
    isWritten[0] = false;  // Particle field.
}




/* ******** CountGlobalNumberOfCells *********************************** */
template<typename T, template<typename U> class Descriptor>
CountGlobalNumberOfCells<T,Descriptor>::CountGlobalNumberOfCells(pluint numberOfCells_)
    : numberOfCells(numberOfCells_)
{
    qId  = this->getStatistics().subscribeIntSum();
}


template<typename T, template<typename U> class Descriptor>
void CountGlobalNumberOfCells<T,Descriptor>::processGenericBlocks (
        Box3D domain, std::vector<AtomicBlock3D*> blocks )
{
    this->getStatistics().gatherIntSum(qId, numberOfCells);
}


template<typename T, template<typename U> class Descriptor>
pluint CountGlobalNumberOfCells<T,Descriptor>::getValue() {
    return pluint(this->getStatistics().getIntSum(qId));
}



/* ******** ComputeRequiredQuantities *********************************** */

template<typename T, template<typename U> class Descriptor>
void ComputeRequiredQuantities<T,Descriptor>::processGenericBlocks (
        Box3D domain, std::vector<AtomicBlock3D*> blocks )
{
    PLB_PRECONDITION( blocks.size()==2 );
    ParticleField3D<T,Descriptor>& particleField =
        *dynamic_cast<ParticleField3D<T,Descriptor>*>(blocks[0]);
    ParticleField3D<T,Descriptor>& reductionParticleField =
        *dynamic_cast<ParticleField3D<T,Descriptor>*>(blocks[1]);

    std::vector<Particle3D<T,Descriptor>*> particles;
    particleField.findParticles(domain, particles); // Gets particle only from the bulk

    std::vector<Particle3D<T,Descriptor>*> reductionParticles;
    reductionParticleField.removeParticles(reductionParticleField.getBoundingBox());

    typename std::map<plint, Cell3D<T,Descriptor>*  >::iterator iter;
    for (iter  = cellIdToCell3D.begin(); iter != cellIdToCell3D.end(); ++iter) {
        (iter->second)->clearReducer();
    }
    std::map<plint, pluint> particlesPerCellId;
    for (pluint iParticle=0; iParticle<particles.size(); ++iParticle) {
        plint cellId = castParticleToICP3D(particles[iParticle])->get_cellId();
        particlesPerCellId[cellId] = particlesPerCellId[cellId] + 1;
        for (std::vector<plint>::iterator i = ccrRequirements.begin(); i != ccrRequirements.end(); ++i)
        {
            plint ccrId = *i;
            typename std::map<plint, Cell3D<T,Descriptor>* >::iterator iter = cellIdToCell3D.find(cellId);
            if (iter != cellIdToCell3D.end()) {
                (iter->second)->computeCCRQuantities(ccrId, particles[iParticle]);
            }
        }
    }

    for (iter  = cellIdToCell3D.begin(); iter != cellIdToCell3D.end(); ++iter) {
        plint cellId = iter->first;
        Cell3D<T,Descriptor> * cell = iter->second;
        cell->closeCCRQuantities();
        Array<T,3> const& vertex = cell->get3D(CCR_NO_PBC_POSITION_MEAN);
        ReductionParticle3D<T,Descriptor>* rParticle = new ReductionParticle3D<T,Descriptor>(cellId, vertex);
        rParticle->get_nParticles() = particlesPerCellId[cellId];
        rParticle->updateCQH(*cell);
        cell->clearQuantities();
        reductionParticleField.addParticle(reductionParticleField.getBoundingBox(), rParticle);
//        std::cout << MPI::COMM_WORLD.Get_rank() << " Cell Volume " << rParticle->getVolume()
//                		<< " surface " << rParticle->getSurface()
//                		<< " CCR_ANGLE_MEAN " << rParticle->getMeanAngle()
//                        << " getMeanEdgeLength " << rParticle->getMeanEdgeLength()
//                        << " getEnergy " << rParticle->getEnergy()
//                		<< " CCR_FORCE (" << rParticle->getForce()[0]
//						<< ", " << rParticle->getForce()[1]
//						<< ", " << rParticle->getForce()[2] << ") "
//        		<< " rPart "  << rParticle->get_nParticles() << std::endl;

    }
}



template<typename T, template<typename U> class Descriptor>
ComputeRequiredQuantities<T,Descriptor>::ComputeRequiredQuantities (
    std::vector<plint> ccrRequirements_, 
    std::map<plint, Cell3D<T,Descriptor>* > & cellIdToCell3D_)
: ccrRequirements(ccrRequirements_), cellIdToCell3D(cellIdToCell3D_) { }


template<typename T, template<typename U> class Descriptor>
ComputeRequiredQuantities<T,Descriptor>::~ComputeRequiredQuantities() { }


template<typename T, template<typename U> class Descriptor>
ComputeRequiredQuantities<T,Descriptor>::ComputeRequiredQuantities (
            ComputeRequiredQuantities<T,Descriptor> const& rhs)
    : ccrRequirements(rhs.ccrRequirements), cellIdToCell3D(rhs.cellIdToCell3D) { }


template<typename T, template<typename U> class Descriptor>
ComputeRequiredQuantities<T,Descriptor>*
    ComputeRequiredQuantities<T,Descriptor>::clone() const
{ return new ComputeRequiredQuantities<T,Descriptor>(*this); }


template<typename T, template<typename U> class Descriptor>
BlockDomain::DomainT ComputeRequiredQuantities<T,Descriptor>::appliesTo() const
{    return BlockDomain::bulk;    }


template<typename T, template<typename U> class Descriptor>
void ComputeRequiredQuantities<T,Descriptor>::getTypeOfModification (
    std::vector<modif::ModifT>& modified ) const  {
    modified[0] = modif::nothing; // Particle field.
    modified[1] = modif::allVariables; // Reduction particles;
}

template<typename T, template<typename U> class Descriptor>
void ComputeRequiredQuantities<T,Descriptor>::getModificationPattern(std::vector<bool>& isWritten) const {
    isWritten[0] = false;  // Particle field.
    isWritten[1] = true;  // Reduction field.
}



/* ******** SyncCellQuantities *********************************** */

template<typename T, template<typename U> class Descriptor>
SyncCellQuantities<T,Descriptor>::SyncCellQuantities (std::map<plint, Cell3D<T,Descriptor>* > & cellIdToCell3D_)
: cellIdToCell3D(cellIdToCell3D_) { }

template<typename T, template<typename U> class Descriptor>
SyncCellQuantities<T,Descriptor>::SyncCellQuantities (SyncCellQuantities<T,Descriptor> const& rhs)
: cellIdToCell3D(rhs.cellIdToCell3D) { }



template<typename T, template<typename U> class Descriptor>
void SyncCellQuantities<T,Descriptor>::processGenericBlocks (
        Box3D domain, std::vector<AtomicBlock3D*> blocks )
{
        PLB_PRECONDITION( blocks.size()==1 );
        ParticleField3D<T,Descriptor>& reductionParticleField
            = *dynamic_cast<ParticleField3D<T,Descriptor>*>(blocks[0]);
        std::vector<Particle3D<T,Descriptor>*> reductionParticles;
        reductionParticleField.findParticles(reductionParticleField.getBoundingBox(), reductionParticles);

        std::map< plint, std::map<plint, bool> > particleInProcessorAndCellid; // processorCellIdMap[processor][cellId] = true
        for (pluint iA = 0; iA < reductionParticles.size(); ++iA) {
            ReductionParticle3D<T,Descriptor>* particle =
                    dynamic_cast<ReductionParticle3D<T,Descriptor>*> (reductionParticles[iA]);

            plint cellId = particle->get_cellId();
            plint processor = particle->get_processor();
            plint nParticles = particle->get_nParticles();
            particleInProcessorAndCellid[processor]; // Create "processor" entry
            if (cellIdToCell3D.count(cellId)) {
                Cell3D<T,Descriptor> * chq = cellIdToCell3D[cellId];
                if (particleInProcessorAndCellid[processor].find(cellId) == particleInProcessorAndCellid[processor].end()) {
                    particleInProcessorAndCellid[processor][cellId] = true;
                    std::map<plint, T > const& q1d = particle->getQuantities1D();
                    std::map<plint, Array<T,3> > const& q3d = particle->getQuantities3D();
                    std::map<plint, std::vector<T> > const& qNd = particle->getQuantitiesND();
                    typename std::map<plint, T >::const_iterator iter1D;
                    for (iter1D  = q1d.begin(); iter1D != q1d.end(); ++iter1D) {
                        chq->reduceQuantity(iter1D->first, iter1D->second, nParticles);
                    }
                    typename std::map<plint, Array<T,3> >::const_iterator iter3D;
                    for (iter3D  = q3d.begin(); iter3D != q3d.end(); ++iter3D) {
                        chq->reduceQuantity(iter3D->first, iter3D->second, nParticles);
                    }
                    typename std::map<plint, std::vector<T> >::const_iterator iterND;
                    for (iterND  = qNd.begin(); iterND != qNd.end(); ++iterND) {
                        chq->reduceQuantity(iterND->first, iterND->second, nParticles);
                    }
                    chq->getParticlesPerCellId() += nParticles;
                }
            }
        }
        reductionParticleField.removeParticles(reductionParticleField.getBoundingBox(), -1);

        typename std::map<plint, Cell3D<T,Descriptor>*  >::iterator iter;
        for (iter  = cellIdToCell3D.begin(); iter != cellIdToCell3D.end(); ++iter) {
            Cell3D<T,Descriptor> & cell = *(iter->second);
            if ( cell.count(CCR_INERTIA) ) {
                std::vector<T> & cellInertia = cell.getInertia();
                std::vector<T> ellipsoidAngles;
                std::vector<T> ellipsoidSemiAxes;
                T difference, cellVolume = cell.getVolume();
                computeEllipsoidFit (cellInertia, ellipsoidAngles, ellipsoidSemiAxes, difference, cellVolume);

                T currMaxDiameter =  max(max(ellipsoidSemiAxes[0], ellipsoidSemiAxes[1]),  ellipsoidSemiAxes[2]) * 2.0;
                T currMinDiameter =  min(min(ellipsoidSemiAxes[0], ellipsoidSemiAxes[1]),  ellipsoidSemiAxes[2]) * 2.0;

                cell.getTumblingAngles() = Array<T,3>(ellipsoidAngles[0], ellipsoidAngles[1], ellipsoidAngles[2]);
                cell.getDiameters() = Array<T,3>(ellipsoidSemiAxes[0], ellipsoidSemiAxes[1], ellipsoidSemiAxes[2]) * 2.0;
                cell.getSymmetryDeviation() = difference;
                cell.getTaylorDeformationIndex() = (currMaxDiameter - currMinDiameter)*1.0/(currMaxDiameter + currMinDiameter);
            }
        }
}

template<typename T, template<typename U> class Descriptor>
SyncCellQuantities<T,Descriptor>*
    SyncCellQuantities<T,Descriptor>::clone() const
{
    return new SyncCellQuantities<T,Descriptor>(*this);
}


template<typename T, template<typename U> class Descriptor>
BlockDomain::DomainT SyncCellQuantities<T,Descriptor>::appliesTo() const {
    return BlockDomain::bulk;
}


template<typename T, template<typename U> class Descriptor>
void SyncCellQuantities<T,Descriptor>::getTypeOfModification (
        std::vector<modif::ModifT>& modified ) const
{
    modified[0] = modif::allVariables; // Reduction Particle field.
}

template<typename T, template<typename U> class Descriptor>
void SyncCellQuantities<T,Descriptor>::getModificationPattern(std::vector<bool>& isWritten) const {
    isWritten[0] = true;  // Reduction Particle field.
}


/* ================================================================================ */
/* ******** computeEllipsoidFit *********************************** */
/* ================================================================================ */
template<typename T>
void computeEllipsoidFit (std::vector<T> & cellInertia,
                          std::vector<T> & cellsEllipsoidFitAngles,
                          std::vector<T> & cellsEllipsoidFitSemiAxes,
                          T & difference,
                          T cellVolume)
{
    vector<T> semiAxes, ellipsoidAngles;
    T dif;
    getLambdasAndAngles(cellInertia, semiAxes, ellipsoidAngles, dif);

    T f1 = semiAxes[0] * 5 / cellVolume;
    T f2 = semiAxes[1] * 5 / cellVolume;
    T f3 = semiAxes[2] * 5 / cellVolume;
    semiAxes[0] = (sqrt( (f1 + f2 - f3)/2 ));
    semiAxes[1] = (sqrt( (f2 + f3 - f1)/2 ));
    semiAxes[2] = (sqrt( (f3 + f1 - f2)/2 ));
    std::sort(semiAxes.begin(), semiAxes.end());

    cellsEllipsoidFitSemiAxes = semiAxes;
    cellsEllipsoidFitAngles = ellipsoidAngles;
    difference = dif;
}


#endif  // CELL_FIELD_FUNCTIONALS_3D_HH

