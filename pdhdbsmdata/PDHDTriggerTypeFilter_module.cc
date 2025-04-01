////////////////////////////////////////////////////////////////////////////////////////////////
//// Class:       PDHDTriggerTypeFilter
//// Plugin Type: filter (Unknown Unknown)
//// File:        PDHDTriggerTypeFilter_module.cc
//// Author:      Ciaran Hasnip (CERN)
//// Version:     2.0 (2024-12-05) by Ciaran Hasnip (CERN) & Hamza Amar Es-sghir (IFIC-Valencia)
//// Description: Filter that removes ground shake events by looking for 
////              Trigger Candidates with type ADCSimpleWindow.
////              One occurrence of this TC type removes the event.
////////////////////////////////////////////////////////////////////////////////////////////////

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

#include "detdataformats/trigger/TriggerObjectOverlay.hpp"
#include "detdataformats/trigger/TriggerPrimitive.hpp"
#include "detdataformats/trigger/TriggerActivityData.hpp"
#include "detdataformats/trigger/TriggerCandidateData.hpp"

namespace pdhd {

using timestamp_t = dunedaq::trgdataformats::timestamp_t;
using type_t = dunedaq::trgdataformats::TriggerCandidateData::Type;

std::map<dunedaq::trgdataformats::TriggerCandidateData::Type, std::string> map_trigger_candidate_type_names = 
  dunedaq::trgdataformats::get_trigger_candidate_type_names();

class PDHDTriggerTypeFilter : public art::EDFilter {
  public:
    explicit PDHDTriggerTypeFilter(fhicl::ParameterSet const & pset);
    virtual ~PDHDTriggerTypeFilter() {};
    bool filter(art::Event& e) override;
    void beginJob() override;

  private:

    int fRun;
    int fSubRun;
    unsigned int fEventID;
    uint64_t fEventTimeStamp; // Timestamp from art::Event object

    std::string fInputLabel;
    bool fDebug;
    bool fVerbose;
};

// Constructor of the class PDHDTriggerTypeFilter
PDHDTriggerTypeFilter::PDHDTriggerTypeFilter(fhicl::ParameterSet const & pset)
  : EDFilter(pset) 
  , fInputLabel(pset.get<std::string>("InputTag"))
  , fDebug(pset.get<bool>("Debug"))
  , fVerbose(pset.get<bool>("verbose", true)) {}

// Filter function
bool PDHDTriggerTypeFilter::filter(art::Event & evt) {

  if (!evt.isRealData()) {
    //Filter is designed for Data only. Don't want to filter on MC
    return true;
  }
 
  fRun = evt.run();
  fSubRun = evt.subRun();
  fEventID = evt.id().event();
  
  if (fVerbose) 
    std::cout << "###PDHDTriggerTypeFilter###\n" 
    << "START PDHDTriggerTypeFilter for Event " << fEventID << " in Run " << fRun << "\n\n";

  if (fDebug) {
    uint64_t timeHigh_ns = evt.time().timeHigh()*1e9;
    uint64_t timeLow_ns = evt.time().timeLow();
    fEventTimeStamp = timeHigh_ns + timeLow_ns;
    fEventTimeStamp *= 1e-6;
    std::cout << "Seconds Timestamp = " << fEventTimeStamp << "\n";
  }

  auto triggerCandidateHandle = evt.getValidHandle<std::vector<dunedaq::trgdataformats::TriggerCandidateData>>(fInputLabel);
  const auto& triggerCandidates = *triggerCandidateHandle;

  for (const auto &tc : triggerCandidates) {
    if (fDebug) {
      timestamp_t trigger_time_ms = tc.time_end * 16e-6; // 16e-9 s * 1e3 time_end is the time of the last sample in the window
      std::cout << "Event " << fEventID << ", Timestamp = " << fEventTimeStamp << ", TC time = " << trigger_time_ms << "\n";
      if (fEventTimeStamp != trigger_time_ms) {
        std::cout << "[WARNING] art::Event timestamp and TC timestamp do not match. Investigate!\n";
      }
    }
    // Look at the algorithm type
    type_t type_tc = tc.type;
    if (fVerbose) std::cout << "Type: " << map_trigger_candidate_type_names[type_tc] << "\n";
    if (type_tc == type_t::kADCSimpleWindow) {
      if (fVerbose) 
        std::cout << "kADCSimpleWindow Trigger Algorithm! Removing ground shake.\n";
      return false;
    }
  }
  
  if (fVerbose) 
    std::cout << "END PDHDTriggerTypeFilter for Event " << fEventID << " in Run " << fRun << "\n\n";

  // If you made it here then you only had physics triggers
  return true;
}

// Begin job function
void PDHDTriggerTypeFilter::beginJob() {}

DEFINE_ART_MODULE(PDHDTriggerTypeFilter)

}
