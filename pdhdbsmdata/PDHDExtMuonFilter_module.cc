////////////////////////////////////////////////////////////////////////
//// Class:       PDHDExtMuonFilter
//// Plugin Type: filter (Unknown Unknown)
//// File:        PDHDExtMuonFilter_module.cc
//// Author:      Ciaran Hasnip (CERN)
//// 
//// Filter that finds the centre of the energy deposition shower in the
//// trigger region (i.e. the TA in the corresponding APA) and defines a
//// narrow window around the centre of the shower. Then looks at APA 3
//// collection plane and checks to see if there was a continuous amount of
//// energy deposited in the first 50 channels.
//////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <utility>
#include <set>
#include <numeric>

#include "lardataobj/RawData/RawDigit.h"
#include "lardataobj/RawData/RDTimeStamp.h"

#include "art/Framework/Core/EDAnalyzer.h" 
#include "art/Framework/Core/EDFilter.h" 
#include "art/Framework/Core/ModuleMacros.h" 
#include "art/Framework/Principal/Event.h"
#include "canvas/Persistency/Common/FindManyP.h"
#include "art_root_io/TFileService.h"

#include "detdataformats/trigger/TriggerObjectOverlay.hpp"
#include "detdataformats/trigger/TriggerPrimitive.hpp"
#include "detdataformats/trigger/TriggerActivityData.hpp"
#include "detdataformats/trigger/TriggerCandidateData.hpp"

#include "TH1D.h"
#include "TF1.h"
#include "TFitResult.h"

namespace pdhd {

using timestamp_t = dunedaq::trgdataformats::timestamp_t;
using channel_t = dunedaq::trgdataformats::channel_t;
using triggerprimitive_t = dunedaq::trgdataformats::TriggerPrimitive;

//-------------------------------------
class PDHDExtMuonFilter : public art::EDFilter {
  public:
    explicit PDHDExtMuonFilter(fhicl::ParameterSet const & pset);
    virtual ~PDHDExtMuonFilter() {};
    virtual bool filter(art::Event& e);
    void beginJob();

  private:

    int fRun;
    int fSubRun;
    unsigned int fEventID;

    int fAPA_id;
    std::pair<channel_t, channel_t> pCollectionAPA1IDs;
    std::pair<channel_t, channel_t> pCollectionAPA2IDs;
    std::pair<channel_t, channel_t> pCollectionAPA3IDs;
    std::pair<channel_t, channel_t> pCollectionAPA4IDs;

