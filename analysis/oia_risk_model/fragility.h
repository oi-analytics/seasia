#ifndef FRAGILITY_H
#define FRAGILITY_H

#include <cmath>
#include <vector>
#include <sstream>
#include <fstream>
#include <string>

#include "utils.h"
#include "exceptions.h"

namespace oia_risk_model{
  namespace fragility{

    // Structure for the storage of a fragility curve...
    struct FragilityCurve{
      bool                    serviceability = false; // Flag indicating if curve represents the serviceability limit state
      std::vector<double>     load;                   // Load vector, needs to be equally spaced
      std::vector<double>     pFail;                  // Vector of failure probabilities, one for each load in the load vector
      int                     numCG;                  // Number of condition grades covered by fragility curve (calculated)
      int                     numLoads;               // Number of loads specified for the curve (calculated)
      double                  minLoad;                // Minimum load (calculated)
      double                  maxLoad;                // Maximum load (calculated)
      double                  deltaLoad;              // Load delta (calculated)
      // Constructor - curve will be read from a nominated file, user to specify if this curve represents structural failure or not...
      FragilityCurve(const std::string fileName, const bool structuralFailue = true){
        // Open the incoming file...
        if(utils::exists(fileName)){
          Exception("The Fragility curve file-name does not exist.");
          return;
        }
        std::ifstream inFile;
        inFile.open(fileName);
        std::string line;

        // Loop over the complete file
        while(!inFile.eof()){
          std::getline(inFile, line);
          std::vector<std::string> words = utils::readLine(line);
          // Get size of the incoming data...
          int numFields = words.size();
          if(numFields > 0){
            // Recover the load...
            load.push_back(std::stod(words.at(0)));
            // Recover the pFail data...
            for(int i=1; i<numFields; i++){
              pFail.push_back(std::stod(words.at(i)));
            }
          }
        }

        // Recover the salient information about the fragility curve...
        numLoads = load.size();
        numCG = pFail.size() / numLoads;
        minLoad = load.at(0);
        maxLoad = load.at(load.size()-1);
        deltaLoad = load.at(1) - load.at(0);

        // Close the incoming file...
        inFile.close();

        // Take a copy of the serviceability flag...
        serviceability = !structuralFailue;
      }

      // Helper function to return the "pseudo-CG" associated with how exposed an asset is to wind load...
      int windCG(const double mean_wind_speed) const {
        // Lookup the CG from the hard-coded mean wind speed relationship...
        if(mean_wind_speed < 2)
          return 1;
        if(mean_wind_speed < 3.5)
          return 2;
        if(mean_wind_speed < 4.5)
          return 3;
        if(mean_wind_speed < 6.5)
          return 4;
        // else
        return 5;
      }

      // Helper function to return the "CG" associated with the road surface (there are only 2)...
      int roadCG(const std::string roadType) const {
        // Is the road paved?
        if( roadType.find("motorway") != std::string::npos &&
            roadType.find("primary") != std::string::npos &&
            roadType.find("secondary") != std::string::npos &&
            roadType.find("trunk") != std::string::npos ){
          return 1;
        }

        // All other road types get the "unpaved" CG...
        return 2;
      }

      // Helper function to return the serviceability CG of the road service...
      int roadServiceabilityCG(const std::string roadType) const {
        if( roadType.find("motorway") != std::string::npos ||
            roadType.find("trunk") != std::string::npos )
          return 1;
        if( roadType.find("primary") != std::string::npos)
          return 2;
        if( roadType.find("secondary") != std::string::npos ||
            roadType.find("tertiary") != std::string::npos)
          return 3;
        // All other assets get a CG of 4 (which is the first in the file)...
        return 4;
      }

      // Get the fragility associated with an asset at a given load (using linear interpolation)
      double probability(const int CG, const double l) const {
        // Firstly, is this load too small to trouble the scorers?
        if(l <= minLoad)
          return pFail.at(0);

        // Make sure the CG is in range (else return something that won't break the callers code)...
        if(CG < 1 ||  CG > numCG)
          return 0;

        // All good, so get the index of the load...
        int index = std::floor((l - minLoad) / deltaLoad);

        // Make sure the load isn't greater than the range of load data...
        if(l >= load.at(load.size()-1))
          return pFail.at((numLoads-1)*numCG + (CG -1));

        // Find the lower and upper bound of the incoming load...
        double lower = pFail.at(    index*numCG + (CG-1));
        double upper = pFail.at((index+1)*numCG + (CG-1));

        // Get the weight to the lower and upper values...
        double A = 1 - (l - load.at(index)) / (load.at(index+1) - load.at(index));

        // Return the calculated pFail value to the caller...
        return A*lower + (1 - A)*upper;
      }
    };
  } // fragility
} // oia_risk_model

#endif //FRAGILITY_H
