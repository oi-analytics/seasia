#ifndef MIF_H
#define MIF_H

#include <iostream>
#include <iomanip>

#include "exceptions.h"
#include "utils.h"
#include "features.h"
#include "raster.h"
#include "fragility.h"
#include "cost_function.h"
#include "graph.h"

namespace oia_risk_model{
  // MapInfo data type, contains internal representatin / methods for polylines and regions
  struct MIF{
    std::string              _fileName;         // When reading the file "Just In time", we need to preserve the filename
    bool                     justInTime=false;  // Should the heavy-data (.mid) be read at once, or defered to later?
    bool                     region=false;      // Does the MIF file describe regions?
    std::vector<std::string> header;            // Verbatim representation of the header of the MIF file
    std::vector<Feature>     features;          // Vector of features in the file
    std::vector<bool>        dropFeature;       // Vector of bools indicating feature can safely be discarded before write-out
    std::vector<std::string> columns;           // String representation of the attribute names
    std::vector<bool>        dropColumn;        // Vector of bools indicating whether the attribute (column) should be dropped before writing
    // Helper function to read the header of the MIF file...
    bool readHeader(std::ifstream* infile){
        // Put some space aside to read the file...
        std::string line = "";

        // Read the file until we hit the word "Data" which signifies the start of the feature information...
        while(line.substr(0,4) != "Data"){
          std::getline(*infile, line);
          if(line.length() > 0){
            if(line.substr(0,7) == "Columns"){
              // We want to read the column details into their own vector...
              std::vector<std::string> words = utils::readLine(line, ' ');
              int numCols = std::stoi(words.at(1));

              std::string colLine;
              for(int i=0; i<numCols; i++){
                std::getline(*infile, colLine);
                std::cout << colLine << "\n";
                columns.push_back(colLine);
                dropColumn.push_back(false);
              }
            }

            // Stick the header line on the header vector APART from the keywords "Data" and "Columns"
            // which are derived from the stored data...
            if(line.substr(0,4) != "Data" && line.substr(0,7) != "Columns")
              header.push_back(line);
          }
        }

        // And finally, read one extra line (which should be blank in a vlid MIF file)...
        std::getline(*infile, line);

        // Indicate the caller can now safely read the data from the rest of the file...
        return line.length() <= 1;
    }

