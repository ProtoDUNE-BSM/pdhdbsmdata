////////////////////////////////////////////////////////////////////////////////////////////////
//// Class:       PDHDSPSSpillFilter
//// Plugin Type: filter (Unknown Unknown)
//// File:        PDHDSPSSpillFilter_module.cc
//// Author:      Ciaran Hasnip (CERN)
//// Version:     2.0 (2024-12-05) by Ciaran Hasnip (CERN) & Hamza Amar Es-sghir (IFIC-Valencia)
//// Description: Filter module designed to read in SPS beam data from a .csv file
////              and select events that occured when the beam was ON or OFF. 
////              The user decides if they want ON or OFF spill events, 
////              as well as the PoT threshold.
////////////////////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>

#include "lardataobj/RawData/RawDigit.h"
#include "lardataobj/RawData/RDTimeStamp.h"

#include "art/Framework/Core/EDFilter.h" 
#include "art/Framework/Core/ModuleMacros.h" 
#include "art/Framework/Principal/Event.h" 
#include "art_root_io/TFileService.h"

#include "detdataformats/trigger/Types.hpp"

namespace pdhd {

using timestamp_t = dunedaq::trgdataformats::timestamp_t;

class PDHDSPSSpillFilter : public art::EDFilter {
public:
    explicit PDHDSPSSpillFilter(fhicl::ParameterSet const & pset);
    virtual ~PDHDSPSSpillFilter() = default;
    bool filter(art::Event& e) override;
    void beginJob() override;

private:
    int fRun;
    int fSubRun;
    unsigned int fEventID;
    uint64_t fEventTimeStamp; // Timestamp from art::Event object

    std::string fInputLabel;
    std::string fSpillLabel;
    std::string fSPSBeamData; // Name and path of input csv file for SPS beam data
    std::ifstream fInputData;
    bool fSpillOn; // To filter for spill ON or OFF. It is set to true by default
    uint64_t fPoT_threshold;
    bool fUseSpillFlag;
    
    std::vector<std::pair<timestamp_t, uint64_t>> vSpillClockPoT; // Vector to store the spill clock times and PoT values
};

// Constructor of the class PDHDSPSSpillFilter
PDHDSPSSpillFilter::PDHDSPSSpillFilter(fhicl::ParameterSet const & pset)
    : EDFilter(pset), 
      fInputLabel(pset.get<std::string>("InputTag")), 
      fSpillLabel(pset.get<std::string>("SpillTag")), 
      fSPSBeamData(pset.get<std::string>("sps_beamdata")),
      fSpillOn(pset.get<bool>("spill_on", true)),
      fPoT_threshold(pset.get<uint64_t>("PoT_threshold")),
      fUseSpillFlag(pset.get<bool>("useSpillFlag", false)) {}

// Filter events according to SPS beam spill data
bool PDHDSPSSpillFilter::filter(art::Event & evt) {
    // Filter designed for Data only. Do not want to filter on MC
    if (!evt.isRealData()) {
        return true;
    }

    fRun = evt.run();
    fSubRun = evt.subRun();
    fEventID = evt.id().event();

    std::cout << "###PDHDSPSSpillFilter###\n"
              << "START PDHDSPSSpillFilter for Event " << fEventID << " in Run " << fRun << "\n\n";

    // If there is no spill flag product always do the manual spill filtering
    art::Handle<std::vector<bool>> spillHandle;
    if (!evt.getByLabel(fSpillLabel, spillHandle)) fUseSpillFlag = false;
    
    bool filter_pass = false;
    
    if (!fUseSpillFlag) {
      uint64_t timeHigh_ns = evt.time().timeHigh() * 1e9;
      uint64_t timeLow_ns = evt.time().timeLow();
      fEventTimeStamp = (timeHigh_ns + timeLow_ns) * 1e-6;

      std::cout << "Event " << fEventID << ", Timestamp = " << fEventTimeStamp << " ms\n";

      for (size_t spill = 0; spill < vSpillClockPoT.size(); ++spill) {
          if (vSpillClockPoT[spill].first > fEventTimeStamp) {
              timestamp_t spill_end = vSpillClockPoT[spill - 1].first + 4785; // 4785 ms is the duration of a spill
              if (fEventTimeStamp < spill_end) {
                std::cout << "Spill ON\n";
                filter_pass = fSpillOn;
              } else {
                std::cout << "Spill OFF\n";
                filter_pass = !fSpillOn;
              }
              break;
          }
      }
    } else {
        for (auto const& spill : (*spillHandle)) { // should only have 1 element
            filter_pass = spill;
        }
    }

    std::string spill_string;
    if (filter_pass) spill_string = "ON";
    else spill_string = "OFF";

    std::cout << ">>> Spill is " << spill_string << std::endl;
    std::cout << "END PDHDSPSSpillFilter for Event " << fEventID << " in Run " << fRun << "\n\n";
    return filter_pass;
}

// Read in the SPS beam data from the .csv file and store the spill clock times and PoT in a vector
void PDHDSPSSpillFilter::beginJob() {
    std::cout << "SPS beam data file: " << fSPSBeamData << "\n";
    fInputData.open(fSPSBeamData);

    if (!fInputData.good()) {
        throw std::runtime_error("Input csv file " + fSPSBeamData + " cannot be read.");
    }

    std::string line;
    // Read and discard the header line
    if (std::getline(fInputData, line)) {
        // Optionally, you can store the header if needed
        // std::vector<std::string> header;
        // std::stringstream headerStream(line);
        // std::string cell;
        // while (std::getline(headerStream, cell, ',')) {
        //     header.push_back(cell);
        // }
    }

    while (std::getline(fInputData, line)) {
        std::vector<std::string> data;
        std::stringstream lineStream(line);
        std::string cell;
        while (std::getline(lineStream, cell, ',')) {
            data.push_back(cell);
        }
        
        try {
            timestamp_t clock = static_cast<timestamp_t>(std::stod(data[0])*1e3); // Convert the string in ms to a double and then to a timestamp_t
            uint64_t PoT = static_cast<uint64_t>(std::stoull(data[1])); // Convert the string to an unsigned long long and then to a uint64_t
            if (PoT >= fPoT_threshold) {
                vSpillClockPoT.push_back(std::make_pair(clock, PoT)); // Store the spill clock time and PoT value
            }
        } catch (const std::invalid_argument& e) {
            // Handle the case where the string is not a valid double
        } catch (const std::out_of_range& e) {
            // Handle the case where the double is out of range
        }
        
    }

    std::cout << "In " << fSPSBeamData << " there are " << vSpillClockPoT.size() << " SPS beam spills.\n\n";
}

DEFINE_ART_MODULE(PDHDSPSSpillFilter)

}
