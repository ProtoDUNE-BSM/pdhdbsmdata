////////////////////////////////////////////////////////////////////////
//// Class:       PDHDSPSSpillFilter
//// Plugin Type: filter (Unknown Unknown)
//// File:        PDHDSPSSpillFilter_module.cc
//// Author:      Ciaran Hasnip (CERN)
//// Version:     2.0 (2024-12-05) by Ciaran Hasnip (CERN) & Hamza Amar Es-sghir (IFIC-Valencia)
//// Filter module designed to read in SPS beam data from a .csv file and
//// select events that occured when the beam was ON or OFF. 
//// The user decides if they want on or off spill events.
//////////////////////////////////////////////////////////////////////////

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
    std::string fSPSBeamData; // Name and path of input csv file for SPS beam data
    std::ifstream fInputData;
    bool fSpillOn; // To filter for spill ON or OFF. It is set to true by default
    
    std::vector<timestamp_t> vSpillClock;
};

// Constructor of the class PDHDSPSSpillFilter
PDHDSPSSpillFilter::PDHDSPSSpillFilter(fhicl::ParameterSet const & pset)
    : EDFilter(pset), 
      fInputLabel(pset.get<std::string>("InputTag")), 
      fSPSBeamData(pset.get<std::string>("sps_beamdata")),
      fSpillOn(pset.get<bool>("spill_on", true)) {}

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

    uint64_t timeHigh_ns = evt.time().timeHigh() * 1e9;
    uint64_t timeLow_ns = evt.time().timeLow();
    fEventTimeStamp = (timeHigh_ns + timeLow_ns) * 1e-6;

    std::cout << "Event " << fEventID << ", Timestamp = " << fEventTimeStamp << " ms\n";

    bool filter_pass = false;

    for (size_t spill = 0; spill < vSpillClock.size(); ++spill) {
        if (vSpillClock[spill] > fEventTimeStamp) {
            timestamp_t spill_end = vSpillClock[spill - 1] + 4785; // 4785 ms is the duration of a spill
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

    std::cout << "END PDHDSPSSpillFilter for Event " << fEventID << " in Run " << fRun << "\n\n";
    return filter_pass;
}

// Read in the SPS beam data from the .csv file and store the spill clock times (at row.size - 4 column) in a vector
void PDHDSPSSpillFilter::beginJob() {
    std::cout << "SPS beam data file: " << fSPSBeamData << "\n";
    fInputData.open(fSPSBeamData);

    if (!fInputData.good()) {
        throw std::runtime_error("Input csv file " + fSPSBeamData + " cannot be read.");
    }

    std::string line;
    while (std::getline(fInputData, line)) {
        std::stringstream lineStream(line);
        std::string cell;
        std::vector<std::string> row;
        while (std::getline(lineStream, cell, ',')) {
            row.push_back(cell);
        }
        if (row.size() >= 4) {
            // timestamp_t clock = static_cast<timestamp_t>(std::stod(row[row.size() - 4]));
            // vSpillClock.push_back(clock);
            try {
                timestamp_t clock = static_cast<timestamp_t>(std::stod(row[row.size() - 4]));
                vSpillClock.push_back(clock);
            } catch (const std::invalid_argument& e) {
                // Handle the case where the string is not a valid double
            } catch (const std::out_of_range& e) {
                // Handle the case where the double is out of range
            }
        }
    }

    std::cout << "In " << fSPSBeamData << " there are " << vSpillClock.size() << " SPS beam spills.\n\n";
}

DEFINE_ART_MODULE(PDHDSPSSpillFilter)

}