    // Function to read MIF file...
    MIF(const std::string file_name, bool const justInTime=false) : _fileName(file_name), justInTime(justInTime){
      // Test that the incoming file actually exists...
      utils::mifExists(file_name);

      // If it does, take a copy of the file-name...
      std::string f_n = file_name;

      // And a flag to indicate whether the extension is upper-case or not...
      bool isUpperCase = false;

      // If the file already has an extension, strip it...
      if(f_n.find(".mif") != std::string::npos ||
         f_n.find(".MIF") != std::string::npos){
        // Trim the file to remove the extension...
        f_n = f_n.substr(0,f_n.length()-4);
        // Is the extension upper-case?
        if(f_n.find(".MIF") != std::string::npos)
          isUpperCase = true;
      }

      // Open the .mif file...
      std::ifstream mif_file;
      if(!isUpperCase)
        mif_file.open(f_n + ".mif");
      else
        mif_file.open(f_n + ".MIF");

      // Open the .mid file...
      std::ifstream mid_file;
      if(!isUpperCase)
        mid_file.open(f_n + ".mid");
      else
        mid_file.open(f_n + ".MID");

      // Create some strings to read the file into...
      std::string mif_line[2];
      std::string pen_line;
      std::string point_line;
      std::string mid_line;

      // Get the header out of the way...
      bool allGood = readHeader(&mif_file);
      if(!allGood)return;

      // And then loop over the body of the file, looking for data...
      while (!mif_file.eof()){
        // Somewhere to store the number of regions in the file...
        int numFeatures = -1;

        // The first two line in the entity description will be the line-type and number of points...
        std::getline(mif_file, mif_line[0]);

        if(mif_line[0].size() > 0){
          // Create somewhere to build the representation of the mif geometric feature...
          std::vector<Feature> fs;

          // Extract the data from the line...
          std::vector<std::string> mif_words = utils::readLine(mif_line[0], ' ');

          int numPoints = 0;

          // ogr2ogr adds numPoints data to the Line or Pline string...
          if(mif_words.size() == 2){
            // ogr2ogr Pline element...
            if(mif_words.at(0) == "Region"){
              // We are dealing with regions...
              region = true;
              numFeatures = std::stoi(mif_words.at(1));

              // The number of points for the first feature comes next...
              std::getline(mif_file, mif_line[1]);
              numPoints = std::stoi(mif_line[1]);
            }else{
              numPoints = std::stoi(mif_words.at(1));
              numFeatures = 1;
            }
          }else if(mif_words.size() == 3){
            // QGIS Pline element - size on next line...
            std::getline(mif_file, mif_line[1]);
            numPoints = std::stoi(mif_line[1]);
            numFeatures = 1;
          } else if(mif_words.size() == 5){
            // ogr2ogr Line element - data read-in on the same line...
            geometry::Vec2<double> pt1(std::stod(mif_words.at(1)), std::stod(mif_words.at(2)));
            geometry::Vec2<double> pt2(std::stod(mif_words.at(3)), std::stod(mif_words.at(4)));

            // Create a geometry...
            std::vector<geometry::Vec2<double>> geom;

            // Give it some space...
            geom.push_back(pt1);
            geom.push_back(pt2);

            // Create a new feature (Feature or Region) from the geom...
            if(region){
              Poly2 f(geom);
              fs.push_back(f);
            }else{
              Feature f;
              for(auto g : geom){
                  f.geometry.push_back(g);
              }
              fs.push_back(f);
            }
          }else{
            // No idea what this is: Stop and complain bitterly...
            Exception("Unknown feature-type");
            return;
          }

          // Loop over each of the features / regions...
          for(int j=0; j<numFeatures; j++){
            // Create a new geometry vector...
            std::vector<geometry::Vec2<double>> geom;

            // For the second and later features / regions we need to read the number of points...
            if(j > 0 && region){
                std::getline(mif_file, mif_line[1]);
                numPoints = std::stoi(mif_line[1]);
            }

            // Loop over each point in the file...
            for(int i=0; i<numPoints; i++){
              // First, retrieve the point string...
              std::getline(mif_file, point_line);

              // Split the string into words...
              std::vector<std::string> words = utils::readLine(point_line, ' ');

              // Parse the point into its Lat, Lon pair and save in the feature geometery...
              if(words.size() == 2){
                  geometry::Vec2<double> pt(std::stod(words.at(0)),std::stod(words.at(1)));
                  geom.push_back(pt);
              }
            }

            // Create a new feature...
            if(region){
                Poly2 f(geom);
              fs.push_back(f);
            }else{
              Feature f;
              for(auto g : geom){
                  f.geometry.push_back(g);
              }
              fs.push_back(f);
            }
          }

          // The features in the mif have a pen line that we don't need to save in the internal repr...
          std::getline(mif_file, pen_line);

          // The features in the region files have a brush as well as a pen...
          if(region)
            std::getline(mif_file, pen_line);

          // If we aren't defering the read-in of the heavy-data, read the mid file (which will be a single line per feature)...
          if(!justInTime)
            std::getline(mid_file, mid_line);

          // And finally, add the features to the internal vector of features...
          for(auto f : fs){
            // Split the attributes into words, store in the feature...
            if(!justInTime)
              f.attributes = utils::readLine(mid_line, ',');
            features.push_back(f);
            // We will assume that this feature matters, for now...
            dropFeature.push_back(false);
          }
        }
      }
    }

    // Helper function to add a new attribute to the MIF file...
    void addAttribute(const std::string name, const std::string type = "string", const int fieldSize=254){
      dropColumn.push_back(false);
      if(utils::lower_case(type) == "string"){
        columns.push_back("  " + name + " Char(" + std::to_string(fieldSize) + ")");
      }else if(utils::lower_case(type) == "float"){
        columns.push_back("  " + name + " Float");
      }
    }

    // Helper function to drop a column from the MIF file before writing...
    void removeAttribute(const int iCol){
      dropColumn.at(iCol) = true;
    }

    // Helper function to drop a column from the MIF file before writing...
    void removeAttribute(const std::string col){
      for(std::size_t i=0; i<columns.size(); i++){
        if(columns.at(i) == col){
            dropColumn.at(i) = true;
        }
      }
    }

    void removeAllAtributesExcept(const std::vector<std::string> cols){
      for(std::size_t i=0; i<columns.size(); i++){
        dropColumn.at(i) = true;
        for(auto c : cols){
          if(columns.at(i).find(c) != std::string::npos){
            dropColumn.at(i) = false;
          }
        }
      }
    }

    // Helper function to return the index of a column (or -9 if it doesn't exist)...
    int columnIndex(const std::string c){
        int i=0;
        for(auto attr : columns){
          if(attr.find(c) != std::string::npos){
            return i;
          }
          i++;
        }
        return -9;
    }

    // Helper function to clean the lines in a MIF file by dividing on grid / graticule lines (file on disk)...
    void divideFeatures(const std::string asciiFile, const bool record_division=true){
      if(!utils::exists(asciiFile))
        Exception("Ascii filedoes not seem to exist (" + asciiFile +")");

      // Read the incoming raster...
      Ascii ascii(asciiFile);

      // Call the overloaded method...
      divideFeatures(ascii, record_division);
    }

