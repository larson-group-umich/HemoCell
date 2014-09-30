#ifndef CELL_REDUCTION_TYPES_H
#define CELL_REDUCTION_TYPES_H

#include "palabos3D.h"
#include "palabos3D.hh"
#include <map>
#include <set>
#include <string>

/* IDs for the reduction types,
 * they are created based on the following rules:
 * ==============================
 * Last digit, Dimension:
 *      1D : 1
 *      2D : 2
 *      3D : 3
 *      ND : 4,5,6,7,8,9 // Not above 10
 * Second to last digit, type of reduction:
 *      Sum  : 0
 *      Mean : 1
 *      Min  : 2
 *      Max  : 3
 *      STD  : 4 // Still to be implemented
 *      Save:  5 // Only for data saving
 * ID for the quantity of interest, starting from 1. Can be grouped together like:
 *      Volume         : 1
 *      Angle          : 2
 *      Area           : 3
 *      Edge Distance  : 4
 *      Edge Tile Span : 5
 *      Position       : 6 // Periodic boundary position
 *      Velocity       : 7
 *      Inertia        : 8
 *      Energy         : 9
 *      Force          : 16
 *      Torque         : 17
 *      Position       : 0
 */
#define CCR_NO_PBC_POSITION_MEAN    13 // 3d // BEWARE OF 0 IN FRONT! GOES TO OCT
#define CCR_NO_PBC_POSITION_MIN     23 // 3d // BEWARE OF 0 IN FRONT! GOES TO OCT
#define CCR_NO_PBC_POSITION_MAX     33 // 3d // BEWARE OF 0 IN FRONT! GOES TO OCT
#define CCR_VOLUME                 101 // 1d
#define CCR_ANGLE_MEAN             211 // 1d
#define CCR_ANGLE_MIN              221 // 1d
#define CCR_ANGLE_MAX              231 // 1d
#define CCR_SURFACE                301 // 1d
#define CCR_TRIANGLE_AREA_MEAN     311 // 1d, Better use 301 and divide by the number of triangles
#define CCR_TRIANGLE_AREA_MIN      321 // 1d
#define CCR_TRIANGLE_AREA_MAX      331 // 1d
#define CCR_EDGE_DISTANCE_MEAN     411 // 1d
#define CCR_EDGE_DISTANCE_MIN      421 // 1d
#define CCR_EDGE_DISTANCE_MAX      431 // 1d
#define CCR_TILE_SPAN_MEAN         511 // 1d
#define CCR_TILE_SPAN_MIN          521 // 1d
#define CCR_TILE_SPAN_MAX          531 // 1d
#define CCR_POSITION_MEAN          613 // 3d
#define CCR_POSITION_MIN           623 // 3d
#define CCR_POSITION_MAX           633 // 3d
#define CCR_VELOCITY_MEAN          713 // 3d
#define CCR_VELOCITY_MIN           723 // 3d
#define CCR_VELOCITY_MAX           733 // 3d
#define CCR_ENERGY                 901 // 1d
#define CCR_FORCE                 1603 // 1d
#define CCR_INERTIA                809 // 9d, Not working
#define CCR_TORQUE                1703 // 3d

// The following are not calculated
#define CCR_TUMBLING_ANGLES       1053 // 1d
#define CCR_TANK_TREADING_ANGLES  1153 // 1d
#define CCR_DIAMETERS             1253 // 1d
#define CCR_SYMMETRY_DEVIATION    1351 // 1d
#define CCR_DEFORMATION_INDEX     1451 // 1d
#define CCR_TAYLOR_DEFORMATION_INDEX             1551 // 1d


using namespace plb;
using namespace std;


// Reduction type (min,max,etc): second to last digit
plint getReductionType(plint ccrId) { return (ccrId%100)/10; }

// Reduction dimension (1,3,N): last digit
plint getReductionDimension(plint ccrId) { return ccrId%10; }

// Reduction quantity (position, velocity): last digit
plint getReductionQuantity(plint ccrId) { return ccrId/100; }



