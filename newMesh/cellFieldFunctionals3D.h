#ifndef CELL_FIELD_FUNCTIONALS_3D_H
#define CELL_FIELD_FUNCTIONALS_3D_H
#include "palabos3D.h"
#include "palabos3D.hh"
#include "immersedBoundaryMethod3D.h"
#include "immersedCellParticle3D.h"
#include "reductionParticle3D.h"
#include "cell3D.h"

using namespace std;
using namespace plb;


template<typename T, template<typename U> class Descriptor>
class ForceToFluid3D : public BoxProcessingFunctional3D
{
public:
    ForceToFluid3D (plint ibmKernel_=4 );
    /// Arguments: [0] Particle-field. [1] Lattice.
    virtual void processGenericBlocks(Box3D domain, std::vector<AtomicBlock3D*> fields);
    virtual ForceToFluid3D<T,Descriptor>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
private:
    plint ibmKernel;
};


template<typename T, template<typename U> class Descriptor>
class FluidVelocityToImmersedCell3D : public BoxProcessingFunctional3D
{
public:
    FluidVelocityToImmersedCell3D (plint ibmKernel_=4 );
    /// Arguments: [0] Particle-field. [1] Lattice.
    virtual void processGenericBlocks(Box3D domain, std::vector<AtomicBlock3D*> fields);
    virtual FluidVelocityToImmersedCell3D<T,Descriptor>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
private:
    plint ibmKernel;
};


template<typename T, template<typename U> class Descriptor>
class ComputeCellForce3D : public BoxProcessingFunctional3D
{
public:
    ComputeCellForce3D (ConstitutiveModel<T,Descriptor>* cellModel_, std::map<plint, Cell3D<T,Descriptor> > & cellIdToCell3D_) ;
    ~ComputeCellForce3D();
    ComputeCellForce3D(ComputeCellForce3D<T,Descriptor> const& rhs);
    /// Arguments: [0] Particle-field
    virtual void processGenericBlocks(Box3D domain, std::vector<AtomicBlock3D*> fields);
    virtual ComputeCellForce3D<T,Descriptor>* clone() const;
    virtual void getModificationPattern(std::vector<bool>& isWritten) const;
    virtual BlockDomain::DomainT appliesTo() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
private:
    ConstitutiveModel<T,Descriptor>* cellModel;
    std::map<plint, Cell3D<T,Descriptor> > & cellIdToCell3D;
};


template<typename T, template<typename U> class Descriptor>
class FillCellMap : public BoxProcessingFunctional3D
{
public:
    FillCellMap (TriangularSurfaceMesh<T> & mesh_, std::map<plint, Cell3D<T,Descriptor> > & cellIdToCell3D_);
    virtual ~FillCellMap();
    FillCellMap(FillCellMap<T,Descriptor> const& rhs);
    /// Arguments: [0] Particle-field
    virtual void processGenericBlocks(Box3D domain, std::vector<AtomicBlock3D*> fields);
    virtual FillCellMap<T,Descriptor>* clone() const;
    virtual BlockDomain::DomainT appliesTo() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
private:
    TriangularSurfaceMesh<T> & mesh;
	std::map<plint, Cell3D<T,Descriptor> > & cellIdToCell3D;
};


template<typename T, template<typename U> class Descriptor>
class ComputeRequiredQuantities : public BoxProcessingFunctional3D
{
public:
    ComputeRequiredQuantities (std::vector<plint> ccrRequirements_, std::map<plint, Cell3D<T,Descriptor> > & cellIdToCell3D_);
    virtual ~ComputeRequiredQuantities();
    ComputeRequiredQuantities(ComputeRequiredQuantities<T,Descriptor> const& rhs);
    /// Arguments: [0] Particle-field
    virtual void processGenericBlocks(Box3D domain, std::vector<AtomicBlock3D*> fields);
    virtual ComputeRequiredQuantities<T,Descriptor>* clone() const;
    virtual BlockDomain::DomainT appliesTo() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
private:
    std::vector<plint> ccrRequirements;
    std::map<plint, Cell3D<T,Descriptor> > & cellIdToCell3D;
};


template<typename T, template<typename U> class Descriptor>
class SyncCellQuantities : public BoxProcessingFunctional3D
{
public:
    SyncCellQuantities (std::map<plint, Cell3D<T,Descriptor> > & cellIdToCell3D_);
    virtual ~SyncCellQuantities() { };
    SyncCellQuantities(SyncCellQuantities<T,Descriptor> const& rhs);
    /// Arguments: [0] Particle-field
    virtual void processGenericBlocks(Box3D domain, std::vector<AtomicBlock3D*> fields);
    virtual SyncCellQuantities<T,Descriptor>* clone() const;
    virtual BlockDomain::DomainT appliesTo() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
private:
    std::map<plint, Cell3D<T,Descriptor> > & cellIdToCell3D;
};


#include "cellFieldFunctionals3D.hh"
#endif  // CELL_FIELD_FUNCTIONALS_3D_H