    // Helper function to clean the lines in a MIF file by dividing on grid / graticule lines (file in memory)...
    void divideFeatures(const Ascii ascii, const bool record_division=false){
      // A vector of cleaned features (features with additional lines, broken by grid / graticule)...
      std::vector<Feature> cleanedFeatures;
      std::vector<bool>    dropCleanFeature;

      // Append a new attribute to indicate the line has been divided...
      if(record_division)
        addAttribute("is_divided", "string", 7);


#ifdef CHATTY
      // Test the number of features...
      std::cout << "Number of features BEFORE cleaning = " << features.size() << "\n";
#endif // CHATTY

      // Loop over all the features in the mif file...
      for(auto f : features){
        // Create a new, clean feature...
        Feature clean;

        // Give the clean feature the same attributes as the feature we are testing...
        clean.attributes = f.attributes;

        // And a flag to indicate that a feature has been divided...
        bool divided = false;
        // Loop over each point in the original feature geometry...
        for(std::size_t i=0; i<f.geometry.size()-1; i++){
          // ...and construct a line to the next point.
          geometry::Line2<double> line(f.geometry.at(i), f.geometry.at(i+1));

          // If the line starts and ends in different cells, it needs to be cleaned...
          if(ascii.cellIndex(line.start) != ascii.cellIndex(line.end)){
            // Recover the points of intersection of the line with the grid / graticule lines...
            std::vector<geometry::Vec2<double>> intersections = ascii.findIntersections(line);

            // Add the start of the line to the cleaned feature...
            clean.geometry.push_back(line.start);

            // Loop over each intersection, and add a new feature for each...
            for(std::size_t j=1; j<intersections.size(); j++){
              // Add the crossing point to the cleaned features geometry...
              clean.geometry.push_back(intersections.at(j));

              // Set the attributes to indicate the feature has been cleaned...
              if(record_division)
                clean.attributes.push_back("\"true\"");

              // Stick the cleaned feature on the tab...
              cleanedFeatures.push_back(clean);
              dropCleanFeature.push_back(false);

              // Reset the clean feature ready to process the rest of the geometry...
              clean.attributes.clear();
              clean.attributes = f.attributes;
              clean.geometry.clear();

              // And add the start point of the line...
              clean.geometry.push_back(intersections.at(j));
            }

            // Mark the line as divided...
            divided = true;
          }else{
            // ...if the line is already clean, append directly to the cleaned features geometry.
          clean.geometry.push_back(f.geometry.at(i));
          }
        }

        // Finally, we need to append the last point in the feature geometry to the newly cleaned geometry.
        clean.geometry.push_back(f.geometry.at(f.geometry.size()-1));

        // Grab the attributes of the original feature...
        clean.attributes = f.attributes;

        // And a bonus attrbute indicating whether the feature was divided or not.
        if(record_division)
          clean.attributes.push_back( divided ? "\"true\"" : "\"false\"");

        // ...and stick it all on the tab.
        cleanedFeatures.push_back(clean);
        dropCleanFeature.push_back(false);
      }

      // Finally, delete the original features and drop indicators...
      features.clear();
      dropFeature.clear();

      // ...and add the new features / indicators in their place.
      features = cleanedFeatures;
      dropFeature = dropCleanFeature;

#ifdef CHATTY
      std::cout << "Number of features AFTER cleaning = " << features.size() << "\n";
#endif // CHATTY
    }

    // Helper function to densify the lines in MIF Regions files by dividing wherever they cross grid / graticule lines...
    void densifyRegions(const std::string asciiFile){
      // A vector of densified features (features with additional lines, broken by grid / graticule)...
      std::vector<Feature> densifiedFeatures;
      std::vector<bool>    dropDensifiedFeature;

      // Read the incoming raster...
      Ascii ascii(asciiFile);

#ifdef CHATTY
      std::cout << "Number of features BEFORE cleaning = " << features.size() << "\n";
#endif // CHATTY

      // Loop over all the features in the mif file...
      for(auto f : features){
        // We will replace the old feature with a new one, with more points in the geometry...
        Feature divided;

        // Give the clean feature the same attributes as old one it will replace...
        divided.attributes = f.attributes;

        // Loop over each point in the original feature geometry...
        for(std::size_t i=0; i<f.geometry.size()-1; i++){
          // ...and construct a line to the next point.
          geometry::Line2<double> line(f.geometry.at(i), f.geometry.at(i+1));

          // If the line starts and ends in different cells, it needs to be cleaned...
          if(ascii.cellIndex(line.start) != ascii.cellIndex(line.end)){
            // Recover the points of intersection of the line with the grid / graticule lines...
            std::vector<geometry::Vec2<double>> intersections = ascii.findIntersections(line);

            // Add the start of the line to the cleaned feature...
            divided.geometry.push_back(line.start);

            // Loop over each intersection, and add a new feature for each...
            for(std::size_t j=1; j<intersections.size(); j++){
              // Add the crossing point to the cleaned features geometry...
              divided.geometry.push_back(intersections.at(j));
            }
          }else{
            // ...if the line is already clean, append directly to the cleaned features geometry.
          divided.geometry.push_back(f.geometry.at(i));
          }
        }

        // Finally, we need to append the last point in the feature geometry to the newly cleaned geometry.
        divided.geometry.push_back(f.geometry.at(f.geometry.size()-1));

        // Grab the attributes of the original feature...
        divided.attributes = f.attributes;

        // ...and stick it all on the tab.
        densifiedFeatures.push_back(divided);
        dropDensifiedFeature.push_back(false);
      }

      // Finally, delete the original features...
      features.clear();
      dropFeature.clear();

      // ...and add the new features in their place.
      features    = densifiedFeatures;
      dropFeature = dropDensifiedFeature;

#ifdef CHATTY
      std::cout << "Number of features AFTER cleaning = " << features.size() << "\n";
#endif // CHATTY
    }