template<typename T>
class BlockStatisticsCCR
{
public:
    BlockStatisticsCCR() {};
    ~BlockStatisticsCCR() {};

public:
    void clear() { sumV.clear(); averageV.clear(); averageV.clear(); averageQV.clear(); ccrIdToBin.clear(); ccrIds.clear(); } ;
    void subscribe(plint ccrId);
    void gather(plint ccrId, T value, pluint qBin);
    void gather(plint ccrId, T value);
    void gather(plint ccrId, Array<T,3> const& value);
    void gather(plint ccrId, std::vector<T> const& value);

    void get(plint ccrId, T &value, pluint qBin);
    void get(plint ccrId, T & value);
    void get(plint ccrId, Array<T,3> & value);
    void get(plint ccrId, std::vector<T> & value);
    std::vector<plint> & get_ccrIds() { return ccrIds; };

public:
    plint subscribeSum() { sumV.push_back(T()); return sumV.size() - 1; } ;
    plint subscribeAverage() { averageV.push_back(T()); averageQV.push_back(0); return averageV.size()  - 1; } ;
    plint subscribeMax() { maxV.push_back( -std::numeric_limits<double>::max() ); return maxV.size() - 1; } ;

    void gatherSum(plint qBin, T value) { sumV[qBin] += value; } ;
    void gatherAverage(plint qBin, T value) { averageV[qBin] += value; averageQV[qBin]+=1; } ;
    void gatherMax(plint qBin, T value) { maxV[qBin] = max(maxV[qBin], value); } ;

    T getSum(plint qBin) { return sumV[qBin]; } ;
    T getAverage(plint qBin) { return averageV[qBin]*1.0/averageQV[qBin]; } ;
    T getMax(plint qBin) { return maxV[qBin]; } ;
private:
    std::vector<T> sumV, averageV, maxV;
    std::vector<plint> averageQV;
    std::map<plint, std::vector<pluint> > ccrIdToBin;
    std::vector<plint> ccrIds;
};




class SyncRequirements
{
public:
    SyncRequirements();
    ~SyncRequirements();

    virtual std::set<plint> getSyncRequirementsSet() { return ccrRequirements; }
    virtual std::vector<plint> getSyncRequirements() {
        std::vector<plint> ccrRequirementsVector(ccrRequirements.begin(), ccrRequirements.end()); 
        return ccrRequirementsVector; 
    }


    virtual void insert(std::set<plint> const& ccrReq) {
        for (std::set<plint>::const_iterator it=ccrReq.begin(); it!=ccrReq.end(); ++it) 
            {   ccrRequirements.insert(*it);    }
    }

    virtual void insert(std::vector<plint> const& ccrReq) {
        for (int iV = 0; iV < ccrReq.size(); ++iV)
        { ccrRequirements.insert( ccrReq[iV] ); }
    }

private:
    std::set<plint> ccrRequirements;
};




const plb::plint allReductions_array[] = {CCR_NO_PBC_POSITION_MEAN, CCR_NO_PBC_POSITION_MIN, CCR_NO_PBC_POSITION_MAX,
                                     CCR_VOLUME,
                                     CCR_ANGLE_MEAN, CCR_ANGLE_MIN, CCR_ANGLE_MAX,
                                     CCR_SURFACE, CCR_TRIANGLE_AREA_MEAN, CCR_TRIANGLE_AREA_MIN, CCR_TRIANGLE_AREA_MAX,
                                     CCR_EDGE_DISTANCE_MEAN, CCR_EDGE_DISTANCE_MIN, CCR_EDGE_DISTANCE_MAX,
                                     CCR_TILE_SPAN_MEAN, CCR_TILE_SPAN_MIN, CCR_TILE_SPAN_MAX,
                                     CCR_POSITION_MEAN, CCR_POSITION_MIN, CCR_POSITION_MAX,
                                     CCR_VELOCITY_MEAN, CCR_VELOCITY_MIN, CCR_VELOCITY_MAX,
                                     /* CCR_INERTIA, */ // CellInertia cannot be calculated at once.
                                     /* CCR_TORQUE, */  // Neither can torque.
                                     CCR_ENERGY, CCR_FORCE};

const plb::plint volumeAndSurfaceAndCentersReductions_array[] = {CCR_VOLUME, CCR_SURFACE, CCR_POSITION_MEAN};
const plb::plint volumeAndSurfaceReductions_array[] = {CCR_VOLUME, CCR_SURFACE};


