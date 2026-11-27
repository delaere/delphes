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

#ifndef DelphesEventDisplay7_h
#define DelphesEventDisplay7_h

#include <vector>

#include "Rtypes.h"

#include <ROOT/REveElement.hxx>

class TAxis;
class TCanvas;
class TChain;
class TH2F;
class THStack;
class Delphes3DGeometry;
class DelphesBranchBase7;
class DelphesCaloData7;
class DelphesParticleTable7;
class DelphesSummary7;
class DelphesEventDisplay7;
class ExRootTreeReader;

namespace ROOT {
namespace Experimental {
class REveElement;
class REveManager;
class REveProjectionManager;
class REveScene;
class REveViewer;
} // namespace Experimental
} // namespace ROOT

/** \class DelphesEventNavigator7
 *
 *  Element living in the EVE world whose methods are bound to the
 *  buttons of the web interface (commands are executed under an
 *  REveManager change guard by the EVE7 server).
 */

class DelphesEventNavigator7: public ROOT::Experimental::REveElement
{
public:
  DelphesEventNavigator7(DelphesEventDisplay7 *display = nullptr);
  ~DelphesEventNavigator7() override {}

  void NextEvent();
  void PreviousEvent();
  void FirstEvent();
  void LastEvent();
  void ShowSummary();
  void QuitRoot();

private:
  DelphesEventDisplay7 *display_;
};

/** \class DelphesEventDisplay7
 *
 *  Delphes event display based on the web-based ROOT EVE7 framework.
 *
 *  Usage (see examples/EventDisplay7.C):
 *    Delphes3DGeometry det3D(new TGeoManager("delphes", "Delphes geometry"), true);
 *    det3D.readFile(card, ...);
 *    auto display = new DelphesEventDisplay7(card, "delphes_output.root", det3D);
 *    display->GotoEvent(42); // from the ROOT prompt
 */

class DelphesEventDisplay7
{
public:
  DelphesEventDisplay7(const char *configFile, const char *inputFile, Delphes3DGeometry &det3D);
  ~DelphesEventDisplay7();

  // navigation from the ROOT prompt (takes the EVE change guard)
  void GotoEvent(Long64_t event);
  void NextEvent();
  void PreviousEvent();

  Long64_t GetCurrentEvent() const { return event_id_; }
  Long64_t GetEntries() const;

  // navigation from the web interface (the caller holds the change guard)
  void LoadEvent(Long64_t event);

  // summary page: distributions of the event and of the sample
  void ShowSummary(); // opens it (from any thread)
  void FillSummary(); // reads the whole sample now (main thread)

private:
  void readConfig(const char *configFile);
  void buildGeometry(Delphes3DGeometry &det3D);
  void buildViews();
  void buildLego(DelphesCaloData7 *calo);
  void loadEvent();
  void projectEvent();
  void fillLego();
  void warmUpClasses();

  // configuration
  Long64_t event_id_;
  Double_t tkRadius_, totRadius_, tkHalfLength_, muHalfLength_, bz_;
  TAxis *etaAxis_, *phiAxis_;

  // input
  TChain *chain_;
  ExRootTreeReader *treeReader_;
  std::vector<DelphesBranchBase7 *> elements_;
  DelphesBranchBase7 *caloBranch_;

  // EVE7 objects (owned by the EVE manager)
  ROOT::Experimental::REveManager *eveMng_;
  DelphesEventNavigator7 *navigator_;
  ROOT::Experimental::REveProjectionManager *rphiMgr_;
  ROOT::Experimental::REveProjectionManager *rhozMgr_;
  ROOT::Experimental::REveScene *rphiGeomScene_, *rphiEventScene_;
  ROOT::Experimental::REveScene *rhozGeomScene_, *rhozEventScene_;
  ROOT::Experimental::REveScene *legoScene_;
  ROOT::Experimental::REveElement *legoHolder_;

  // lego plot, drawn by JSROOT in the web interface
  TCanvas *legoCanvas_;
  TH2F *legoEcal_, *legoHcal_;
  THStack *legoStack_;

  DelphesParticleTable7 *table_;
  DelphesSummary7 *summary_;
};

#endif // DelphesEventDisplay7_h
