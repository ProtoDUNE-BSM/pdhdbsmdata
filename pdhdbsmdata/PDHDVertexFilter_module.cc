////////////////////////////////////////////////////////////////////////
//// Class:       PDHDVertexFilter
//// Plugin Type: filter (Unknown Unknown)
//// File:        PDHDVertexFilter_module.cc
//// Author:      Ciaran Hasnip (CERN)
//// 
//// Filter that finds the centre of the energy deposition shower in the
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
#include "TH2D.h"
#include "TGraph.h"
#include "TF1.h"
#include "TFitResult.h"

namespace pdhd {

using timestamp_t = dunedaq::trgdataformats::timestamp_t;
using channel_t = dunedaq::trgdataformats::channel_t;
using triggerprimitive_t = dunedaq::trgdataformats::TriggerPrimitive;

//-------------------------------------
class PDHDVertexFilter : public art::EDFilter {
  public:
    explicit PDHDVertexFilter(fhicl::ParameterSet const & pset);
    virtual ~PDHDVertexFilter() {};
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

    std::vector<TH2D*> fAPA1TAHIST;
    std::vector<TH2D*> fAPA2TAHIST;
    std::vector<TH2D*> fAPA3TAHIST;
    std::vector<TH2D*> fAPA4TAHIST;

    std::vector<TH1D*> fAPA1_TimeProjHIST;
    std::vector<TH1D*> fAPA2_TimeProjHIST;
    std::vector<TH1D*> fAPA3_TimeProjHIST;
    std::vector<TH1D*> fAPA4_TimeProjHIST;
    
    std::vector<TH1D*> fAPA1_ChanProjHIST;
    std::vector<TH1D*> fAPA2_ChanProjHIST;
    std::vector<TH1D*> fAPA3_ChanProjHIST;
    std::vector<TH1D*> fAPA4_ChanProjHIST;
    
    std::vector<TH2D*> fAPA3WindowHIST;
    
