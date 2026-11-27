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

#include <algorithm>
#include <cmath>

#include "TParticle.h"

#include <ROOT/REveJetCone.hxx>
#include <ROOT/REveStraightLineSet.hxx>
#include <ROOT/REveTrackPropagator.hxx>

#include "classes/DelphesClasses.h"
#include "display7/DelphesBranchElement7.h"

using namespace ROOT::Experimental;

//------------------------------------------------------------------------------
// calorimeter towers
//------------------------------------------------------------------------------

template <>
DelphesBranchElement7<DelphesCaloData7>::DelphesBranchElement7(const char *name, TClonesArray *branch, const enum EColor color, Float_t maxPt) :
  DelphesBranchBase7(name, branch, color, maxPt)
{
  data_ = new DelphesCaloData7(2);
  data_->SetName(name);
  data_->RefSliceInfo(0).Setup("ECAL", 0.1, kRed);
  data_->RefSliceInfo(1).Setup("HCAL", 0.1, kBlue);
}

template <>
void DelphesBranchElement7<DelphesCaloData7>::SetTrackingVolume(Float_t r, Float_t l, Float_t Bz)
{
  DelphesBranchBase7::SetTrackingVolume(r, l, Bz);
}

template <>
void DelphesBranchElement7<DelphesCaloData7>::Reset() { data_->ClearTowers(); }

template <>
void DelphesBranchElement7<DelphesCaloData7>::ReadBranch()
{
  if(TString(GetType()) == "Tower")
  {
    TIter itTower(branch_);
    Tower *tower;
    while((tower = (Tower *)itTower.Next()))
    {
      data_->AddTower(tower->Edges[0], tower->Edges[1], tower->Edges[2], tower->Edges[3]);
      data_->FillSlice(0, tower->Eem);
      data_->FillSlice(1, tower->Ehad);
    }
  }
  data_->DataChanged();
}

template <>
std::vector<TLorentzVector> DelphesBranchElement7<DelphesCaloData7>::GetVectors()
{
  std::vector<TLorentzVector> output;
  if(TString(GetType()) == "Tower")
  {
    TIter itTower(branch_);
    Tower *tower;
    while((tower = (Tower *)itTower.Next()))
    {
      TLorentzVector v;
      v.SetPtEtaPhiM(tower->Eem + tower->Ehad, (tower->Edges[0] + tower->Edges[1]) / 2., (tower->Edges[2] + tower->Edges[3]) / 2., 0.);
      output.push_back(v);
    }
  }
  return output;
}

//------------------------------------------------------------------------------
// element lists: jets and missing transverse energy
//------------------------------------------------------------------------------

template <>
DelphesBranchElement7<REveCompound>::DelphesBranchElement7(const char *name, TClonesArray *branch, const enum EColor color, Float_t maxPt) :
  DelphesBranchBase7(name, branch, color, maxPt)
{
  data_ = new REveCompound(name);
  data_->SetMainColor(color_);
}

template <>
void DelphesBranchElement7<REveCompound>::SetTrackingVolume(Float_t r, Float_t l, Float_t Bz)
{
  DelphesBranchBase7::SetTrackingVolume(r, l, Bz);
}

template <>
void DelphesBranchElement7<REveCompound>::Reset() { data_->DestroyElements(); }

