#ifndef VEHICLES_H
#define VEHICLES_H

#include <map>
#include <cmath>

#include "utils.h"
#include "exceptions.h"

namespace oia_risk_model{
  namespace vehicles{

    // Structure to describe a vehicle, in terms of its emissions as a function of speed.
    struct Vehicle{
      double              factor; // Emissions scale factor (generally 1, used to modify profile by age)
      std::vector<double> coeffs; // Vector of coefficients
      // Vehicle constructor: needs scale-factor (f) and vector of 6 coefficients (coeffs)
      Vehicle(const double f, const std::vector<double> c) : factor(f), coeffs(c) {}
      // Calculate the emissions as per the TRI paper on emissions with speed...
      double emissions(const double speed_km) const {
        double g_km = coeffs.at(0);
        for(std::size_t i=1; i<coeffs.size(); i++){
          g_km += coeffs.at(i)*std::pow(speed_km,i);
        }
        return factor * g_km / speed_km;
      }
    };

    // NOTE: This code assumes Euro emissions standards for vehicle age:
    // "OLD" ~=  20 years old...
    // "MED" ~=  10 years old...
    // "NEW" <=   5 years old (mean 2)...
    // Mean ages of cars in europe are ~= 8y Austria, 10y France, 12y Spain, 14y Poland, 16.5y Romania.
    // Mean ages of trucks in europe are ~= 6.4y Austria, 9.3y France, 14.7y Spain, 12.2y Poland, 16.1y Romania.
    // Based on economic classification of country, I am assuming the following:
    //
    //                   CARS AND TRUCKS
    //                OLD   MED   NEW   (AV)
    // high:          30    50    20   (11.4) // Consistent with EU average...
    // upper-middle:  50    30    20   (13.2)
    // lower-middle:  60    30    10   (15.2)
    // low:           70    20    10   (16.2)

