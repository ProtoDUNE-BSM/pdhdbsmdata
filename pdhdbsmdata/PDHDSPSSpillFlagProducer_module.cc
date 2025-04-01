////////////////////////////////////////////////////////////////////////
// Class:       PDHDSPSSpillFlagProducer
// Plugin Type: producer (Unknown Unknown)
// File:        PDHDSPSSpillFlagProducer_module.cc
//
// Generated at Tue Mar 11 10:10:40 2025 by Ciaran Hasnip using cetskelgen
// from cetlib version 3.18.02.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "detdataformats/trigger/Types.hpp"

#include <memory>

namespace sps {
  class PDHDSPSSpillFlagProducer;
  using spill_t = bool;
}

using timestamp_t = dunedaq::trgdataformats::timestamp_t;

class sps::PDHDSPSSpillFlagProducer : public art::EDProducer {
public:
  explicit PDHDSPSSpillFlagProducer(fhicl::ParameterSet const& p);
  // The compiler-generated destructor is fine for non-base
  // classes without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  PDHDSPSSpillFlagProducer(PDHDSPSSpillFlagProducer const&) = delete;
  PDHDSPSSpillFlagProducer(PDHDSPSSpillFlagProducer&&) = delete;
  PDHDSPSSpillFlagProducer& operator=(PDHDSPSSpillFlagProducer const&) = delete;
  PDHDSPSSpillFlagProducer& operator=(PDHDSPSSpillFlagProducer&&) = delete;

  // Required functions.
  void produce(art::Event& e) override;

  // Selected optional functions.
  void beginJob() override;
  void endJob() override;

private:

  // Declare member data here.
  int fRun;
  int fSubRun;
  unsigned int fEventID;
  uint64_t fEventTimeStamp; // Timestamp from art::Event object

  std::string fOutputLabel;
  std::string fSPSBeamData; // Name and path of input csv file for SPS beam data
  std::ifstream fInputData;
  uint64_t fPoT_threshold;
  bool fVerbose;
    
  std::vector<std::pair<timestamp_t, uint64_t>> vSpillClockPoT; // Vector to store the spill clock times and PoT values

  // The key result variable: is spill on or off?
  sps::spill_t fSpillResult;

};


sps::PDHDSPSSpillFlagProducer::PDHDSPSSpillFlagProducer(fhicl::ParameterSet const& p)
  : EDProducer{p}
  , fOutputLabel(p.get<std::string>("OutputTag"))
  , fSPSBeamData(p.get<std::string>("sps_beamdata"))
  , fPoT_threshold(p.get<uint64_t>("PoT_threshold"))
  , fVerbose(p.get<bool>("verbose", true))
{
  produces<std::vector<sps::spill_t>> (fOutputLabel);
}

void sps::PDHDSPSSpillFlagProducer::produce(art::Event& e)
{
  // Implementation of required member function here.
  // Filter designed for Data only. Do not want to filter on MC
  if (!e.isRealData()) {
    return;
  }

  fRun = e.run();
  fSubRun = e.subRun();
  fEventID = e.id().event();

  if (fVerbose)
    std::cout << "###PDHDSPSSpillFilter###\n"
    << "START PDHDSPSSpillFilter for Event " << fEventID << " in Run " << fRun << "\n\n";

  uint64_t timeHigh_ns = e.time().timeHigh() * 1e9;
  uint64_t timeLow_ns = e.time().timeLow();
  fEventTimeStamp = (timeHigh_ns + timeLow_ns) * 1e-6;

  if (fVerbose) 
    std::cout << "Event " << fEventID << ", Timestamp = " << fEventTimeStamp << " ms\n";

  fSpillResult = false;

  for (size_t spill = 0; spill < vSpillClockPoT.size(); ++spill) {
    if (vSpillClockPoT[spill].first > fEventTimeStamp) {
      timestamp_t spill_end = vSpillClockPoT[spill - 1].first + 4785; // 4785 ms is the duration of a spill
      if (fEventTimeStamp < spill_end) {
        if (fVerbose) std::cout << "Spill ON\n";
        fSpillResult = true;
      } else {
        if (fVerbose) std::cout << "Spill OFF\n";
        fSpillResult = false;
      }
      break;
    }
  }

  // Get the spill on/off result and store it in a vector
  std::vector<sps::spill_t> spill_result_output = { fSpillResult };

  // Put the vector with our spill result in the art event
  e.put(std::make_unique<std::vector<sps::spill_t>>(std::move(spill_result_output)), fOutputLabel);

  if (fVerbose) std::cout << "END PDHDSPSSpillFilter for Event " << fEventID << " in Run " << fRun << "\n\n";
}

void sps::PDHDSPSSpillFlagProducer::beginJob()
{
  // Implementation of optional member function here.
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

void sps::PDHDSPSSpillFlagProducer::endJob()
{
  // Implementation of optional member function here.
}

DEFINE_ART_MODULE(sps::PDHDSPSSpillFlagProducer)