template <>
void DelphesBranchElement7<REveCompound>::ReadBranch()
{
  if(TString(GetType()) == "Jet")
  {
    TIter itJet(branch_);
    Jet *jet;
    Int_t counter = 0;
    while((jet = (Jet *)itJet.Next()))
    {
      auto *cone = new REveJetCone(Form("jet [%d]", counter));
      cone->SetTitle(Form("jet [%d]: Pt=%f, Eta=%f, \nPhi=%f, M=%f", counter, jet->PT, jet->Eta, jet->Phi, jet->Mass));
      cone->SetMainTransparency(60);
      cone->SetLineColor(GetColor());
      cone->SetFillColor(GetColor());
      cone->SetCylinder(tkRadius_ - 10, tkHalfLength_ - 10);
      cone->SetPickable(kTRUE);
      cone->AddEllipticCone(jet->Eta, jet->Phi, jet->DeltaEta, jet->DeltaPhi);
      data_->AddElement(cone);
      ++counter;
    }
  }
  else if(TString(GetType()) == "MissingET")
  {
    // There is no arrow primitive in EVE7: draw a dashed line in the
    // transverse plane, with a length proportional to MET (capped at the
    // tracker radius) and a marker at its tip.
    if(branch_->GetEntriesFast() > 0)
    {
      MissingET *MET = (MissingET *)branch_->At(0);
      Float_t length = tkRadius_ * std::min(1.f, MET->MET / maxPt_);
      auto *met = new REveStraightLineSet("Missing Et");
      met->SetTitle(Form("Missing Et: MET=%f, Phi=%f", MET->MET, MET->Phi));
      met->AddLine(0., 0., 0., length * std::cos(MET->Phi), length * std::sin(MET->Phi), 0.);
      met->AddMarker(0, 1.);
      met->SetMainColor(GetColor());
      met->SetMarkerColor(GetColor());
      met->SetMarkerStyle(4);
      met->SetMarkerSize(2.);
      met->SetLineWidth(2);
      met->SetLineStyle(kDashed);
      met->SetPickable(kTRUE);
      data_->AddElement(met);
    }
  }
}

template <>
std::vector<TLorentzVector> DelphesBranchElement7<REveCompound>::GetVectors()
{
  std::vector<TLorentzVector> output;
  if(TString(GetType()) == "Jet")
  {
    TIter itJet(branch_);
    Jet *jet;
    while((jet = (Jet *)itJet.Next()))
    {
      TLorentzVector v;
      v.SetPtEtaPhiM(jet->PT, jet->Eta, jet->Phi, jet->Mass);
      output.push_back(v);
    }
  }
  else if(TString(GetType()) == "MissingET")
  {
    TIter itMet(branch_);
    MissingET *MET;
    while((MET = (MissingET *)itMet.Next()))
    {
      TLorentzVector v;
      v.SetPtEtaPhiM(MET->MET, MET->Eta, MET->Phi, 0.);
      output.push_back(v);
    }
  }
  return output;
}

//------------------------------------------------------------------------------
// track lists
//------------------------------------------------------------------------------

template <>
DelphesBranchElement7<REveTrackList>::DelphesBranchElement7(const char *name, TClonesArray *branch, const enum EColor color, Float_t maxPt) :
  DelphesBranchBase7(name, branch, color, maxPt)
{
  data_ = new REveTrackList(name, new REveTrackPropagator());
  data_->SetMainColor(color_);
  data_->SetMarkerColor(color_);
  data_->SetMarkerStyle(kCircle);
  data_->SetMarkerSize(0.5);
}

template <>
void DelphesBranchElement7<REveTrackList>::SetTrackingVolume(Float_t r, Float_t l, Float_t Bz)
{
  DelphesBranchBase7::SetTrackingVolume(r, l, Bz);
  REveTrackPropagator *trkProp = data_->GetPropagator();
  trkProp->SetMagField(0., 0., -tk_Bz_);
  trkProp->SetMaxR(tkRadius_);
  trkProp->SetMaxZ(tkHalfLength_);
}

template <>
void DelphesBranchElement7<REveTrackList>::Reset() { data_->DestroyElements(); }

