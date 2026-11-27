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

#include <functional>
#include <iostream>
#include <memory>
#include <typeinfo>
#include <vector>

#include "TAxis.h"
#include "TBase64.h"
#include "TBufferJSON.h"
#include "TCanvas.h"
#include "TChain.h"
#include "TClass.h"
#include "TClonesArray.h"
#include "TGeoManager.h"
#include "TGeoNode.h"
#include "TGeoVolume.h"
#include "TH2F.h"
#include "THStack.h"

#include <ROOT/REveCalo.hxx>
#include <ROOT/REveCompound.hxx>
#include <ROOT/REveGeoShape.hxx>
#include <ROOT/REveJetCone.hxx>
#include <ROOT/REveManager.hxx>
#include <ROOT/REveProjectionManager.hxx>
#include <ROOT/REveScene.hxx>
#include <ROOT/REveStraightLineSet.hxx>
#include <ROOT/REveTrackProjected.hxx>
#include <ROOT/REveTrans.hxx>
#include <ROOT/REveViewer.hxx>

#include "display/Delphes3DGeometry.h"
#include "display7/DelphesBranchElement7.h"
#include "display7/DelphesCaloData7.h"
#include "display7/DelphesEventDisplay7.h"

#include "external/ExRootAnalysis/ExRootConfReader.h"
#include "external/ExRootAnalysis/ExRootTreeReader.h"

using namespace ROOT::Experimental;

//------------------------------------------------------------------------------
// DelphesEventNavigator7
//------------------------------------------------------------------------------

DelphesEventNavigator7::DelphesEventNavigator7(DelphesEventDisplay7 *display) :
  REveElement("Event"), display_(display)
{
}

void DelphesEventNavigator7::NextEvent()
{
  display_->LoadEvent(display_->GetCurrentEvent() + 1);
}

void DelphesEventNavigator7::PreviousEvent()
{
  display_->LoadEvent(display_->GetCurrentEvent() - 1);
}

void DelphesEventNavigator7::FirstEvent()
{
  display_->LoadEvent(0);
}

void DelphesEventNavigator7::LastEvent()
{
  display_->LoadEvent(display_->GetEntries() - 1);
}

void DelphesEventNavigator7::QuitRoot()
{
  REveManager::QuitRoot();
}

//------------------------------------------------------------------------------
// DelphesEventDisplay7
//------------------------------------------------------------------------------

namespace
{
// bin edges of an axis, for both fixed and variable binnings
std::vector<Double_t> binEdges(const TAxis *axis)
{
  std::vector<Double_t> edges;
  for(Int_t i = 1; i <= axis->GetNbins() + 1; ++i) edges.push_back(axis->GetBinLowEdge(i));
  return edges;
}
} // namespace

