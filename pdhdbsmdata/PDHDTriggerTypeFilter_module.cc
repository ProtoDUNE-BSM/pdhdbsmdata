////////////////////////////////////////////////////////////////////////
//// Class:       PDHDTriggerTypeFilter
//// Plugin Type: filter (Unknown Unknown)
//// File:        PDHDTriggerTypeFilter_module.cc
//// Author:      Ciaran Hasnip (CERN)
//// 
//// Filter that removes ground shake events by looking for 
//// Trigger Candidates with type ADCSimpleWindow. One occurance of this
//// TC type removes the event.
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

#include "detdataformats/trigger/TriggerObjectOverlay.hpp"
#include "detdataformats/trigger/TriggerPrimitive.hpp"
#include "detdataformats/trigger/TriggerActivityData.hpp"
#include "detdataformats/trigger/TriggerCandidateData.hpp"

namespace pdhd {

using timestamp_t = dunedaq::trgdataformats::timestamp_t;
using type_t = dunedaq::trgdataformats::TriggerCandidateData::Type;

std::map<dunedaq::trgdataformats::TriggerCandidateData::Type, std::string> map_trigger_candidate_type_names = 
  dunedaq::trgdataformats::get_trigger_candidate_type_names();

//-------------------------------------
class PDHDTriggerTypeFilter : public art::EDFilter {
  public:
    explicit PDHDTriggerTypeFilter(fhicl::ParameterSet const & pset);
    virtual ~PDHDTriggerTypeFilter() {};
    virtual bool filter(art::Event& e);
    void beginJob();

  private:

    int fRun;
    int fSubRun;
    unsigned int fEventID;
    // Timestamp from art::Event object
    uint64_t fEventTimeStamp;

    std::string fInputLabel;
    bool fDebug;
    
    std::vector<dunedaq::trgdataformats::TriggerCandidateData> fTriggerCandidate;
};

//-------------------------------------
PDHDTriggerTypeFilter::PDHDTriggerTypeFilter::PDHDTriggerTypeFilter(fhicl::ParameterSet const & pset):
  EDFilter(pset), 
  fInputLabel(pset.get<std::string>("InputTag")),
  fDebug(pset.get<bool>("Debug")) {}

//-------------------------------------
bool PDHDTriggerTypeFilter::filter(art::Event & evt) {

  if (!evt.isRealData()) {
    //Filter is designed for Data only. Don't want to filter on MC
    return true;
  }
 
  fRun = evt.run();
  fSubRun = evt.subRun();
  fEventID = evt.id().event();
  
  std::cout << "###PDHDTriggerTypeFilter###"<< std::endl
    << "START PDHDTriggerTypeFilter for Event " << fEventID << " in Run " << fRun << std::endl << std::endl;

  uint64_t timeHigh_ns = evt.time().timeHigh()*1e9;
  uint64_t timeLow_ns = evt.time().timeLow();

  fEventTimeStamp = timeHigh_ns + timeLow_ns;

  std::cout << "Event " << fEventID << ", Timestamp = " << fEventTimeStamp << std::endl;

  fEventTimeStamp *= 1e-6;
  std::cout << "ms Seconds Timestamp = " << fEventTimeStamp << std::endl;

  auto triggerCandidateHandle = evt.getValidHandle<std::vector<dunedaq::trgdataformats::TriggerCandidateData>>(fInputLabel);
  fTriggerCandidate = *triggerCandidateHandle;

  std::vector<timestamp_t> vTCtimestamps;
  std::vector<type_t> vTCtypes;

  for (const auto &tc : fTriggerCandidate) {
    //timestamp_t trigger_time_ms = tc.time_start * 16e-9 * 1e3;
    timestamp_t trigger_time_ms = tc.time_end * 16e-9 * 1e3;
    vTCtimestamps.push_back(trigger_time_ms);
    std::cout << "Event " << fEventID << ", Timestamp = " << fEventTimeStamp << ", TC time = " << trigger_time_ms << std::endl;
    if (fDebug) {
      if (fEventTimeStamp != trigger_time_ms) {
        std::cout << "[WARNING] art::Event timestamp and TC timestamp do not match. Investigate!" << std::endl;
      }
    }
    // Look at the algorithm
    type_t type_tc = tc.type;
    vTCtypes.push_back(type_tc);
  }
  std::cout << "Number of TCs = " << vTCtypes.size() << std::endl;

  // Immediately reject event if it has a TC made with ground shake algorithm
  for (const auto &typ : vTCtypes) {
    std::cout << "Type: " << map_trigger_candidate_type_names[typ] << std::endl;
    if (typ == type_t::kSupernova) {
      std::cout << "kSupernova Trigger Algorithm!" << std::endl;
      // do nothing ,all is good
    }
    if (typ == type_t::kADCSimpleWindow) {
      std::cout << "kADCSimpleWindow Trigger Algorithm! Removing ground shake." << std::endl;
      // If one of your TCs is groundshake then it is probably all groundshake. Remove.
      return false;
    }
  }
  
  std::cout << "END PDHDTriggerTypeFilter for Event " << fEventID << " in Run " << fRun << std::endl << std::endl;

  // If you made it here then you only had physics triggers
  return true;
}

//-------------------------------------
void PDHDTriggerTypeFilter::beginJob() {}

DEFINE_ART_MODULE(PDHDTriggerTypeFilter)

}