    // Helper function to write the MID file to disk...
    void writeMID(const std::string fileName, const std::string merge="NONE") const {
      // Open the output file...
      std::ofstream outFile;
      outFile.open(fileName + ".mid");

      // Has processing the MID file been defered?
      std::vector<std::string> midLines;
      if(justInTime){
        std::ifstream           inFile;
        inFile.open(_fileName + ".mid");

        std::string line;

        while(!inFile.eof()){
          std::getline(inFile, line);
          if(line.size() > 0){
            midLines.push_back(line);
          }
        }

#ifdef CHATTY
        std::cout << "Size of mid file vector = " << midLines.size() << "\n";
#endif

        inFile.close();
      }

      // Loop over each feature...
      std::size_t iF=0;
      while(iF < features.size()){
        // Is this a feature that is being dropped?
        if(dropFeature.at(iF))
          continue;

        // Copy the first features attributes...
        std::vector<std::string> merged_attributes;

        // Get the attributes for the feature...
        if(justInTime){
          merged_attributes = utils::readLine(midLines.at(iF));
        }else{
          merged_attributes = features.at(iF).attributes;
        }

        std::size_t iFF = iF + 1;
        double numMerged = 1;
        while(iFF < features.size() && dropFeature.at(iFF)){
          std::vector<std::string> next_attributes;

          // Are we doing this JIT or from memory?
          if(justInTime){
            next_attributes = utils::readLine(midLines.at(iFF));
          }else{
            next_attributes = features.at(iFF).attributes;
          }

          for(std::size_t iA=0; iA<merged_attributes.size(); iA++){
            // For floats, we want to operate on the attribute data...
            if(columns.at(iA).find("Float") != std::string::npos){
              // Does this column contain a probability?
              bool prob = (columns.at(iA).find("annualProbability") != std::string::npos);

              // Recover the existing value and the equivalent from the merged feature...
              double existing = std::stod(merged_attributes.at(iA));
              double incoming = std::stod(next_attributes.at(iA));

              // And then do stuff...
              if(merge == "SUM" || merge == "MEAN"){
                // We can't just sum up probailities...
                if(!prob){
                  merged_attributes.at(iA) = std::to_string(existing + incoming);
                }else{
                  double p = existing + ((1 - existing) * incoming);
                  merged_attributes.at(iA) = std::to_string(p);
                }
              }else if(merge == "MIN"){
                merged_attributes.at(iA) = std::to_string(std::min(existing, incoming));
              }else if(merge == "MAX"){
                merged_attributes.at(iA) = std::to_string(std::max(existing, incoming));
              }
            }
          }
          // Increase the number of merged features...
          numMerged++;
          // Increate the counter...
          iFF++;
        }

        // If the attributes are being averaged, then do this now...
        if(merge == "MEAN"){
          for(auto& a : merged_attributes){
              double existing = std::stod(a);
              a = std::to_string(existing / numMerged);
          }
        }

        // FINALLY write the data to disk...
        for(std::size_t iA=0; iA<merged_attributes.size()-1; iA++){
          if(!dropColumn.at(iA)){
            auto a = merged_attributes.at(iA);
            outFile  << a << ",";
          }
        }

        if(!dropColumn.at(merged_attributes.size()-1))
          outFile << merged_attributes.at(merged_attributes.size()-1) << "\n";
        else
          outFile << "\n";

        iF = iFF;
      }

      outFile.close();
    }

