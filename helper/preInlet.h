/*
This file is part of the HemoCell library

HemoCell is developed and maintained by the Computational Science Lab 
in the University of Amsterdam. Any questions or remarks regarding this library 
can be sent to: info@hemocell.eu

When using the HemoCell library in scientific work please cite the
corresponding paper: https://doi.org/10.3389/fphys.2017.00563

The HemoCell library is free software: you can redistribute it and/or
modify it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

The library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef PREINLET_H
#define PREINLET_H

class preInlet;

enum Direction : int {
  Xpos, Xneg, Ypos, Yneg, Zpos, Zneg
};
  
#include "hemocell.h"

#define DSET_SLICE 1000

namespace hemo {


inline plint cellsInBoundingBox(Box3D const & box) {
  return abs((box.x1 - box.x0)*(box.y1-box.y0)*(box.z1-box.z0));
}

class preInlet {
public:
  class CreatePreInletBoundingBox: public HemoCellFunctional {
    void processGenericBlocks(plb::Box3D, std::vector<plb::AtomicBlock3D*>);
    CreatePreInletBoundingBox * clone() const;
    Box3D & boundingBox;
    bool & foundPreInlet = false;
    public:
      CreatePreInletBoundingBox(Box3D & b_, bool & fp_ ) :
                                boundingBox(b_), foundPreInlet(fp_) {}
  };
  
  preInlet(MultiScalarField3D<T> & flagMatrix);
  inline plint getNumberOfNodes() { return cellsInBoundingBox(location);}
  plb::Box3D location;
  vector<vector<bool>> fluidslice;
  int nProcs = 0;
  map<plint,plint> BlockToMpi;
};
  
struct particle_hdf5_t {
  float location[3];
  float velocity[3];
  int cell_id;
  int vertex_id;
  unsigned int particle_type;
};

class createPreInlet {
  class createPreInletFunctional: public HemoCellFunctional {
    createPreInlet & parent;
    void processGenericBlocks(Box3D, std::vector<AtomicBlock3D*>);
    createPreInletFunctional * clone() const;
  public:
    createPreInletFunctional(createPreInlet & parent_) : parent(parent_) {}
  };

  HemoCell & hemocell;
  Box3D domain;
  Box3D fluidDomain;
  string outputFileName;
  int particlePositionTimeStep;
  Direction flowDir;
  plint file_id, dataspace_velocity_id,dataset_velocity_id = -1,plist_dataset_collective_id;
  plint particle_type_mem, particle_type_h5;
  bool reducedPrecision;
  int reducedPrecisionDirection;
  int desired_iterations;
  //ofstream counter;
  int current_velocity_field = -1;  
public:
  createPreInlet(Box3D domain, string outputFileName, int particlePositionTimestep, Direction flow_direction, HemoCell & hemocell, int desired_iterations, bool reducedPrecision_ = false);
  void saveCurrent();
  ~createPreInlet();
};



class PreInlet {
  class PreInletFunctional: public HemoCellFunctional {
    PreInlet & parent;
    void processGenericBlocks(Box3D, std::vector<AtomicBlock3D*>);
    PreInletFunctional * clone() const;
  public:
    PreInletFunctional(PreInlet & parent_) : parent(parent_) {}
  };
  class DeletePreInletParticles: public HemoCellFunctional {
    void processGenericBlocks(Box3D, std::vector<AtomicBlock3D*>);
    DeletePreInletParticles * clone() const;
  };
  class ImmersePreInletParticles: public HemoCellFunctional {
    void processGenericBlocks(Box3D, std::vector<AtomicBlock3D*>);
    ImmersePreInletParticles * clone() const;
  };
  
  HemoCell & hemocell;
  Box3D domain;
  Box3D fluidDomain;
  string sourceFileName;
  int particlePositionTimeStep;
  Direction flowDir;
  plint file_id,dataspace_velocity_id,dataset_velocity_id = -1,dataset_particles_id,dataspace_particles_id;
  int current_velocity_field = -1;
  plint particle_type_mem, particle_type_h5, particles_size;
  particle_hdf5_t * particles;
  int nCellsOffset, nCellsSelf;
  //ifstream counter;
  bool reducedPrecision;
  int reducedPrecisionDirection;


public:  
  PreInlet(Box3D domain, string sourceFileName, int particlePositionTimestep, Direction flow_direction, HemoCell & hemocell, bool reducedPrecision_ = false);
  void update();
  ~PreInlet();
};

}
#endif /* PREINLET_H */