std::vector<plb::plint> const allReductions(allReductions_array, allReductions_array + sizeof(allReductions_array) / sizeof(allReductions_array[0]) );
std::vector<plb::plint> const volumeAndSurfaceAndCentersReductions(volumeAndSurfaceAndCentersReductions_array, volumeAndSurfaceAndCentersReductions_array + sizeof(volumeAndSurfaceAndCentersReductions_array) / sizeof(volumeAndSurfaceAndCentersReductions_array[0]) );
std::vector<plb::plint> const volumeAndSurfaceReductions(volumeAndSurfaceReductions_array, volumeAndSurfaceReductions_array + sizeof(volumeAndSurfaceReductions_array) / sizeof(volumeAndSurfaceReductions_array[0]) );

std::set<plb::plint> const setAllReductions(allReductions_array, allReductions_array + sizeof(allReductions_array) / sizeof(allReductions_array[0]) );
std::set<plb::plint> const setVolumeAndSurfaceAndCentersReductions(volumeAndSurfaceAndCentersReductions_array, volumeAndSurfaceAndCentersReductions_array + sizeof(volumeAndSurfaceAndCentersReductions_array) / sizeof(volumeAndSurfaceAndCentersReductions_array[0]) );
std::set<plb::plint> const setVolumeAndSurfaceReductions(volumeAndSurfaceReductions_array, volumeAndSurfaceReductions_array + sizeof(volumeAndSurfaceReductions_array) / sizeof(volumeAndSurfaceReductions_array[0]) );

std::map<int, std::string> createMapCCR() {
    std::map<int, std::string> ccrNames;
    ccrNames[CCR_VOLUME] = "Volume";
    ccrNames[CCR_SURFACE] = "Surface";

    ccrNames[CCR_NO_PBC_POSITION_MEAN] = "Position (not periodic)";
    ccrNames[CCR_NO_PBC_POSITION_MIN] = "Min positions (not periodic)";
    ccrNames[CCR_NO_PBC_POSITION_MAX] = "Max positions (not periodic)";


    ccrNames[CCR_ANGLE_MEAN] = "Mean Angle";
    ccrNames[CCR_ANGLE_MIN] = "Min Angle";
    ccrNames[CCR_ANGLE_MAX] = "Max Angle";
    ccrNames[CCR_TRIANGLE_AREA_MEAN] = "Mean triangle area";
    ccrNames[CCR_TRIANGLE_AREA_MIN] = "Min triangle area";
    ccrNames[CCR_TRIANGLE_AREA_MAX] = "Max triangle area";
    ccrNames[CCR_EDGE_DISTANCE_MEAN] = "Mean edge distance";
    ccrNames[CCR_EDGE_DISTANCE_MIN] = "Min edge distance";
    ccrNames[CCR_EDGE_DISTANCE_MAX] = "Max edge distance";
    ccrNames[CCR_TILE_SPAN_MEAN] = "Mean tile span";
    ccrNames[CCR_TILE_SPAN_MIN] = "Min tile span";
    ccrNames[CCR_TILE_SPAN_MAX] = "Max tile span";

    ccrNames[CCR_POSITION_MEAN] = "Position";
    ccrNames[CCR_POSITION_MIN] = "Min positions";
    ccrNames[CCR_POSITION_MAX] = "Max positions";
    ccrNames[CCR_VELOCITY_MEAN] = "Velocity";
    ccrNames[CCR_VELOCITY_MIN] = "Min velocity";
    ccrNames[CCR_VELOCITY_MAX] = "Max velocity";

    ccrNames[CCR_INERTIA] = "Inertia";
    ccrNames[CCR_TORQUE] = "Torque";
    ccrNames[CCR_ENERGY] = "Energy";
    ccrNames[CCR_FORCE] = "Force";

    ccrNames[CCR_TUMBLING_ANGLES] = "Tumbling angles";
    ccrNames[CCR_DIAMETERS] = "Diameters";
    ccrNames[CCR_SYMMETRY_DEVIATION] = "Symmetry deviation";
    ccrNames[CCR_TAYLOR_DEFORMATION_INDEX] = "Taylor deviation index";
    return ccrNames;
}

std::map<int, std::string> ccrNames(createMapCCR());

#include "cellReductionTypes.hh"

#endif