template <>
void DelphesBranchElement7<REveTrackList>::ReadBranch()
{
  TString type = GetType();
  TIter itTrack(branch_);
  Int_t counter = 0;
  REveTrackPropagator *trkProp = data_->GetPropagator();

  // creates one track from a TParticle (positions in cm, momenta in GeV)
  auto addTrack = [&](TParticle &pb, bool dashed) {
    auto *eveTrack = new REveTrack(&pb, counter, trkProp);
    eveTrack->SetName(Form("%s [%d]", pb.GetName(), counter++));
    eveTrack->SetStdTitle();
    eveTrack->SetAttLineAttMarker(data_);
    eveTrack->SetMainColor(GetColor());
    if(dashed) eveTrack->SetLineStyle(kDashed);
    eveTrack->SetPickable(kTRUE);
    data_->AddElement(eveTrack);
    eveTrack->MakeTrack();
  };

  if(type == "Track")
  {
    Track *track;
    while((track = (Track *)itTrack.Next()))
    {
      TParticle pb(track->PID, 1, 0, 0, 0, 0,
        track->P4().Px(), track->P4().Py(), track->P4().Pz(), track->P4().E(),
        track->X / 10.0, track->Y / 10.0, track->Z / 10.0, track->T / 10.0);
      addTrack(pb, false);
    }
  }
  else if(type == "Electron")
  {
    Electron *electron;
    while((electron = (Electron *)itTrack.Next()))
    {
      GenParticle *particle = (GenParticle *)electron->Particle.GetObject();
      TParticle pb(electron->Charge < 0 ? 11 : -11, 1, 0, 0, 0, 0,
        electron->P4().Px(), electron->P4().Py(), electron->P4().Pz(), electron->P4().E(),
        particle->X / 10.0, particle->Y / 10.0, particle->Z / 10.0, particle->T / 10.0);
      addTrack(pb, false);
    }
  }
  else if(type == "Muon")
  {
    Muon *muon;
    while((muon = (Muon *)itTrack.Next()))
    {
      GenParticle *particle = (GenParticle *)muon->Particle.GetObject();
      TParticle pb(muon->Charge < 0 ? 13 : -13, 1, 0, 0, 0, 0,
        muon->P4().Px(), muon->P4().Py(), muon->P4().Pz(), muon->P4().E(),
        particle->X / 10.0, particle->Y / 10.0, particle->Z / 10.0, particle->T / 10.0);
      addTrack(pb, false);
    }
  }
  else if(type == "Photon")
  {
    Photon *photon;
    while((photon = (Photon *)itTrack.Next()))
    {
      TParticle pb(22, 1, 0, 0, 0, 0,
        photon->P4().Px(), photon->P4().Py(), photon->P4().Pz(), photon->P4().E(),
        0.0, 0.0, 0.0, 0.0);
      addTrack(pb, true);
    }
  }
  else if(type == "GenParticle")
  {
    GenParticle *particle;
    while((particle = (GenParticle *)itTrack.Next()))
    {
      if(particle->Status != 1) continue;
      TParticle pb(particle->PID, particle->Status, particle->M1, particle->M2, particle->D1, particle->D2,
        particle->P4().Px(), particle->P4().Py(), particle->P4().Pz(), particle->P4().E(),
        particle->X / 10.0, particle->Y / 10.0, particle->Z / 10.0, particle->T / 10.0);
      addTrack(pb, particle->Charge == 0);
    }
  }
}

template <>
std::vector<TLorentzVector> DelphesBranchElement7<REveTrackList>::GetVectors()
{
  std::vector<TLorentzVector> output;
  TString type = GetType();
  TIter itTrack(branch_);
  if(type == "Track")
  {
    Track *track;
    while((track = (Track *)itTrack.Next())) output.push_back(track->P4());
  }
  else if(type == "Electron")
  {
    Electron *electron;
    while((electron = (Electron *)itTrack.Next())) output.push_back(electron->P4());
  }
  else if(type == "Muon")
  {
    Muon *muon;
    while((muon = (Muon *)itTrack.Next())) output.push_back(muon->P4());
  }
  else if(type == "Photon")
  {
    Photon *photon;
    while((photon = (Photon *)itTrack.Next())) output.push_back(photon->P4());
  }
  else if(type == "GenParticle")
  {
    GenParticle *particle;
    while((particle = (GenParticle *)itTrack.Next()))
    {
      if(particle->Status != 1) continue;
      output.push_back(particle->P4());
    }
  }
  return output;
}