    std::string fInputLabelTA;
    std::string fInputLabelTP;
    channel_t fUpstreamVetoChannels;
    // Name and path of input csv file for SPS beam data
    std::vector<triggerprimitive_t> fTriggerPrimitive;
};

//-------------------------------------
PDHDExtMuonFilter::PDHDExtMuonFilter::PDHDExtMuonFilter(fhicl::ParameterSet const & pset):
  EDFilter(pset), 
  fInputLabelTA(pset.get<std::string>("InputTagTA")),
  fInputLabelTP(pset.get<std::string>("InputTagTP")),
  fUpstreamVetoChannels(pset.get<channel_t>("fUpstreamVetoChannels")) {
  
    fAPA_id = 0;
    pCollectionAPA1IDs = std::make_pair(2080, 2559);
    pCollectionAPA2IDs = std::make_pair(7200, 7680);  
    pCollectionAPA3IDs = std::make_pair(4160, 4639);
    pCollectionAPA4IDs = std::make_pair(9280, 9759); 
  
    consumes<std::vector<triggerprimitive_t>>(fInputLabelTP);
    consumes<std::vector<triggerprimitive_t>>(fInputLabelTA);
    consumes<std::vector<dunedaq::trgdataformats::TriggerActivityData>>(fInputLabelTA);
    consumes<art::Assns<dunedaq::trgdataformats::TriggerActivityData,dunedaq::trgdataformats::TriggerPrimitive>>(fInputLabelTA);
  } 

//-------------------------------------
bool PDHDExtMuonFilter::filter(art::Event & evt) {

  if (!evt.isRealData()) {
    //Filter is designed for Data only. Don't want to filter on MC
    return true;
  }
 
  fRun = evt.run();
  fSubRun = evt.subRun();
  fEventID = evt.id().event();

  std::cout << "###PDHDExtMuonFilter###"<< std::endl
    << "START PDHDExtMuonFilter for Event " << fEventID << " in Run " << fRun << std::endl << std::endl;
  
  // Get TPs across the detector
  auto triggerPrimitiveHandle = evt.getValidHandle<std::vector<triggerprimitive_t>>(fInputLabelTP);
  fTriggerPrimitive = *triggerPrimitiveHandle;
  
  std::cout << "There are " << fTriggerPrimitive.size() << " TPs across the detector." << std::endl;
  
  auto triggerActivityHandle = evt.getValidHandle<std::vector<dunedaq::trgdataformats::TriggerActivityData>>(fInputLabelTA);

  std::cout << "There are " << triggerActivityHandle->size() << " TAs." << std::endl;

  const art::FindManyP<triggerprimitive_t> findTPsInTAs(triggerActivityHandle, evt, fInputLabelTA);
  if ( ! findTPsInTAs.isValid() ) {
    std::cout << " [WARNING] TPs not found in TA." << std::endl;
  }
 
  
  std::vector<timestamp_t> fShowerCentres;
  std::vector<timestamp_t> fShowerUpperBounds;
  std::vector<timestamp_t> fShowerLowerBounds;
  
  for (size_t ta = 0; ta < triggerActivityHandle->size(); ta++) {
    std::cout << "START TA " << ta << " out of " << triggerActivityHandle->size() << std::endl;
    auto fTPs = findTPsInTAs.at(ta);

    std::cout << "Found " << fTPs.size() << " TPs in TA " << ta << std::endl;

    timestamp_t first_tick = triggerActivityHandle->at(ta).time_start;
    timestamp_t last_tick = triggerActivityHandle->at(ta).time_end;
    
    std::sort(fTPs.begin(), fTPs.end(),
        [] (const art::Ptr<triggerprimitive_t> &lh, const art::Ptr<triggerprimitive_t> &rh) -> bool { return lh->channel < rh->channel; });

    channel_t current_chan = fTPs.at(0)->channel;
    
    std::cout << "First tick = " << first_tick << ", last tick = " << last_tick << std::endl;
    std::cout << "First channel = " << current_chan << std::endl;

    if (current_chan >= pCollectionAPA1IDs.first) {
      if (current_chan <= pCollectionAPA1IDs.second) {
        // APA 1, TPC 1
        fAPA_id = 1;
      }
    }
    if (current_chan >= pCollectionAPA3IDs.first) {
      if (current_chan <= pCollectionAPA3IDs.second) {
        // APA 3, TPC 2
        fAPA_id = 3;
      }
    }
    if (current_chan >= pCollectionAPA2IDs.first) {
      if (current_chan <= pCollectionAPA2IDs.second) {
        // APA 2, TPC 5
        fAPA_id = 2;
      }
    }
    if (current_chan >= pCollectionAPA4IDs.first) {
      if (current_chan <= pCollectionAPA4IDs.second) {
        // APA 4, TPC 6
        fAPA_id = 4;
      }
    }

    std::cout << "APA ID = " << fAPA_id << std::endl;
    if (fAPA_id == 3 || fAPA_id == 4) {
      std::cout << "IS IN APA 3 OR 4" << std::endl;
    } else if (fAPA_id == 1 || fAPA_id == 2) {
      std::cout << "IS IN APA 1 OR 2 - don't do anything" << std::endl;
      return true;
    } else {
      std::cout << " Not in any of the main TPCs, removing." << std::endl;
      return false;
    }

    uint32_t adc_integral_sum(0);
    uint32_t tp_mult_sum(0);
    std::vector<uint32_t> v_adc_integral_sum_perchan;
    std::vector<uint32_t> v_tp_mult_sum_perchan;
    for (size_t tp = 0; tp < fTPs.size(); tp++) {
      channel_t new_chan = fTPs.at(tp)->channel;
      if (new_chan != current_chan) {
        current_chan = fTPs.at(tp)->channel;
        v_adc_integral_sum_perchan.push_back(adc_integral_sum);
        v_tp_mult_sum_perchan.push_back(tp_mult_sum);
        adc_integral_sum = 0;
        tp_mult_sum = 0;
      } else if (new_chan == current_chan) {
        adc_integral_sum += fTPs.at(tp)->adc_integral;
        tp_mult_sum++;
      }
    }

    std::cout << "TP multiplicity of " << tp_mult_sum << " reached at channel " << current_chan << std::endl;

    std::cout << "tp_mult_sum vector has size " << v_tp_mult_sum_perchan.size() << std::endl;
    // Get the channel that is definitely in the shower
    uint32_t tp_thresh(200);
    uint32_t tp_counter(0);
    channel_t th_chan = dunedaq::trgdataformats::INVALID_CHANNEL;
    for (size_t ch = 0; ch < v_tp_mult_sum_perchan.size(); ch++) {
      tp_counter += v_tp_mult_sum_perchan.at(ch);
      //std::cout << "channel " << ch << " counting tps: " << tp_counter << std::endl;
      if (tp_counter > tp_thresh) {
        th_chan = fTPs.at(ch)->channel;
        break;
      } else {
        th_chan = fTPs.at(ch)->channel;
      }
    }
    std::cout << "Threshold channel = " << th_chan << std::endl;

    std::vector<timestamp_t> fTPTimeStampsToThresh;
    for (size_t tp = 0; tp < fTPs.size(); tp++) {
      if (fTPs.at(tp)->channel <= th_chan) {
        timestamp_t norm_time = fTPs.at(tp)->time_peak - first_tick;
        fTPTimeStampsToThresh.push_back(norm_time);
      }
    }
    // Calculate average timestamp up to threshold
    timestamp_t sum_time = 0;
    int N(0);
    for (const auto &time : fTPTimeStampsToThresh) {
      sum_time += time;
      N++;
    }
    timestamp_t average_timestamps = sum_time / N;

    std::cout << "time sum = " << sum_time << "; average time = " << average_timestamps << std::endl;

    double shower_stddev = 5000.;
    
    timestamp_t shower_centre = static_cast<timestamp_t>(first_tick + average_timestamps);
    timestamp_t shower_upper_bound = static_cast<timestamp_t>(first_tick + average_timestamps + shower_stddev);
    timestamp_t shower_lower_bound = static_cast<timestamp_t>(first_tick + average_timestamps - shower_stddev);
  
    std::cout << "TA: " << ta << "... Shower centre = " << shower_lower_bound << " < " << shower_centre << " < " << shower_upper_bound << std::endl;
    
    fShowerCentres.push_back(shower_centre);
    fShowerUpperBounds.push_back(shower_upper_bound);
    fShowerLowerBounds.push_back(shower_lower_bound);
  }


  std::vector<triggerprimitive_t> fAPA3TPsInShowerWindow;
  for (const auto &TP : fTriggerPrimitive) {

    channel_t current_chan = TP.channel;

    bool fInAPA3(false);
    if (current_chan >= pCollectionAPA3IDs.first) {
      if (current_chan <= pCollectionAPA3IDs.second) {
        // APA 3, TPC 2
        fInAPA3 = true;
      }
    }
    if (!fInAPA3) continue;
    // Look at all TPs in APA 3 and look for track in small time window
    // Events in with trigger APA 1 or 2 should already have passed filter
    if (TP.time_peak <= fShowerUpperBounds.at(0) && TP.time_peak >= fShowerLowerBounds.at(0)) {
      //std::cout << "TP in APA 3 and time window, channel " << TP.channel << std::endl;
      fAPA3TPsInShowerWindow.push_back(TP);
    }
  }

  std::cout << "fAPA3TPsInShowerWindow size = " << fAPA3TPsInShowerWindow.size() << std::endl;

  // Sort TPs in APA 3 time window in channel order
  std::sort(fAPA3TPsInShowerWindow.begin(), fAPA3TPsInShowerWindow.end(),
      [] (const triggerprimitive_t &lh, const triggerprimitive_t &rh) -> bool { return lh.channel < rh.channel; });

  int number_hits_window(0);

  channel_t start_chan = pCollectionAPA3IDs.first;
  // Look at first 40 channels
  channel_t end_chan = pCollectionAPA3IDs.first + fUpstreamVetoChannels;
  channel_t veto_threshold = 0.9 * fUpstreamVetoChannels;

  for (size_t tp_it = 0; tp_it < fAPA3TPsInShowerWindow.size(); tp_it++){
    if (fAPA3TPsInShowerWindow.at(tp_it).channel >= start_chan && fAPA3TPsInShowerWindow.at(tp_it).channel <= end_chan) {
      number_hits_window++;
    }
  }

  bool filter_pass(true);
  // Fail filter if more than 90% of channels in the first fUpstreamVetoChannels on APA 3 collection plane have TP hits
  if (number_hits_window >= static_cast<int>(veto_threshold)) {
    std::cout << "There are " << number_hits_window << " hits in first " << fUpstreamVetoChannels << " APA 3 collection plane channels so remove." << std::endl;
    filter_pass = false;
  } else {
    std::cout << "There are " << number_hits_window << " hits in first " << fUpstreamVetoChannels <<  " collection plane. No external muon so pass filter!" << std::endl;
  }

  std::cout << "END PDHDExtMuonFilter for Event " << fEventID << " in Run " << fRun << std::endl << std::endl;
  
  return filter_pass;
}

//-------------------------------------
void PDHDExtMuonFilter::beginJob() {}

DEFINE_ART_MODULE(PDHDExtMuonFilter)

}