DelphesEventDisplay7::DelphesEventDisplay7(const char *configFile, const char *inputFile, Delphes3DGeometry &det3D) :
  event_id_(0),
  tkRadius_(det3D.getTrackerRadius()),
  totRadius_(det3D.getDetectorRadius()),
  tkHalfLength_(det3D.getTrackerHalfLength()),
  muHalfLength_(det3D.getDetectorHalfLength()),
  bz_(det3D.getBField()),
  etaAxis_(det3D.getCaloAxes().first),
  phiAxis_(det3D.getCaloAxes().second),
  chain_(new TChain("Delphes")),
  treeReader_(nullptr),
  caloBranch_(nullptr),
  eveMng_(nullptr),
  navigator_(nullptr),
  rphiMgr_(nullptr),
  rhozMgr_(nullptr),
  rphiGeomScene_(nullptr),
  rphiEventScene_(nullptr),
  rhozGeomScene_(nullptr),
  rhozEventScene_(nullptr),
  legoScene_(nullptr),
  legoHolder_(nullptr),
  legoCanvas_(nullptr),
  legoEcal_(nullptr),
  legoHcal_(nullptr),
  legoStack_(nullptr)
{
  // initialize the application
  eveMng_ = REveManager::Create();

  // open the input file
  std::cout << "** Opening Delphes data file " << inputFile << std::endl;
  chain_->Add(inputFile);
  treeReader_ = new ExRootTreeReader(chain_);

  // the event navigation commands, shown as buttons in the web interface
  navigator_ = new DelphesEventNavigator7(this);
  eveMng_->GetWorld()->AddElement(navigator_);
  eveMng_->GetWorld()->AddCommand("Previous event", "sap-icon://media-rewind", navigator_, "PreviousEvent()");
  eveMng_->GetWorld()->AddCommand("Next event", "sap-icon://media-forward", navigator_, "NextEvent()");
  eveMng_->GetWorld()->AddCommand("First event", "sap-icon://close-command-field", navigator_, "FirstEvent()");
  eveMng_->GetWorld()->AddCommand("Last event", "sap-icon://open-command-field", navigator_, "LastEvent()");
  eveMng_->GetWorld()->AddCommand("Quit", "sap-icon://log", navigator_, "QuitRoot()");

  // the detector
  buildGeometry(det3D);

  // prepare data collections and add them to the event scene
  readConfig(configFile);
  for(auto *element : elements_)
  {
    eveMng_->GetEventScene()->AddElement(element->GetElement());
  }

  // the views: 3D (default viewer), R-Phi, Rho-Z and Lego
  buildViews();

  // the calorimeter: 3D towers in the event scene, projected with the rest of the event
  if(caloBranch_)
  {
    DelphesCaloData7 *calo = static_cast<DelphesBranchElement7<DelphesCaloData7> *>(caloBranch_)->GetContainer();
    auto *calo3d = new REveCalo3D(calo, "Calorimeter towers");
    calo3d->SetBarrelRadius(tkRadius_);
    calo3d->SetEndCapPos(tkHalfLength_);
    eveMng_->GetEventScene()->AddElement(calo3d);
    buildLego(calo);
  }

  // first event
  loadEvent();

  // The web server threads stream the scenes and need the TClass of each
  // element. Building a TClass can invoke the interpreter, which crashes in
  // these threads (small stacks): build them all now, in the main thread.
  warmUpClasses();

  // start the web server and open the browser
  eveMng_->Show();
}

//------------------------------------------------------------------------------

void DelphesEventDisplay7::warmUpClasses()
{
  // types that might not be present in the first event
  for(const std::type_info *type : {&typeid(DelphesEventNavigator7), &typeid(DelphesCaloData7),
        &typeid(REveCompound), &typeid(REveCompoundProjected),
        &typeid(REveJetCone), &typeid(REveJetConeProjected),
        &typeid(REveStraightLineSet), &typeid(REveStraightLineSetProjected),
        &typeid(REveTrackList), &typeid(REveTrackListProjected),
        &typeid(REveTrack), &typeid(REveTrackProjected),
        &typeid(REveCalo3D), &typeid(REveCalo2D),
        &typeid(REveGeoShape), &typeid(REveGeoShapeProjected)})
  {
    TClass::GetClass(*type);
  }

  // everything that is already in the scenes
  std::function<void(REveElement *)> warmUp = [&warmUp](REveElement *el) {
    el->IsA();
    for(auto *child : el->RefChildren()) warmUp(child);
  };
  warmUp(eveMng_->GetWorld());
  warmUp(eveMng_->GetScenes());
  warmUp(eveMng_->GetViewers());
}

//------------------------------------------------------------------------------

DelphesEventDisplay7::~DelphesEventDisplay7()
{
  // the EVE elements are owned by the EVE manager
  for(auto *element : elements_) delete element;
  delete treeReader_;
  delete chain_;
}

//------------------------------------------------------------------------------

Long64_t DelphesEventDisplay7::GetEntries() const
{
  return treeReader_->GetEntries();
}

//------------------------------------------------------------------------------