    // Helper function to write the MIF file to disk...
    void write(const std::string fileName, const bool justMif=false) const {
      std::ofstream outFile;
      outFile.open(fileName + ".mif");

      // First, write the header...
      for(auto line : header){
        outFile << line << "\n";
      }

      int colsDropped = 0;
      for(auto c : dropColumn){
        colsDropped += c;
      }
      outFile << "Columns " << columns.size() - colsDropped << "\n";
      int iLine=0;
      for(auto col : columns){
        if(!dropColumn.at(iLine))
          outFile << col << "\n";
        iLine++;
      }

      outFile << "Data\n";
      outFile << "\n";

      // Loop over each of the features...
      std::size_t iF = 0;
      while(iF < features.size()){
        // Grab the feature...
        auto f = features.at(iF);

        // If this is not a dropped feature, Write the region or pline descriptor...
        if(!dropFeature.at(iF)){
          if(region)
            outFile << "Region 1\n";
          else
            outFile << "Pline ";

          // We need to calculate the number of points in the geometry, which needs to be calculated...
          int numPoints = f.geometry.size();

          // Get the index of the next feature...
          std::size_t iFF = iF + 1;
          while(iFF < features.size() && dropFeature.at(iFF)){
            numPoints += features.at(iFF).geometry.size() - (features.at(iFF-1).geometry.at(features.at(iFF-1).geometry.size()-1) == features.at(iFF).geometry.at(0) && dropFeature.at(iFF));
            iFF++;
          }

          outFile << numPoints << "\n";
        }

        // Write the first point in the geometry if needed...
        if(!dropFeature.at(iF) || !(f.geometry.at(0) == features.at(iF-1).geometry.at(features.at(iF-1).geometry.size()-1) && dropFeature.at(iF)))
          outFile << std::setprecision(13) << f.geometry.at(0).x << " " << f.geometry.at(0).y << "\n";

        // Write the rest of the features...
        for(std::size_t iP=1; iP<f.geometry.size(); iP++)
          outFile << std::setprecision(13) << f.geometry.at(iP).x << " " << f.geometry.at(iP).y << "\n";

        // If this is NOT the last feature, we want to test if the next feature is being dropped...
        if(iF < features.size()-1){
          // Are we dropping the next feature?
          if(!dropFeature.at(iF+1)){
            // If not, we can close out the repr with the pen and brush (if needed)...
            outFile << "    Pen (1,2,0)\n";
            if(region)
                outFile << "    Brush (1,0,16777215)\n";
          }
        }

        // Mosy on over to the first feature that is different to this one...
        iF++;
      }

      outFile.close();

      // Write tne MID file...
      if(!justMif)
        writeMID(fileName);
    }

    // Write a MIf file to disk from buffered files, rather than feature attributes...
    void writefromBuffer(const std::string fileName, const std::vector<std::string> buffers){
      // Open the mid file for output...
      std::ofstream midFile;
      midFile.open(fileName + ".mid");

      // Create some ifstreams to use to read the files...
      std::vector<std::ifstream*> files;

      // Open each of the buffers in turn...
      for(auto buffer : buffers){
        std::ifstream* f = new std::ifstream(buffer, std::ios::in | std::ios::binary);
        files.push_back(f);
      }

      // Create some storage for the buffered data...
      std::vector<double> data;
      data.resize(buffers.size(), 0);

      // Loop over each feature...
      for(auto f : features){
        // Read the data from file...
        int iFile=0;
        for(auto f : files){
          // Read the next record...
          f->read((char*)&data.at(iFile), sizeof(double));
          // Increment the file...
          iFile++;
        }

        // First, add the attributes stored "properly"...
        for(auto a : f.attributes)
          midFile  << a << ",";

        // Followed by the data just read from disk...
        for(std::size_t i=0; i<data.size()-1; i++)
          midFile << data.at(i) << ",";

        // Finally, write the last vale to disk, and add a newline...
        midFile << data.at(data.size()-1) << "\n";

      }

      // And close out the midFile...
      midFile.close();

      // Close each of the buffers...
      for(auto& f : files)
        f->close();

      // ...and delete!
      for(auto b : buffers)
        remove(b.c_str());

      // Finally, we need to add the buffers to the MIF file itself...
      for(auto buffer : buffers)
        addAttribute(buffer, "Float");

      // And write it out (without the MID)...
      write(fileName, true);
    }
  };

  // Helper function to merge two MID files (assuming the same MIF files for each - not tested)...
  void mergeMID(const std::string mid1, const std::string mid2, const std::string fileName){
    // Open the output file...
    std::ofstream outFile;
    outFile.open(fileName);

    // Open the input files...
    std::ifstream inFile1;
    inFile1.open(mid1);

    std::ifstream inFile2;
    inFile2.open(mid2);

    // Create somewhere to read the data...
    std::string line1;
    std::string line2;

    while(!inFile1.eof()){
      std::getline(inFile1,line1);
      std::getline(inFile2,line2);

      if(line1.size() > 0){
        // Parse the words...
        std::vector<std::string> words1 = utils::readLine(line1);
        std::vector<std::string> words2 = utils::readLine(line2);

        // Write the data from the first mid...
        for(auto word : words1){
          outFile  << word << ",";
        }

        // And the non-repeated data from the second mid...
        for(std::size_t i=3; i<words2.size()-1; i++){
          outFile  << words2.at(i) << ",";
        }

        // And then add a newline...
        outFile << words2.at(words2.size()-1) << "\n";
      }
    }

    // Close the new mid file...
    outFile.close();
  }

