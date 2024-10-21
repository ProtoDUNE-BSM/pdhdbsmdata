////////////////////////////////////////////////////////////////////////
//// Class:       PDHDSPSSpillFilter
//// Plugin Type: filter (Unknown Unknown)
//// File:        PDHDSPSSpillFilter_module.cc
//// Author:      Ciaran Hasnip (CERN)
//// 
//// Filter module designed to read in SPS beam data from a .csv file and
//// select events that occured when the beam was ON or OFF. The user 
//// decides if they want on or off spill events.
//////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <utility>
#include <set>

#include "lardataobj/RawData/RawDigit.h"
#include "lardataobj/RawData/RDTimeStamp.h"

#include "art/Framework/Core/EDAnalyzer.h" 
#include "art/Framework/Core/EDFilter.h" 
#include "art/Framework/Core/ModuleMacros.h" 
#include "art/Framework/Principal/Event.h" 
#include "art_root_io/TFileService.h"

#include "detdataformats/trigger/Types.hpp"

namespace pdhd {

using timestamp_t = dunedaq::trgdataformats::timestamp_t;

//-------------------------------------
class PDHDSPSSpillFilter : public art::EDFilter {
  public:
    explicit PDHDSPSSpillFilter(fhicl::ParameterSet const & pset);
    virtual ~PDHDSPSSpillFilter() {};
    virtual bool filter(art::Event& e);
    void beginJob();

  private:

    int fRun;
    int fSubRun;
    unsigned int fEventID;
    // Timestamp from art::Event object
    uint64_t fEventTimeStamp;

    std::string fInputLabel;
    // Name and path of input csv file for SPS beam data
    std::string fSPSBeamData;
    std::ifstream* fInputData;
    // User can decide if they want to filter for spill ON or OFF
    bool fSpillOn;
    
    std::vector<timestamp_t> vSpillClock;
};

//-------------------------------------
PDHDSPSSpillFilter::PDHDSPSSpillFilter::PDHDSPSSpillFilter(fhicl::ParameterSet const & pset):
  EDFilter(pset), 
  fInputLabel(pset.get<std::string>("InputTag")), 
  fSPSBeamData(pset.get<std::string>("sps_beamdata")),
  fInputData(nullptr), 
  fSpillOn(pset.get<bool>("spill_on", true)) {}

//-------------------------------------
bool PDHDSPSSpillFilter::filter(art::Event & evt) {

  if (!evt.isRealData()) {
    //Filter is designed for Data only. Don't want to filter on MC
    return true;
  }
 
  fRun = evt.run();
  fSubRun = evt.subRun();
  fEventID = evt.id().event();
  
  std::cout << "###PDHDSPSSpillFilter###"<< std::endl
    << "START PDHDSPSSpillFilter for Event " << fEventID << " in Run " << fRun << std::endl << std::endl;

  uint64_t timeHigh_ns = evt.time().timeHigh()*1e9;
  uint64_t timeLow_ns = evt.time().timeLow();

  fEventTimeStamp = timeHigh_ns + timeLow_ns;

  std::cout << "Event " << fEventID << ", Timestamp = " << fEventTimeStamp << std::endl;

  fEventTimeStamp *= 1e-6;
  std::cout << "ms Seconds Timestamp = " << fEventTimeStamp << std::endl;

  bool filter_pass(false);

  for (int spill = 0; spill < (int)vSpillClock.size(); spill++) {
    if (vSpillClock.at(spill) > fEventTimeStamp) {
      timestamp_t spill_end = vSpillClock.at(spill - 1) + 4.785e3;
      if (fEventTimeStamp < spill_end) {
        std::cout << "Spill ON " << std::endl;
        fSpillOn ? (filter_pass = true) : (filter_pass = false);
      } else {
        std::cout << "Spill OFF " << std::endl;
        fSpillOn ? (filter_pass = false) : (filter_pass = true);
      }
      break;
    }
  }
  std::cout << "END PDHDSPSSpillFilter for Event " << fEventID << " in Run " << fRun << std::endl << std::endl;

  return filter_pass;
}

//-------------------------------------
void PDHDSPSSpillFilter::beginJob() {
  
  std::cout << "SPS beam data file : " << fSPSBeamData << std::endl;
  fInputData = new std::ifstream(fSPSBeamData.c_str(), std::ifstream::in);

  // check that the file is a good one
  if( !fInputData->good() )
    throw cet::exception("HepMCFileGen") << "input text file "
      << fSPSBeamData << " cannot be read.\n";

  std::string line;
  while (std::getline(*fInputData, line)) {
    std::stringstream lineStream(line);
    std::string cell;
    std::vector<std::string> row;
    while (std::getline(lineStream, cell, ',')) {
      row.push_back(cell);
    }
    // column to extrac is row.size - 4
    timestamp_t clock = static_cast<timestamp_t>(std::stod(row.at(row.size()-4)));
    vSpillClock.push_back(clock);
  }
  std::cout << "In " << fSPSBeamData << " there are " << vSpillClock.size() << " SPS beam spills." << std::endl;
}

DEFINE_ART_MODULE(PDHDSPSSpillFilter)

}
