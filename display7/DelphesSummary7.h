/*
 *  Delphes: a framework for fast simulation of a generic collider experiment
 *  Copyright (C) 2012-2014  Universite catholique de Louvain (UCL), Belgium
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DelphesSummary7_h
#define DelphesSummary7_h

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Rtypes.h"
#include "TLorentzVector.h"

class TCanvas;
class TChain;
class TClonesArray;
class TH1D;
class TTimer;
class DelphesBranchBase7;
class ExRootTreeReader;

namespace ROOT {
class RWebWindow;
}

/** \class DelphesSummary7
 *
 *  Summary page of the EVE7 display, in its own web window: for every
 *  displayed branch, the distributions of the current event (multiplicity,
 *  pT, eta, phi) compared to the whole sample (all, leading and subleading
 *  objects, per event). The page also has event navigation.
 *
 *  The page is served and updated from the main thread (ROOT timer); the
 *  current event can be set from any thread.
 */

class DelphesSummary7
{
public:
  DelphesSummary7(const char *inputFile, const std::vector<DelphesBranchBase7 *> &branches);
  ~DelphesSummary7();

  // record the objects of the event just loaded in the branches (any thread)
  void SetEvent(Long64_t event, Long64_t entries);

  // open the page (main thread), or ask for it from any thread
  void Show();
  void RequestShow() { showRequested_ = true; }

  // called when the page asks for another event (main thread)
  void SetNavigation(std::function<void(Long64_t)> gotoEvent) { gotoEvent_ = gotoEvent; }

  // fill the sample distributions now (main thread, blocking)
  void FillSample();

private:
  struct Histograms
  {
    std::vector<Long64_t> multiplicity; // number of events with n objects
    TH1D *all[3], *leading[3], *subleading[3]; // pT, eta, phi
  };
  struct Collection
  {
    std::string name, type;
    Color_t color;
    DelphesBranchBase7 *branch;
    TClonesArray *sampleBranch;
    Histograms sample;
    std::vector<TLorentzVector> event; // sorted by decreasing pT
  };

  friend class DelphesSummary7Timer;
  void poll();
  void processMessage(unsigned connid, const std::string &message);
  void fillSampleChunk(double seconds);
  void sendInit(unsigned connid);
  void sendEvent(unsigned connid);
  void sendPlot(unsigned connid);
  void sendProgress();
  std::string eventJson();
  std::string plotsJson(const Collection &collection);

  std::vector<Collection> collections_;
  std::shared_ptr<ROOT::RWebWindow> window_;
  TTimer *timer_;
  std::map<unsigned, std::string> selected_; // connection -> displayed collection
  std::function<void(Long64_t)> gotoEvent_;

  // current event (written from any thread)
  std::mutex eventMutex_;
  Long64_t event_, entries_;
  std::atomic<bool> eventChanged_, showRequested_;

  // sample, read with its own reader
  std::string inputFile_;
  TChain *chain_;
  ExRootTreeReader *reader_;
  Long64_t sampleNext_, sampleEvents_;
  bool sampleRunning_;
  double lastProgress_;
};

#endif // DelphesSummary7_h
