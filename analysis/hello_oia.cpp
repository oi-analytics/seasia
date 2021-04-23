#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

// Import the MapInfo header file, which takes care of other imports
#include "oia_risk_model/mif.h"

// Alias the imported namespace, to make it a little easier to use...
namespace oia = oia_risk_model;


/* 
 * "Hello world" basic usage of oia_risk_model, which is a header-only library so only needs to be included (no linking).
 * Suggested compilation script: g++ hello_oia.cpp -std=c++17 -O3 -o hello_oia 
 */
int main(int argc, char** argv){
  ////////////////////////////////////////////////////////////
  // 0. Preliminaries
  if(argc != 4)
    // oia_risk_model exceptions are fairly blunt, and used this way...
    oia::Exception("The hello_oia app needs to be called with three arguments:\n"
                   "   1. Existing MapInfo file containing linear assets"
                   "   2. Existing ESRI Ascii raster of hazards"
                   "   3. Output file-name for the modified MapInfo (without exension)\n");

  // Extract the file-names for the test...
  std::string input_mif_file   = std::string(argv[1]);
  std::string ascii_file       = std::string(argv[2]);
  std::string output_mif_file  = std::string(argv[3]);


  ////////////////////////////////////////////////////////////
  // 1. Load linear assets
  oia::MIF assets(input_mif_file);

  // Access key features of the MIF file...
  std::cout << "On loading, MIF file has " << assets.features.size() << " features\n";
  std::cout << "(and each feature has " << assets.columns.size() << " attributes)\n\n";


  ////////////////////////////////////////////////////////////
  // 2. Load gridded hazard data
  oia::Ascii flood_depth(ascii_file);


  ////////////////////////////////////////////////////////////
  // 3. Divide linea assetes to match gridded data, add new attribute
  assets.divideFeatures(flood_depth);

  // Add a new numerical attribute to the assets file...
  assets.addAttribute("hello_oia", "Float");

  // Access some key features of the MIF file...
  std::cout << "After division, MIF file now has " << assets.features.size() << " features\n";
  std::cout << "(and each feature has " << assets.columns.size() << " attributes)\n\n";



  ///////////////////////////////////////////////////////////
  // 4. Calculate exposure for assets
  for(auto& f : assets.features){
    // Extract the mid_point from the feature...
    oia::geometry::Vec2<double> mid_point = f.mid_point();

    // Find the depth associated with this point...
    double local_depth = flood_depth.data_at_point(mid_point);

    // Stick it on the tab...
    f.attributes.push_back(std::to_string(local_depth));
  }



  ///////////////////////////////////////////////////////////
  // 5. Write new file to disk
  assets.write(output_mif_file);

  return 0;
}