void DelphesEventDisplay7::buildGeometry(Delphes3DGeometry &det3D)
{
  // The detector is made of a few simple volumes placed in the top volume.
  // Each of them becomes an REveGeoShape, which can be projected.
  TGeoVolume *top = det3D.getDetector(false);
  gGeoManager->SetTopVolume(top);

  auto *geometry = new REveElement("Geometry");
  TIter itNodes(top->GetNodes());
  TGeoNode *node;
  while((node = (TGeoNode *)itNodes.Next()))
  {
    TGeoVolume *volume = node->GetVolume();
    auto *shape = new REveGeoShape(node->GetName());
    shape->SetShape(volume->GetShape());
    shape->InitMainTrans();
    shape->RefMainTrans().SetFrom(*node->GetMatrix());
    shape->SetMainColor(volume->GetLineColor());
    shape->SetMainTransparency(volume->GetTransparency());
    shape->SetPickable(kTRUE);
    geometry->AddElement(shape);
  }
  eveMng_->GetGlobalScene()->AddElement(geometry);
}

//------------------------------------------------------------------------------

void DelphesEventDisplay7::buildViews()
{
  // R-Phi projection
  rphiGeomScene_ = eveMng_->SpawnNewScene("RPhi Geometry", "RPhi");
  rphiEventScene_ = eveMng_->SpawnNewScene("RPhi Event Data", "RPhi");
  rphiMgr_ = new REveProjectionManager(REveProjection::kPT_RPhi);
  auto *rphiView = eveMng_->SpawnNewViewer("RPhi View", "");
  rphiView->SetCameraType(REveViewer::kCameraOrthoXOY);
  rphiView->AddScene(rphiGeomScene_);
  rphiView->AddScene(rphiEventScene_);

  // Rho-Z projection
  rhozGeomScene_ = eveMng_->SpawnNewScene("RhoZ Geometry", "RhoZ");
  rhozEventScene_ = eveMng_->SpawnNewScene("RhoZ Event Data", "RhoZ");
  rhozMgr_ = new REveProjectionManager(REveProjection::kPT_RhoZ);
  auto *rhozView = eveMng_->SpawnNewViewer("RhoZ View", "");
  rhozView->SetCameraType(REveViewer::kCameraOrthoXOY);
  rhozView->AddScene(rhozGeomScene_);
  rhozView->AddScene(rhozEventScene_);

  // project the geometry once; endcaps would cover the whole R-Phi view
  for(auto *el : eveMng_->GetGlobalScene()->RefChildren())
  {
    for(auto *shape : el->RefChildren())
    {
      if(!TString(shape->GetCName()).Contains("endcap")) rphiMgr_->ImportElements(shape, rphiGeomScene_);
      rhozMgr_->ImportElements(shape, rhozGeomScene_);
    }
  }
}

//------------------------------------------------------------------------------

void DelphesEventDisplay7::buildLego(DelphesCaloData7 *calo)
{
  if(!etaAxis_ || !phiAxis_) return;

  calo->SetEtaBins(std::unique_ptr<TAxis>((TAxis *)etaAxis_->Clone()));
  calo->SetPhiBins(std::unique_ptr<TAxis>((TAxis *)phiAxis_->Clone()));

  // The web interface has no native lego renderer: a viewer named "Lego"
  // draws with JSROOT the canvas stored (JSON, base64) in the title of the
  // first element of its scene.
  std::vector<Double_t> etaEdges = binEdges(etaAxis_);
  std::vector<Double_t> phiEdges = binEdges(phiAxis_);
  legoEcal_ = new TH2F("DelphesLegoECAL", "ECAL", etaEdges.size() - 1, etaEdges.data(), phiEdges.size() - 1, phiEdges.data());
  legoHcal_ = new TH2F("DelphesLegoHCAL", "HCAL", etaEdges.size() - 1, etaEdges.data(), phiEdges.size() - 1, phiEdges.data());
  for(TH2F *h : {legoEcal_, legoHcal_})
  {
    h->SetDirectory(nullptr);
    h->SetStats(kFALSE);
    h->GetXaxis()->SetTitle("#eta");
    h->GetYaxis()->SetTitle("#varphi");
    h->GetZaxis()->SetTitle("E [GeV]");
  }
  legoEcal_->SetFillColor(kRed);
  legoHcal_->SetFillColor(kBlue);

  legoStack_ = new THStack("DelphesLegoStack", "");
  legoStack_->Add(legoEcal_);
  legoStack_->Add(legoHcal_);

  // canvas without graphics, only used to produce the JSON; empty bins are not drawn
  legoCanvas_ = new TCanvas(kFALSE);
  legoCanvas_->SetName("DelphesLegoCanvas");
  legoCanvas_->SetTitle("Delphes Lego");
  legoCanvas_->Add(legoStack_, "lego1 0");

  legoScene_ = eveMng_->SpawnNewScene("Lego", "LegoView");
  auto *legoView = eveMng_->SpawnNewViewer("Lego", "");
  legoView->AddScene(legoScene_);
  legoHolder_ = new REveElement("Calorimeter lego");
  legoScene_->AddElement(legoHolder_);
}

