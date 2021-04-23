#ifndef COST_FUNCTION_H
#define COST_FUNCTION_H

#include <vector>
#include <string>
#include <fstream>

#include "utils.h"

namespace oia_risk_model{
  namespace fragility{

    // Structure to represent the explicitly specified reinstantement cost of an asset
    struct Cost{
      std::string    asset; // Asset name
      double      min_cost; // Minimum reinstatement cost
      double      max_cost; // Maximum reinstatement cost
      // Construct a cost...
      Cost(const std::string name, const double min, const double max) : asset(name), min_cost(min), max_cost(max){}
    };

    // Structure representing a cost-function
    struct CostFunction{
      std::vector<Cost> costs;        // Vector of cost objects
      double            default_min;  // Default minimum cost of asset for which no cost is known
      double            default_max;  // Default maximum value of asset for which no cost is known
      // Helper function to return the minimum cost associated with an assset
      double min(const std::string asset) const {
        // Loop over all the known costs...
        for(auto c : costs)
          // Are these the droids we are looking for?
          if(asset == c.asset)
            // Return the identified minimun cost...
            return c.min_cost;

        // Asset not recognised: Return the default cost
        return default_min;
      }
      // Helper function to return the maximum cost associated with an assset
      double max(const std::string asset) const {
        // Loop over all the known costs...
        for(auto c : costs)
          if(asset == c.asset)
            // Return the identified minimun cost...
            return c.max_cost;

        // Asset not recognised: Return the default cost
        return default_max;
      }
    };

    // Helper function to read a cost file from disk in a starndard format...
    CostFunction readCostFile(const std::string fileName){
      // Open the cost file...
      std::ifstream inFile;
      inFile.open(fileName);

      // Space for the incoming data...
      std::string line;

      // Somewhere to store the resulting costfunction...
      CostFunction cf;

      // Read the file, line by line...
      while(!inFile.eof()){
        // Get the next line...
        std::getline(inFile, line);
        // If it's valid....
        if(line.size() > 0){
          // Chunk it into words...
          std::vector<std::string> words = utils::readLine(line);
          // If it fits the desired pattern, parse it further...
          if(words.size() == 3){
            if(words.at(0) == "\"default\""){
              cf.default_min = std::stod(words.at(1));
              cf.default_max = std::stod(words.at(2));
            }else{
              Cost c(words.at(0), std::stod(words.at(1)), std::stod(words.at(2)));
              cf.costs.push_back(c);
            }
          }
        }
      }

      // Return the fully-populated cost function to the caller...
      return cf;
    }
  } // fragility
} // oia_risk_model

#endif //COST_FUNCTION_H