  // Helper function to "rejoin" features in a feature that has previously been divided...
  void rejoin(const std::string mifFile, const std::string outputFile, const int id_col, const std::string method="SUM") {
    // Method matches features on id_col and supports the following merging methods,
    // which operate on Float attributes only:
    // "SUM", "MIN", "MAX" and "MEAN"

    // Make sure both parts of the file to merge actually exist...
    if(!utils::exists(mifFile + ".mif") || !utils::exists(mifFile + ".mid"))
      Exception("File does not exist: " + mifFile + ".mif");

    // Read the file...
    MIF mif(mifFile);

    // Are we doing this JIT, or from memory?
    if(mif.justInTime){
      // Open and read the MIF to find the ids of the features...
      std::ifstream inFile;
      inFile.open(mif._fileName + ".mid");

      std::string line;

      std::vector<std::string> id;

      while(!inFile.eof()){
        std::getline(inFile, line);
        std::vector<std::string> words = utils::readLine(line, ',');
        id.push_back(words.at(id_col));
      }

      inFile.close();

      // Loop over all the features, looking for neighbours that have been split...
      for(std::size_t i=1; i<mif.features.size(); i++)
        if(id.at(i) == id.at(i-1))
          mif.dropFeature.at(i) = true;

    }else{
      // We have the data in memory, so can find the features that have been divided simply...
      for(std::size_t i=1; i<mif.features.size(); i++)
        if(mif.features.at(i).attributes.at(id_col) == mif.features.at(i-1).attributes.at(id_col))
          mif.dropFeature.at(i) = true;
    }

    // By this point, we have identidied the features to merge back together again - we can write the
    // file, being clear what we want to do to the merged attributes features.
    mif.write(outputFile, true);
    mif.writeMID(outputFile, method);

    return;
  }

  // Helper function to "rejoin" features in a feature that has previously been divided...
  void rejoin(MIF mif, const std::string outputFile, const std::size_t id_col, const std::string method="SUM") {
    // Method matches features on id_col and supports the following merging methods,
    // which operate on Float attributes only:
    // "SUM", "MIN", "MAX" and "MEAN"

    // Are we doing this JIT, or from memory?
    if(mif.justInTime){
      // Open and read the MIF to find the ids of the features...
      std::ifstream inFile;
      inFile.open(mif._fileName + ".mid");

      std::vector<std::string> id;

      while(!inFile.eof()){
        std::string line;
        std::getline(inFile, line);

        std::vector<std::string> words = utils::readLine(line, ',');

        if(words.size() > id_col)
          id.push_back(words.at(id_col));
      }

      inFile.close();

      // Loop over all the features, looking for neighbours that have been split...
      for(std::size_t i=1; i<mif.features.size(); i++)
        if(id.at(i) == id.at(i-1))
          mif.dropFeature.at(i) = true;
    }else{
      // We have the data in memory, so can find the features that have been divided simply...
      for(std::size_t i=1; i<mif.features.size(); i++)
        if(mif.features.at(i).attributes.at(id_col) == mif.features.at(i-1).attributes.at(id_col))
          mif.dropFeature.at(i) = true;
    }

    // By this point, we have identidied the features to merge back together again - we can write the
    // file, being clear what we want to do to the merged attributes features.
    mif.write(outputFile, true);
    mif.writeMID(outputFile, method);

    return;
  }