    std::string fInputLabelTA;
    std::string fInputLabelTP;
    channel_t fUpstreamVetoChannels;
    // Name and path of input csv file for SPS beam data
    std::vector<triggerprimitive_t> fTriggerPrimitive;
};

//-------------------------------------
PDHDVertexFilter::PDHDVertexFilter::PDHDVertexFilter(fhicl::ParameterSet const & pset) :
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
void PDHDVertexFilter::beginJob() {}

//-------------------------------------
bool PDHDVertexFilter::filter(art::Event & evt) {

  art::ServiceHandle<art::TFileService> tfs;
  
  fRun = evt.run();
  fSubRun = evt.subRun();
  fEventID = evt.id().event();

  std::cout << "###PDHDVertexFilter###"<< std::endl
    << "START PDHDVertexFilter for Event " << fEventID << " in Run " << fRun << std::endl << std::endl;
  
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

  // Boolean to return - if any one of the TAs passes the filters, pass the whole event
  bool fEventPassesFilters(true);

  for (size_t ta = 0; ta < triggerActivityHandle->size(); ta++) {
    std::cout << "START TA " << ta << " out of " << triggerActivityHandle->size() << std::endl;
    auto fTPs = findTPsInTAs.at(ta);

    std::cout << "Found " << fTPs.size() << " TPs in TA " << ta << std::endl;

    timestamp_t first_tick = triggerActivityHandle->at(ta).time_start;
    timestamp_t last_tick = triggerActivityHandle->at(ta).time_end;
  
    timestamp_t TAWindow = last_tick - first_tick;

    std::cout << ">>> TAWindow = " << TAWindow << std::endl;
    // Now sort in channel number
    std::sort(fTPs.begin(), fTPs.end(),
        [] (const art::Ptr<triggerprimitive_t> &lh, const art::Ptr<triggerprimitive_t> &rh) -> bool { return lh->channel < rh->channel; });

    channel_t current_chan = fTPs.at(0)->channel;
    
    std::cout << "First tick = " << first_tick << ", last tick = " << last_tick << std::endl;
    std::cout << "First channel = " << current_chan << std::endl;

    std::string title("");

    if (current_chan >= pCollectionAPA1IDs.first) {
      if (current_chan <= pCollectionAPA1IDs.second) {
        // APA 1, TPC 1
        fAPA_id = 1;
        title = "APA" + std::to_string(fAPA_id) + "_TATPs_ev" + std::to_string(fEventID) + "_run" + std::to_string(fRun) + "_ta" + std::to_string(ta);
        fAPA1TAHIST.emplace_back(tfs->make<TH2D>(title.c_str(), ";Channel Number;Time (ticks)", 25, pCollectionAPA1IDs.first, pCollectionAPA1IDs.second, 20, 0, TAWindow));
      }
    }
    if (current_chan >= pCollectionAPA3IDs.first) {
      if (current_chan <= pCollectionAPA3IDs.second) {
        // APA 3, TPC 2
        fAPA_id = 3;
        title = "APA" + std::to_string(fAPA_id) + "_TATPs_ev" + std::to_string(fEventID) + "_run" + std::to_string(fRun) + "_ta" + std::to_string(ta);
        fAPA3TAHIST.emplace_back(tfs->make<TH2D>(title.c_str(), ";Channel Number;Time (ticks)", 25, pCollectionAPA3IDs.first, pCollectionAPA3IDs.second, 20, 0, TAWindow));
      }
    }
    if (current_chan >= pCollectionAPA2IDs.first) {
      if (current_chan <= pCollectionAPA2IDs.second) {
        // APA 2, TPC 5
        fAPA_id = 2;
        title = "APA" + std::to_string(fAPA_id) + "_TATPs_ev" + std::to_string(fEventID) + "_run" + std::to_string(fRun) + "_ta" + std::to_string(ta);
        fAPA2TAHIST.emplace_back(tfs->make<TH2D>(title.c_str(), ";Channel Number;Time (ticks)", 25, pCollectionAPA2IDs.first, pCollectionAPA2IDs.second, 20, 0, TAWindow));
      }
    }
    if (current_chan >= pCollectionAPA4IDs.first) {
      if (current_chan <= pCollectionAPA4IDs.second) {
        // APA 4, TPC 6
        fAPA_id = 4;
        title = "APA" + std::to_string(fAPA_id) + "_TATPs_ev" + std::to_string(fEventID) + "_run" + std::to_string(fRun) + "_ta" + std::to_string(ta);
        fAPA4TAHIST.emplace_back(tfs->make<TH2D>(title.c_str(), ";Channel Number;Time (ticks)", 25, pCollectionAPA4IDs.first, pCollectionAPA4IDs.second, 20, 0, TAWindow));
      }
    }

    std::cout << "APA ID = " << fAPA_id << std::endl;
    if (fAPA_id == 0) {
      std::cout << "APA ID not set to one of the main volumes so remove." << std::endl;
      return false;
    }

    for (size_t tp = 0; tp < fTPs.size(); tp++) {
      timestamp_t filltime = fTPs.at(tp)->time_start - first_tick;
      if (fAPA_id == 1) {
        fAPA1TAHIST.back()->Fill(static_cast<double>(fTPs.at(tp)->channel), static_cast<double>(filltime), static_cast<double>(fTPs.at(tp)->adc_integral));       
      } else if (fAPA_id == 2) {
        fAPA2TAHIST.back()->Fill(static_cast<double>(fTPs.at(tp)->channel), static_cast<double>(filltime), static_cast<double>(fTPs.at(tp)->adc_integral));       
      } else if (fAPA_id == 3) {
        fAPA3TAHIST.back()->Fill(static_cast<double>(fTPs.at(tp)->channel), static_cast<double>(filltime), static_cast<double>(fTPs.at(tp)->adc_integral));       
      } else if (fAPA_id == 4) {
        fAPA4TAHIST.back()->Fill(static_cast<double>(fTPs.at(tp)->channel), static_cast<double>(filltime), static_cast<double>(fTPs.at(tp)->adc_integral));       
      }
    }
     
    std::string title_timeproj = title + "_projTime";
    if (fAPA_id == 1) {
      TH1D *hTimeProj = fAPA1TAHIST.back()->ProjectionY(title_timeproj.c_str());
      fAPA1_TimeProjHIST.emplace_back(tfs->make<TH1D>(*hTimeProj));
      delete hTimeProj;
    } else if (fAPA_id == 2) {
      TH1D *hTimeProj = fAPA2TAHIST.back()->ProjectionY(title_timeproj.c_str());
      fAPA2_TimeProjHIST.emplace_back(tfs->make<TH1D>(*hTimeProj));
      delete hTimeProj;
    } else if (fAPA_id == 3) {
      TH1D *hTimeProj = fAPA3TAHIST.back()->ProjectionY(title_timeproj.c_str());
      fAPA3_TimeProjHIST.emplace_back(tfs->make<TH1D>(*hTimeProj));
      delete hTimeProj;
    } else if (fAPA_id == 4) {
      TH1D *hTimeProj = fAPA4TAHIST.back()->ProjectionY(title_timeproj.c_str());
      fAPA4_TimeProjHIST.emplace_back(tfs->make<TH1D>(*hTimeProj));
      delete hTimeProj;
    }

    TH2D *hAPAXTAHIST;
    TH1D *hAPAXTimeProj;

    if (fAPA_id == 1) {
      hAPAXTAHIST = fAPA1TAHIST.back();
      hAPAXTimeProj = fAPA1_TimeProjHIST.back();
    } else if (fAPA_id == 2) {
      hAPAXTAHIST = fAPA2TAHIST.back();
      hAPAXTimeProj = fAPA2_TimeProjHIST.back();
    } else if (fAPA_id == 3) {
      hAPAXTAHIST = fAPA3TAHIST.back();
      hAPAXTimeProj = fAPA3_TimeProjHIST.back();
    } else { // APA 4
      hAPAXTAHIST = fAPA4TAHIST.back();
      hAPAXTimeProj = fAPA4_TimeProjHIST.back();
    }

    if (!hAPAXTAHIST || !hAPAXTimeProj) {
      std::cerr << "[ERROR] Null histogram." << std::endl;
      fEventPassesFilters = false;
      continue;
      //return false;
    }

    // >>> Shower spread in time filter
    // Find the shower region in time using the time TP projections
    // Get the bin with the maximum content (approximate peak position)
    int peakBin = hAPAXTimeProj->GetMaximumBin();
    double peakValue = hAPAXTimeProj->GetBinCenter(peakBin);
    double peakHeight = hAPAXTimeProj->GetBinContent(peakBin);

    if (peakBin == hAPAXTimeProj->GetXaxis()->GetNbins()) {
      std::cout << "[WARNING] Peak in time at the edge, gaussian likely to be a bad fit." << std::endl;
    }

    // Fit a gaussian around peak region
    // Define a fitting range around the peak
    double fitRangeMin = peakValue - 5000;
    double fitRangeMax = peakValue + 5000;

    // Get range in TA bounds
    fitRangeMin = std::max(fitRangeMin, hAPAXTimeProj->GetXaxis()->GetXmin());
    fitRangeMax = std::min(fitRangeMax, hAPAXTimeProj->GetXaxis()->GetXmax());

    // Define a Gaussian function for fitting
    TF1* timeFit = new TF1("gaussFit", "gaus", fitRangeMin, fitRangeMax);
    // Set initial parameter guesses
    timeFit->SetParameters(peakHeight, peakValue, 1000); // amplitude, mean, sigma

    // Do fit
    TFitResultPtr r = hAPAXTimeProj->Fit(timeFit, "RS");

    double mean_time = r->Parameter(1);
    double sigma_time = r->Parameter(2);
    std::cout << "Time: Mean = " << mean_time << ", sigma = " << sigma_time << std::endl;
    
    if (sigma_time > 4000) {
      std::cout << "Too broad in time. Remove." << std::endl;
      fEventPassesFilters = false;
      continue;
      //return false;
    }

    Int_t timeFitStatus = r;
    if (timeFitStatus != 0) {
      std::cout << "[WARNING] Bad fit status " << timeFitStatus << ", so removing." << std::endl;
      fEventPassesFilters = false;
      continue;
      //return false;
    }
    // >>> Shower spread filter end
    
    TH1D *hAPAXChanProj;
    int timeRangeMinBin = hAPAXTimeProj->FindFixBin(fitRangeMin);
    int timeRangeMaxBin = hAPAXTimeProj->FindFixBin(fitRangeMax);
    std::string title_chanproj = title + "_projChan";

    if (fAPA_id == 1) {
      TH1D *hChanProj = fAPA1TAHIST.back()->ProjectionX(title_chanproj.c_str(), timeRangeMinBin, timeRangeMaxBin);
      fAPA1_ChanProjHIST.emplace_back(tfs->make<TH1D>(*hChanProj));
      delete hChanProj;
      hAPAXChanProj = fAPA1_ChanProjHIST.back();
    } else if (fAPA_id == 2) {
      TH1D *hChanProj = fAPA2TAHIST.back()->ProjectionX(title_chanproj.c_str(), timeRangeMinBin, timeRangeMaxBin);
      fAPA2_ChanProjHIST.emplace_back(tfs->make<TH1D>(*hChanProj));
      delete hChanProj;
      hAPAXChanProj = fAPA2_ChanProjHIST.back();
    } else if (fAPA_id == 3) {
      TH1D *hChanProj = fAPA3TAHIST.back()->ProjectionX(title_chanproj.c_str(), timeRangeMinBin, timeRangeMaxBin);
      fAPA3_ChanProjHIST.emplace_back(tfs->make<TH1D>(*hChanProj));
      delete hChanProj;
      hAPAXChanProj = fAPA3_ChanProjHIST.back();
    } else { // Must be APA 4
      TH1D *hChanProj = fAPA4TAHIST.back()->ProjectionX(title_chanproj.c_str(), timeRangeMinBin, timeRangeMaxBin);
      fAPA4_ChanProjHIST.emplace_back(tfs->make<TH1D>(*hChanProj));
      delete hChanProj;
      hAPAXChanProj = fAPA4_ChanProjHIST.back();
    }

    // >>> Channel search for vertex start
    double minimum_region_bin(1e9);
    for (int ch = 2; ch <= hAPAXChanProj->GetXaxis()->GetNbins() - 1; ch++) {
      double region_amp = hAPAXChanProj->GetBinContent(ch-1) + hAPAXChanProj->GetBinContent(ch) + hAPAXChanProj->GetBinContent(ch+1);
      if (region_amp < minimum_region_bin) {
        minimum_region_bin = region_amp;
      }
    }

    double maximum_region_bin(0);
    for (int ch = 2; ch <= hAPAXChanProj->GetXaxis()->GetNbins() - 1; ch++) {
      double region_amp = hAPAXChanProj->GetBinContent(ch-1) + hAPAXChanProj->GetBinContent(ch) + hAPAXChanProj->GetBinContent(ch+1);
      if (region_amp > maximum_region_bin) {
        maximum_region_bin = region_amp;
      }
    }

    // I think I only want a vertex cut in APA 3... too risky otherwise
    if (fAPA_id == 3) {
      // Minimum region is to the left - vertex in TPC volume
      if (minimum_region_bin < maximum_region_bin) {
        // Find max difference between adjacent bins between min and max region
        double max_diff(0);
        double old_diff(0);
        int max_diff_bin = minimum_region_bin;
        for (int ch = minimum_region_bin; ch  <= maximum_region_bin; ch++) {
          double diff = hAPAXChanProj->GetBinContent(ch) - hAPAXChanProj->GetBinContent(ch-1);
          if (old_diff < 0) {
            old_diff = diff;   
            continue;
          }
          old_diff = diff;
          if (diff > max_diff) {
            max_diff = diff;
            max_diff_bin = ch;
          }
        }
        // Assume that start of the shower (vertex) is 2 bins back from max difference bin
        int vertex_chan_bin = max_diff_bin - 2;
        double vertex_chan = hAPAXChanProj->GetBinCenter(vertex_chan_bin);
        std::cout << ">>> Vertex approx. at channel: " << vertex_chan << std::endl;
        if (vertex_chan < (pCollectionAPA3IDs.first + 40)) {
          std::cout << "Vertex likely to be within the first 40 channels of APA3 collection plane. Remove." << std::endl;
          return false;
        }
      } else {
        double first_minimum_region_bin(1e9);
        // Count back from the maximum region to find the minimum point to the left of the max region
        for (int ch = maximum_region_bin; ch >= 1; ch--) {
          double region_amp = hAPAXChanProj->GetBinContent(ch-1) + hAPAXChanProj->GetBinContent(ch) + hAPAXChanProj->GetBinContent(ch+1);
          if (region_amp < first_minimum_region_bin) {
            first_minimum_region_bin = region_amp;
          }
        }

        // If the minimum region to the left of the max region is more than half the height of the max region then remove event
        if (hAPAXChanProj->GetBinContent(first_minimum_region_bin) > 0.5*hAPAXChanProj->GetBinContent(maximum_region_bin)) {
          std::cout << "First minimum region has too many hits - shower likely entering from outside. Remove." << std::endl;
          fEventPassesFilters = false;
          continue;
          //return false;
        }

        // So the minimum to the left of the max region is small enough
        // Now repeat the vertex channel finding from above
        double max_diff(0);
        double old_diff(0);
        int max_diff_bin = minimum_region_bin;
        for (int ch = minimum_region_bin; ch  <= maximum_region_bin; ch++) {
          double diff = hAPAXChanProj->GetBinContent(ch) - hAPAXChanProj->GetBinContent(ch-1);
          if (old_diff < 0) {
            old_diff = diff;   
            continue;
          }
          old_diff = diff;
          if (diff > max_diff) {
            max_diff = diff;
            max_diff_bin = ch;
          }
        }
        // Assume that start of the shower (vertex) is 2 bins back from max difference bin
        int vertex_chan_bin = max_diff_bin - 2;
        double vertex_chan = hAPAXChanProj->GetBinCenter(vertex_chan_bin);
        std::cout << ">>> Vertex approx. at channel: " << vertex_chan << std::endl;
        if (vertex_chan < (pCollectionAPA3IDs.first + 40)) {
          std::cout << "Vertex likely to be within the first 40 channels of APA3 collection plane or outside the detector. Remove." << std::endl;
          fEventPassesFilters = false;
          continue;
          //return false;
        }
      }
    }
    // >>> Channel search for vertex end
    
    // >>> External muon filter start
    // This cut only works for APA 3/4 because of the broken collection plane on APA1
    if (fAPA_id == 3 || fAPA_id == 4) { 
      // Get narrow region in time around shower peak
      timestamp_t fShowerCenter = static_cast<timestamp_t>(mean_time);
      timestamp_t fShowerUpperBound = fShowerCenter + 0.5*static_cast<timestamp_t>(sigma_time);
      timestamp_t fShowerLowerBound = fShowerCenter - 0.5*static_cast<timestamp_t>(sigma_time);
    
      std::string title_apa3window = title + "_APA3Window";
      fAPA3WindowHIST.emplace_back(tfs->make<TH2D>(title_apa3window.c_str(), ";Channel Number;Time (ticks)", 50, pCollectionAPA3IDs.first, pCollectionAPA3IDs.second, 40, fShowerLowerBound, fShowerUpperBound));

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
        timestamp_t filltime = TP.time_peak - first_tick;
        if (filltime <= fShowerUpperBound && filltime >= fShowerLowerBound) {
          fAPA3TPsInShowerWindow.push_back(TP);
          // Just want to see the number of hits in this histogram
          fAPA3WindowHIST.back()->Fill(static_cast<double>(TP.channel), static_cast<double>(filltime));
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
      // Fail filter if more than 90% of channels in the first fUpstreamVetoChannels on APA 3 collection plane have TP hits
      if (number_hits_window >= static_cast<int>(veto_threshold)) {
        std::cout << "There are " << number_hits_window << " hits in first " << fUpstreamVetoChannels << " APA 3 collection plane channels so remove." << std::endl;
        fEventPassesFilters = false;
        continue;
        //return false;
      } else {
        std::cout << "There are " << number_hits_window << " hits in first " << fUpstreamVetoChannels <<  " collection plane. No external muon so pass filter!" << std::endl;
      }
      // >>> External muon filter end

    }
    // Found at least 1 TA that passed these filters, so pass the whole event on to reconstruction
    fEventPassesFilters = true;
    break;
  }
      
  std::cout << "END PDHDVertexFilter for Event " << fEventID << " in Run " << fRun << std::endl << std::endl;

  return fEventPassesFilters;
}

DEFINE_ART_MODULE(PDHDVertexFilter)

}
