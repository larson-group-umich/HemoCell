#ifndef CELLFIELDS3D_H
#define CELLFIELDS3D_H

class CellFields3D;
#include "palabos3D.h"
#include "constant_defaults.h"
#include "palabos3D.hh"
#include "cell3D.h"
#include "cellModel3D.h"
#include "hemoCellParticleField3D.h"
#include "shapeMemoryModel3D.h"
#include "cellReductionTypes.h"
#include "cellFieldFunctionals3D.h"
#include "cellCellForces3D.h"
#include "fcnGenericFunctions.h"
#include <sys/stat.h>
#include <unistd.h>
#include <set>
#include <string>
#include <algorithm>    // std::sort


using namespace std;
using namespace plb;

class HemoCellField;

/*
 * This class can contain many cellfields, it keeps track of all the particles
 * in all the cellfields. The option exists to get exclusive access to a single
 * cellfield if necessary.`
 * 
 * A cellField should have an local update and a between cell update function
 *
 * The between cell interaction might be different for different type of
 * particles, they can be set in a special map
 *
 *
 * TODO: light and heavy (tracking cellfield arrays to particles or not)
 *
 */
class CellFields3D
{
public:
	CellFields3D(MultiBlockLattice3D<double, DESCRIPTOR> & lattice_, unsigned int particleEnvelopeWidth);
    MultiParticleField3D<HEMOCELL_PARTICLE_FIELD> & getParticleField3D() { return *immersedParticles; };
    virtual void advanceParticles();
    virtual void spreadForceIBM();
    virtual void interpolateVelocityIBM();
    virtual void applyConstitutiveModel();

    void addCellType(TriangularSurfaceMesh<double> & meshElement, double hematocrit, ShellModel3D<double> * cellmodel, std::string name_);
	  void setParticleUpdateScheme (double _cellTimeStep=1.0); //For decoupled update schemes 

    void readPositionsCellFields(std::string particlePosFile);
    /*Checkpoint functions*/
    void copyXMLreader2XMLwriter(XMLreader const& reader, XMLwriter & writer);
    void copyXMLreader2XMLwriter(XMLreaderProxy readerProxy, XMLwriter & writer);
    void load(XMLreader * documentXML, plint & iter);
    void save(XMLreader * documentXML, plint iter);
    void InitAfterLoadCheckpoint();

    double getMaximumForce_Global() {return 0;}

    unsigned int size();

    HemoCellField * operator[](unsigned int index);

    void setFluidExternalForce(double poiseuilleForce) {}

	MultiBlockLattice3D<double, DESCRIPTOR> & lattice;
  vector<HemoCellField> cellFields;
	MultiParticleField3D<HEMOCELL_PARTICLE_FIELD> * immersedParticles;
	MultiParticleField3D<HEMOCELL_PARTICLE_FIELD> * reductionParticles;
  double cellTimeStep;
  void synchronizeCellQuantities(SyncRequirements _dummy) {}
};

/*contains information about one particular cellfield, structlike*/
class HemoCellField{
  static vector<int> default_output;
  public:
  HemoCellField(CellFields3D& cellFields_, Cell3D<double,DESCRIPTOR> cell3D_, TriangularSurfaceMesh<double>& meshElement_)
      :cellFields(cellFields_), desiredOutputVariables(default_output),
       cell3D(cell3D_), meshElement(meshElement_) {numVertex = meshElement.getNumVertices();};
  double getVolumeFraction() { return hematocrit;}
  double hematocrit;
  ShellModel3D<double> * model;
  TriangularSurfaceMesh<double> & getMesh() { return meshElement;}
  std::string name;
  int ctype;
  int numVertex;
  CellFields3D & cellFields;
  vector<int> & desiredOutputVariables;
  Cell3D<double,DESCRIPTOR> & cell3D;
  TriangularSurfaceMesh<double> & meshElement;
  MultiParticleField3D<HEMOCELL_PARTICLE_FIELD> * getParticleField3D() {return cellFields.immersedParticles;}
  MultiBlockLattice3D<double,DESCRIPTOR> * getFluidField3D() {return &(cellFields.lattice);}
  int getNumberOfCells_Global() {return 0;}
  std::string getIdentifier() {return name;}
  Box3D getBoundingBox() { return cellFields.immersedParticles->getBoundingBox(); }
  MultiParticleField3D<HEMOCELL_PARTICLE_FIELD> * getParticleArg() { return cellFields.immersedParticles; }
  std::map<plint, Cell3D<double,DESCRIPTOR>* > getCellIdToCell3D() { std::map<plint,Cell3D<double,DESCRIPTOR>* > tmp; return tmp ;}
  void synchronizeSyncRequirements(SyncRequirements _dummy) {}
  void setOutputVariables(const vector<int> & outputs) { desiredOutputVariables = outputs;}


};

vector<int> HemoCellField::default_output ({OUTPUT_POSITION});
#endif