  // Helper function to addfragility to a MIF file (by default, throwing out any assets with 0 risk)...
  void addRoadFragility(MIF mif,
                        const fragility::FragilityCurve f,
                        const fragility::CostFunction cf,
                        const std::string outFile,
                        const bool removeNoRiskAssets=true){
    // First, we want to know which of our input columns are numeric, and need pFail calculated...
    std::vector<bool> isRP;

    // And we want to keep track of which ones belong together...
    std::vector<std::pair<std::string,int>> scenarios;

    // For the roads, there is a highway index but not for the rails or the electricity grid...
    int highway_index    = -9;
    int length_index     = -9;
    int mean_speed_index = -9;
    int tree_cover_index = -9;

    // First go around, lets find the indices of interest...
    for(std::size_t i=0; i<mif.columns.size(); i++){
      // We also need to keep track of the highway tag on the roads - this is not part of the above calcs....
      if(mif.columns.at(i).find("highway") != std::string::npos){
        highway_index = i;
      }

      // For risk calcs, we need to keep track of the length of the asset....
      if(mif.columns.at(i).find("feature_length_km") != std::string::npos){
        length_index = i;
      }

      // For wind-risk, we need the tree-cover percentage...
      if(mif.columns.at(i).find("tree_cover_percent") != std::string::npos){
        tree_cover_index = i;
      }

      // ...and the mean 10m windspeed.
      if(mif.columns.at(i).find("mean_wind_speed_10m") != std::string::npos){
        mean_speed_index = i;
      }
    }

    // Is the incoming file meant to provide wind risk?
    bool windRisk = mean_speed_index > 0 && tree_cover_index > 0;

#ifdef CHATTY
    if(windRisk)
      std::cout << "Handling wind risk...\n";
#endif // CHATTY

    // We want to know how many pFail columns we are adding...
    int numRPCols = 0;

    //...second go around, lets do the actual maths...
    for(std::size_t i=0; i<mif.columns.size(); i++){
      if(mif.columns.at(i).find("RP") != std::string::npos){
        isRP.push_back(true);
        numRPCols++;

        // Now we can try to identify the scenario and rp of the load...
        std::vector<std::string> words = utils::readLine(mif.columns.at(i), '_');

        // Pull out the Scenario and Year (the first and second words)...
        std::string scenario;

        if(windRisk){
          scenario = words.at(0);
        }else{
          scenario = words.at(0) + "_" + words.at(1);
        }

        // And the RP (third word, remove "RP" from the start)...
        int rp = std::stoi(words.at(words.size()-1).substr(2,words.at(words.size()-1).size()-2));

        // Stick it all on the tab...
        scenarios.push_back(std::pair<std::string,int>(scenario,rp));
      }else{
        isRP.push_back(false);
        scenarios.push_back(std::pair<std::string,int>("None",-9));
      }
    }

    ///////////////////////////////
    // Find the unique scenarios...
    std::vector<std::string> uniqueScenarios;
    for(auto scenario : scenarios){
      // Skip things that aren't scenarios...
      if(scenario.first == "None")
        continue;

      // Find all the unique scenarios...
      bool notUnique = false;
      for(auto unique : uniqueScenarios){
        if( unique == scenario.first){
          notUnique = true;
        }
      }

      // Is this new?
      if(!notUnique){
        uniqueScenarios.push_back(scenario.first);
      }
    }

#ifdef CHATTY
    std::cout << "Number of unique scenarios = " << uniqueScenarios.size() << ":\n";

    for(auto u : uniqueScenarios)
      std::cout << "unique scenario = " << u << "\n";
#endif // CHATTY

    // Get out of Dodge?
    if(!windRisk){
      if(highway_index < 0 || length_index < 0){
        Exception("No highway index");
        return;
      }
    }else{
      if(mean_speed_index < 0 || tree_cover_index < 0 || length_index < 0){
        Exception("No mean wind-speed index");
        return;
      }
    }

    // There are seriously large number of assets at little / no risk (flooding, at least)...
    std::vector<bool> removeFeature;

    // We only really care about the mid file - let's go get it!
    std::ifstream mid_file;
    mid_file.open(mif._fileName + ".mid");

    // And we are generating a new mid file...
    std::ofstream new_mid;
    new_mid.open(outFile + ".mid");

    // Somewhere to read the data...
    std::string line;

    int assetsToRemove=0;
    int assetsAtRisk=0;
    while(!mid_file.eof()){
      std::getline(mid_file, line);
      if(line.size() > 0){
        // Get the next line...
        std::vector<std::string> mid_words = utils::readLine(line);

        // Pull out the CG of the road (either wind or flood, structural or serviceability)...
        int CG;
        if(windRisk){
          CG = f.windCG(std::stod(mid_words.at(mean_speed_index)));
        }else{
          if( f.serviceability )
            CG = f.roadServiceabilityCG(mid_words.at(highway_index));
          else
            CG = f.roadCG(mid_words.at(highway_index));
        }

        // Get the length of the asset (km)...
        double length = std::stod(mid_words.at(length_index));

        // And the min and max costs for this type of asset...
        double minCost;
        double maxCost;

        // Get the highway cost, guarding on the lack of highway index (i.e. electricity or rail)...
        if(highway_index > 0){
          minCost = cf.min(mid_words.at(highway_index));
          maxCost = cf.max(mid_words.at(highway_index));
        }else{
          minCost = cf.min(mid_words.at(0));
          maxCost = cf.max(mid_words.at(0));
        }

        // Check to see if the asset is at ANY risk at all...
        bool noRisk = true;
        for(std::size_t i=0; i<isRP.size(); i++){
          if(isRP.at(i)){
            if(std::stod(mid_words.at(i)) > 0){
              noRisk = false;
              break;
            }
          }
        }

        // Make sure we respect the lack of risk in the summary file...
        if(noRisk && removeNoRiskAssets){
          removeFeature.push_back(true);
          assetsToRemove++;
        }else{
          removeFeature.push_back(false);
          assetsAtRisk++;
        }

        if(!(noRisk && removeNoRiskAssets)){
          // Loop over the words in the file...
          for(std::size_t i=0; i<mid_words.size(); i++){
            // Just pass the incoming data into the new file...
            new_mid << mid_words.at(i);

            // IFF this is a numeric value, calculate the pFail and add it to the file as well...
            if(isRP.at(i)){
              double pFail = f.probability(CG, std::stod(mid_words.at(i)));

              // Wind-risk needs some different data...
              if(windRisk)
                pFail = pFail * (std::stod(mid_words.at(tree_cover_index)) / 40.0);

              // Add the probability of failure to the file...
              new_mid << "," << pFail;
              // ...and the expected length damaged...
              new_mid << "," << length * pFail;
              // ...and the minimum event damage...
              new_mid << "," << length * pFail * minCost * 1000000;
              // ...and the maximum event damage...
              new_mid << "," << length * pFail * maxCost * 1000000;
            }

            // Then handle either the next delimiter, or the new line character...
            new_mid << ",";
          }

          // We now need to loop over each scenario and calculate the Annual probability of failure...
          for(std::size_t uIndex=0; uIndex<uniqueScenarios.size(); uIndex++){
            std::vector<int>     returnPeriods;
            std::vector<double>  pFail;
            for(std::size_t i=0; i<mid_words.size(); i++){
              if(scenarios.at(i).first == uniqueScenarios.at(uIndex)){
                returnPeriods.push_back(scenarios.at(i).second);
                pFail.push_back(f.probability(CG, std::stod(mid_words.at(i))));
              }
            }

            // Create a graph from the RP and probability of failure...
            fragility::Graph annualProb(returnPeriods, pFail);

            // Get the areas under the curve...
            double area = annualProb.area();

            // Add the annualProb to disk...
            new_mid << area;

            // And the event length damage and min / max costs of damage...
            new_mid << "," << area * length;
            new_mid << "," << area * length * minCost * 1000000;
            new_mid << "," << area * length * maxCost * 1000000;

            // And an appropriate delimiter...
            if(uIndex < uniqueScenarios.size()-1){
              new_mid << ",";
            }else{
              new_mid << "\n";
            }
          }
        }
      }
    }

#ifdef CHATTY
    std::cout << "Size of no risk vector = " <<  removeFeature.size() << "\n";
    std::cout << "Assets to remove = " << assetsToRemove << "\n";
    std::cout << "Assets at risk   = " << assetsAtRisk << "\n";
    std::cout << "% discarded      = " << double(assetsToRemove) / double(assetsToRemove + assetsAtRisk) << "\n\n";
#endif // CHATTY

    // Close out the files...
    mid_file.close();
    new_mid.close();

    // We now need to create a new mif file, with the extra header data...
    std::ifstream mif_file;
    mif_file.open(mif._fileName + ".mif");

    std::ofstream new_mif;
    new_mif.open(outFile + ".mif");

    // The header remains the same...
    for(std::size_t i=0; i<mif.header.size(); i++){
      std::getline(mif_file, line);
      new_mif << line << "\n";
    }

    // Get the count of columns (but don't use)...
    std::getline(mif_file, line);

    // The count of columns changes...
    new_mif << "Columns " << mif.columns.size() + 4*numRPCols + 4*uniqueScenarios.size() << "\n";

    // Loop over each column in the file...
    for(std::size_t i=0; i<mif.columns.size(); i++){
      std::getline(mif_file, line);
      new_mif << line << "\n";
      if(isRP.at(i)){
        std::vector<std::string> words = utils::readLine(line, ' ');
        // NOTE: Using short-version of pFail flag...
        new_mif << "  pFail_" + words.at(0) + " Float\n";
        new_mif << "  eventDamage_" + words.at(0) + " Float\n";
        new_mif << "  minEventCost_" + words.at(0) + " Float\n";
        new_mif << "  maxEventCost_" + words.at(0) + " Float\n";
      }
    }

    // Then add the annula probabilities...
    for(std::size_t i=0; i<uniqueScenarios.size(); i++){
      new_mif << "  annualProbability_" + uniqueScenarios.at(i) + " Float\n";
      new_mif << "  EAL_" + uniqueScenarios.at(i) + " Float\n";
      new_mif << "  minEAD_" + uniqueScenarios.at(i) + " Float\n";
      new_mif << "  maxEAD_" + uniqueScenarios.at(i) + " Float\n";
    }

    // There are two lines in the file "Data" and "\n" that need to be parsed to reach the assets...
    std::getline(mif_file, line); new_mif << line << "\n";
    std::getline(mif_file, line); new_mif << line << "\n";

    // Then process the remaining lines in the file, observing the callers desire to throw out assets not at risk...
    int featureIndex = 0;
    while(!mif_file.eof()){
      // Read the next line...
      std::getline(mif_file, line);

      if(line.size() > 0){
        // Check to see if we are throwing it out and it is a qualifying asset...
        if(!removeFeature.at(featureIndex) && removeNoRiskAssets)
          new_mif << line << "\n";

        // Increment the feature index if this is the end of a description...
        if(line.find("Pen") != std::string::npos )
          featureIndex++;
      }
    }

    //Close the files...
    mif_file.close();
    new_mif.close();

  }
} // oia_risk_model

#endif //MIF_H