//------------------------------------------------------------------------------

void DelphesEventDisplay7::fillLego()
{
  if(!legoHolder_ || !caloBranch_) return;

  DelphesCaloData7 *calo = static_cast<DelphesBranchElement7<DelphesCaloData7> *>(caloBranch_)->GetContainer();
  legoEcal_->Reset();
  legoHcal_->Reset();
  std::vector<REveCaloData::CellGeom_t> &cells = calo->GetCellGeom();
  std::vector<Float_t> &ecal = calo->GetSliceVals(0);
  std::vector<Float_t> &hcal = calo->GetSliceVals(1);
  for(size_t i = 0; i < cells.size(); ++i)
  {
    legoEcal_->Fill(cells[i].Eta(), cells[i].Phi(), ecal[i]);
    legoHcal_->Fill(cells[i].Eta(), cells[i].Phi(), hcal[i]);
  }
  legoStack_->Modified();
  legoCanvas_->Modified(kTRUE);

  TString json = TBufferJSON::ToJSON(legoCanvas_, TBufferJSON::kNoSpaces + TBufferJSON::kSameSuppression);
  legoHolder_->SetTitle(TBase64::Encode(json).Data());
  legoHolder_->StampObjProps();
}

//------------------------------------------------------------------------------

// parses the config to extract the branches of interest and prepare containers
void DelphesEventDisplay7::readConfig(const char *configFile)
{
  ExRootConfReader *confReader = new ExRootConfReader;
  confReader->ReadFile(configFile);
  ExRootConfParam branches = confReader->GetParam("TreeWriter::Branch");
  Int_t nBranches = branches.GetSize() / 3;
  DelphesBranchElement7<REveTrackList> *tlist;
  DelphesBranchElement7<REveCompound> *elist;
  // first loop with all but tracks
  for(Int_t b = 0; b < nBranches; ++b)
  {
    TString input = branches[b * 3].GetString();
    TString name = branches[b * 3 + 1].GetString();
    TString className = branches[b * 3 + 2].GetString();
    if(className == "Tower")
    {
      if(input.Contains("eflow", TString::kIgnoreCase) || name.Contains("eflow", TString::kIgnoreCase)) continue; //no eflow
      if(caloBranch_) continue; // only the first calorimeter is displayed
      caloBranch_ = new DelphesBranchElement7<DelphesCaloData7>(name, treeReader_->UseBranch(name), kBlack);
      elements_.push_back(caloBranch_);
    }
    else if(className == "Jet")
    {
      if(input.Contains("GenJetFinder"))
      {
        elist = new DelphesBranchElement7<REveCompound>(name, treeReader_->UseBranch(name), kCyan);
        elist->GetContainer()->SetRnrSelfChildren(kFALSE, kFALSE);
      }
      else
      {
        elist = new DelphesBranchElement7<REveCompound>(name, treeReader_->UseBranch(name), kYellow);
      }
      elist->SetTrackingVolume(tkRadius_, tkHalfLength_, bz_);
      elements_.push_back(elist);
    }
    else if(className == "Electron")
    {
      tlist = new DelphesBranchElement7<REveTrackList>(name, treeReader_->UseBranch(name), kRed);
      tlist->SetTrackingVolume(tkRadius_, tkHalfLength_, bz_);
      elements_.push_back(tlist);
    }
    else if(className == "Photon")
    {
      tlist = new DelphesBranchElement7<REveTrackList>(name, treeReader_->UseBranch(name), kYellow);
      tlist->SetTrackingVolume(tkRadius_, tkHalfLength_, bz_);
      elements_.push_back(tlist);
    }
    else if(className == "Muon")
    {
      tlist = new DelphesBranchElement7<REveTrackList>(name, treeReader_->UseBranch(name), kGreen);
      tlist->SetTrackingVolume(totRadius_, muHalfLength_, bz_);
      elements_.push_back(tlist);
    }
    else if(className == "MissingET")
    {
      elist = new DelphesBranchElement7<REveCompound>(name, treeReader_->UseBranch(name), kViolet);
      elist->SetTrackingVolume(tkRadius_, tkHalfLength_, bz_);
      elements_.push_back(elist);
    }
    else if(className == "GenParticle")
    {
      tlist = new DelphesBranchElement7<REveTrackList>(name, treeReader_->UseBranch(name), kCyan);
      tlist->SetTrackingVolume(tkRadius_, tkHalfLength_, bz_);
      tlist->GetContainer()->SetRnrSelfChildren(kFALSE, kFALSE);
      elements_.push_back(tlist);
    }
  }
  // second loop for tracks
  for(Int_t b = 0; b < nBranches; ++b)
  {
    TString input = branches[b * 3].GetString();
    TString name = branches[b * 3 + 1].GetString();
    TString className = branches[b * 3 + 2].GetString();
    if(className == "Track")
    {
      if(input.Contains("eflow", TString::kIgnoreCase) || name.Contains("eflow", TString::kIgnoreCase)) continue; //no eflow
      tlist = new DelphesBranchElement7<REveTrackList>(name, treeReader_->UseBranch(name), kBlue);
      tlist->SetTrackingVolume(tkRadius_, tkHalfLength_, bz_);
      elements_.push_back(tlist);
    }
  }
  delete confReader;
}