    // Simple car-fleet representation...
    struct CarFleet{
      std::string                                     country;  // Name of the country fleet belongs to
      int                                           passenger;  // Number of passenger cars in the fleet
      int                                          commercial;  // Number of commercial vehicles in the fleet
      static std::map<std::string,Vehicle*>          vehicles;  // Vector of vehicle names and Vehicle repr (static)
      std::vector<std::pair<std::string,double>> distribution;  // Vector of vehicls distributions
      // Constructor for fleet, taking 3-letter country-name...
      CarFleet(const std::string c,
               const std::string income,
               const int passenger,
               const int commercial,
               const std::string emissionsFile="./data/emissions") : country(c), passenger(passenger), commercial(commercial) {
        // Make sure the emissions file exists...
        if(!utils::exists(emissionsFile)){
          Exception("The emissions file does not exist (i.e. this one: " + emissionsFile + ")");
          return;
        }

        // Initialise the vehicle fleet...
        readVehiclesFile(emissionsFile);

        // Set the share of vehicles by age according to national income...
        double pOld = income == "high" ? 0.30 : income == "upper-middle" ? 0.50 : income == "lower-middle" ? 0.60 : 0.70;
        double pMed = income == "high" ? 0.50 : income == "upper-middle" ? 0.30 : income == "lower-middle" ? 0.30 : 0.20;
        double pNew = income == "high" ? 0.20 : income == "upper-middle" ? 0.20 : income == "lower-middle" ? 0.10 : 0.10;

        // Global distribution of car sizes as of 2021...
        double pSmall = 0.36;
        double pLarge = 0.64;

        // Populate the distribution of the vehicles...
        distribution.push_back(std::pair<std::string,double>("small_car_old", pSmall * pOld * passenger));
        distribution.push_back(std::pair<std::string,double>("small_car_med", pSmall * pMed * passenger));
        distribution.push_back(std::pair<std::string,double>("small_car_new", pSmall * pNew * passenger));
        distribution.push_back(std::pair<std::string,double>("large_car_old", pLarge * pOld * passenger));
        distribution.push_back(std::pair<std::string,double>("large_car_med", pLarge * pMed * passenger));
        distribution.push_back(std::pair<std::string,double>("large_car_new", pLarge * pNew * passenger));
        distribution.push_back(std::pair<std::string,double>("lgv_I_old",     0.20   * pOld * commercial)); // All commercial vehicles are assumed to split equally by class...
        distribution.push_back(std::pair<std::string,double>("lgv_I_med",     0.20   * pMed * commercial));
        distribution.push_back(std::pair<std::string,double>("lgv_I_new",     0.20   * pNew * commercial));
        distribution.push_back(std::pair<std::string,double>("lgv_II_old",    0.20   * pOld * commercial));
        distribution.push_back(std::pair<std::string,double>("lgv_II_med",    0.20   * pMed * commercial));
        distribution.push_back(std::pair<std::string,double>("lgv_II_new",    0.20   * pNew * commercial));
        distribution.push_back(std::pair<std::string,double>("lgv_III_old",   0.20   * pOld * commercial));
        distribution.push_back(std::pair<std::string,double>("lgv_III_med",   0.20   * pMed * commercial));
        distribution.push_back(std::pair<std::string,double>("lgv_III_new",   0.20   * pNew * commercial));
        distribution.push_back(std::pair<std::string,double>("rhgv_old",      0.20   * pOld * commercial));
        distribution.push_back(std::pair<std::string,double>("rhgv_med",      0.20   * pMed * commercial));
        distribution.push_back(std::pair<std::string,double>("rhgv_new",      0.20   * pNew * commercial));
        distribution.push_back(std::pair<std::string,double>("ahgv_old",      0.20   * pOld * commercial));
        distribution.push_back(std::pair<std::string,double>("ahgv_med",      0.20   * pMed * commercial));
        distribution.push_back(std::pair<std::string,double>("ahgv_new",      0.20   * pNew * commercial));
      }
      // Helper function to read a vector of vehicles from disk...
      void readVehiclesFile(const std::string fileName){
        // Get out of dodge...
        if(!utils::exists(fileName))
            Exception("Vehicles file does not exist.");

        // Open the file...
        std::ifstream inFile;
        inFile.open(fileName);

        std::string line;
        std::string header;

        // Test the file is correctly formatted...
        std::getline(inFile,header);
        std::vector<std::string> headerWords = utils::readLine(header);

        // Make sure the header is the correct format...
        if(headerWords.size() != 9){
          Exception("The header of the emissions file seems to have the wrong number data.");
          return;
        }

        // Read the vehicles file line by line...
        while(!inFile.eof()){
          std::getline(inFile, line);
          std::vector<std::string> words = utils::readLine(line);

          // If the line is the correct length, parse it out...
          if(words.size() == 9){
            std::vector<double> c;
            for(std::size_t i=2; i<words.size(); i++){
              c.push_back(std::stod(words.at(i)));
            }
            // Insert the vehicle object into the std::map...
            auto ret = CarFleet::vehicles.insert(std::pair<std::string,Vehicle*>(words.at(0), new Vehicle(std::stod(words.at(1)), c)));
          }
        }
      }
      // Helper function to calculate fleet emissions for a given speed...
      double emissions(const double speed_km, const double dist_km=1.0) const {
        double total_emissions = 0;
        // Accumulate the total emissions for a fleet of vehicles at a given speed...
        for(auto p : distribution){
          total_emissions += vehicles[p.first]->emissions(speed_km) * p.second * dist_km;
        }
        return total_emissions;
      }
      // Helper function to calculate passenger-fleet emissions for a given speed...
      double passengerEmissions(const double speed_km, const double dist_km=1.0) const {
        double passenger_emissions = 0;
        // Accumulate the total emissions for a fleet of vehicles at a given speed...
        for(auto p : distribution){
          if(p.first.compare("car") != std::string::npos){
            passenger_emissions += vehicles[p.first]->emissions(speed_km) * p.second * dist_km;
          }
        }
        return passenger_emissions;
      }
      // Helper function to calculate commercial-fleet emissions for a given speed...
      double commercialEmissions(const double speed_km, const double dist_km=1.0) const {
        double commercial_emissions = 0;
        // Accumulate the total emissions for a fleet of vehicles at a given speed...
        for(auto p : distribution){
          if(p.first.compare("car") == std::string::npos){
            commercial_emissions += vehicles[p.first]->emissions(speed_km) * p.second * dist_km;
          }
        }
        return commercial_emissions;
      }
      // Helper function to return the total number of vehicles per day that generate a given total of emissions...
      std::pair<double,double>vehiclesPerDay(const double tonnes_nox, const double speed_km, const double dist_km) const {
        // So, what kind of emissions can we expect from the full fleet at this speed, over this distance?
        double t_km_fleet      =           emissions(speed_km, dist_km) / 1000000.0; // CONVERT THIS DATA TO TONNES...
        double t_km_passenger  =  passengerEmissions(speed_km, dist_km) / 1000000.0;
        double t_km_commercial = commercialEmissions(speed_km, dist_km) / 1000000.0;

        // Contribution to total from each type of transport?
        double t_passenger  = (t_km_passenger  / t_km_fleet) * tonnes_nox;
        double t_commercial = (t_km_commercial / t_km_fleet) * tonnes_nox;

        // How many passenger trips can we make per tonne at this speed?
        double passenger_per_year  = passenger  * t_passenger  / t_km_passenger;
        double commercial_per_year = commercial * t_commercial / t_km_commercial;

        // Return the daily number of vehicles...
        return std::pair<double,double>(passenger_per_year / 365.25, commercial_per_year / 365.25);
      }
    };

    // Initialise the static members...
    std::map<std::string,Vehicle*> CarFleet::vehicles;
  } // vehicles
} // oia_risk_model

#endif //VEHICLES_H
