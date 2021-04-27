#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

// Import the MapInfo header file, which takes care of other imports
#include "oia_risk_model/mif.h"

// Alias the imported namespace, to make it a little easier to use...
namespace oia = oia_risk_model;

/*
 * This application is used to add exposure data to assets, for multiple sources. 
 * Suggested compilation script: g++ asset_exposure.cpp -std=c++17 -O3 -o asset_exposure 
 */
int main(int argc, char** argv){
  /////////////////////////////////////////////////////////
  // 0: Check that application has been called correctly...
  if(argc != 4)
    // oia_risk_model exceptions are fairly blunt, and used this way...
    oia::Exception("The hello_oia app needs to be called with three arguments:\n"
                   "   1. Comma delimited steering file giving raster file name and attribute-name to store data against\n"
                   "   2. Existing MIF file of linear assets (without extension)\n"
                   "   3. Output file-name for the modified MapInfo (without exension)\n\n"
                   "NOTE: This application should ONLY be used for rasters with a common origin, cellsize and dimension.\n");

  std::string rasterSteeringFile = std::string(argv[1]);
  std::string mifFile            = std::string(argv[2]);
  std::string outputFile         = std::string(argv[3]);

  // ...and that the nominated steering file exists...
  if(!oia::utils::exists(rasterSteeringFile))
    oia::Exception("The steering file of rasters does not exist");

  //...as well as the MIF / MID.
  oia::utils::mifExists(mifFile);


  ////////////////////////////////////////////////////////////////////////////////////////////////
  // 1: Read the steering file into a vector of key_value pairs (k=file_name, v=attribute_name)...
  std::vector<std::pair<std::string,std::string>> rasterFiles = oia::readRasterSteeringFile(rasterSteeringFile);


  ///////////////////////////////////////////////////////
  // 2: Read and prepare the assets for exposure calcs...
  oia::MIF assets(mifFile);

  // Divide the assets onto the hazards (but don't bother recording the fact we are dividing the assets)...
  assets.divideFeatures(rasterFiles.at(0).first, false);


  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  // 3: Calculate per-raster exposure (Note: this process needs to be run buffered i.e. out-of-memory)...
  std::vector<double>       buffer;       // A buffer of data into which to save the exposure calcs
  std::vector<std::string>  bufferFiles;  // A vector of attribute names, which are being used as file-names for buffered file creation

  // Loop over all the rasters...
  for(auto rasterFile : rasterFiles){
    // Wipe-out the old data in the buffer...
    buffer.resize(assets.features.size(),0);

    // Read the raster...
    oia::Ascii ascii(rasterFile.first);

    // Loop over all the features in the mif file...
    int featureIndex=0;
    for(auto f : assets.features){

      // Recover the cell index at the mid-point of the first feature...
      oia::geometry::Vec2<double> midPoint = f.mid_point();

      // Add the data to the buffer...
      buffer.at(featureIndex) = ascii.data_at_point(midPoint); // Add the data to the feature index...

      // Increment the count of features...
      featureIndex++;
    }

    // Write the buffered exposure data to disk (using the atrribute name)...
    oia::utils::writeBuffer(buffer, rasterFile.second);

    // ...and stick it on the tab.
    bufferFiles.push_back(rasterFile.second);
  }


  ////////////////////////////////////////////////////////////////
  // 4: Write the new MIF file with exposure attributes to disk...
  assets.writefromBuffer(outputFile, bufferFiles);

  return 0;
}