//------------------------------------------------------------------------------

void DelphesEventDisplay7::GotoEvent(Long64_t event)
{
  REveManager::ChangeGuard guard;
  LoadEvent(event);
}

void DelphesEventDisplay7::NextEvent()
{
  GotoEvent(event_id_ + 1);
}

void DelphesEventDisplay7::PreviousEvent()
{
  GotoEvent(event_id_ - 1);
}

void DelphesEventDisplay7::LoadEvent(Long64_t event)
{
  if(event < 0 || event >= GetEntries())
  {
    std::cout << "** No such event: " << event << " (the file has " << GetEntries() << " events)" << std::endl;
    return;
  }
  event_id_ = event;
  loadEvent();
}

//------------------------------------------------------------------------------

void DelphesEventDisplay7::loadEvent()
{
  // drop the projections of the previous event before its content
  rphiEventScene_->DestroyElements();
  rhozEventScene_->DestroyElements();
  for(auto *element : elements_) element->Reset();

  // load selected branches with data from the current event
  treeReader_->ReadEntry(event_id_);
  for(auto *element : elements_) element->ReadBranch();

  fillLego();
  projectEvent();

  navigator_->SetName(Form("Event %lld / %lld", event_id_, GetEntries()));
  navigator_->StampObjProps();
  std::cout << "** Loaded event " << event_id_ << std::endl;
}

//------------------------------------------------------------------------------

void DelphesEventDisplay7::projectEvent()
{
  for(auto *el : eveMng_->GetEventScene()->RefChildren())
  {
    if(dynamic_cast<REveCaloData *>(el)) continue; // not a visual element
    for(auto mgrScene : {std::make_pair(rphiMgr_, rphiEventScene_), std::make_pair(rhozMgr_, rhozEventScene_)})
    {
      REveElement *projected = mgrScene.first->ImportElements(el, mgrScene.second);
      if(projected) projected->SetRnrSelfChildren(el->GetRnrSelf(), el->GetRnrChildren());
    }
  }
}

//------------------------------------------------------------------------------
