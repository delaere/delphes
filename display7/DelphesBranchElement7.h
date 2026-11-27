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

#ifndef DelphesBranchElement7_h
#define DelphesBranchElement7_h

#include <vector>

#include "TClass.h"
#include "TClonesArray.h"
#include "TColor.h"
#include "TLorentzVector.h"
#include "TString.h"

#include <ROOT/REveCompound.hxx>
#include <ROOT/REveTrack.hxx>

#include "display7/DelphesCaloData7.h"

/** \class DelphesBranchBase7
 *
 *  EVE7 counterpart of DelphesBranchBase:
 *  represents the objects of one branch of a Delphes tree.
 */

class DelphesBranchBase7
{
public:
  DelphesBranchBase7(const char *name = "", TClonesArray *branch = nullptr, const enum EColor color = kBlack, Float_t maxPt = 50.) :
    name_(name), maxPt_(maxPt), branch_(branch), color_(color), tkRadius_(0.), tkHalfLength_(0.), tk_Bz_(0.) {}
  virtual ~DelphesBranchBase7() {}
  const char *GetName() const { return (const char *)name_; }
  const char *GetType() const { return branch_ ? branch_->GetClass()->GetName() : "None"; }
  enum EColor GetColor() const { return color_; }
  virtual void SetTrackingVolume(Float_t r, Float_t l, Float_t Bz = 0.)
  {
    tkRadius_ = r;
    tkHalfLength_ = l;
    tk_Bz_ = Bz;
  }
  // the EVE element holding the objects of this branch
  virtual ROOT::Experimental::REveElement *GetElement() = 0;
  // resets the collection (before moving to the next event)
  virtual void Reset() = 0;
  // read the branch and fill elements for display
  virtual void ReadBranch() = 0;
  // the four-vectors of the objects of a Delphes branch, whatever its class
  // (only final-state particles for GenParticle)
  static std::vector<TLorentzVector> Vectors(TClonesArray *branch);
  std::vector<TLorentzVector> GetVectors() const { return Vectors(branch_); }

  TClonesArray *GetBranch() const { return branch_; }
  // the EVE element drawn for object i of the branch (nullptr if not drawn)
  ROOT::Experimental::REveElement *GetItemElement(Int_t i) const
  {
    return (i >= 0 && i < (Int_t)items_.size()) ? items_[i] : nullptr;
  }
  Int_t GetNItemElements() const { return items_.size(); }

protected:
  TString name_;
  Float_t maxPt_;
  TClonesArray *branch_;
  const enum EColor color_;
  Float_t tkRadius_, tkHalfLength_, tk_Bz_;
  std::vector<ROOT::Experimental::REveElement *> items_; // one per object of the branch
};

// Concrete implementations. EveContainer can be
//  - REveTrackList     (tracks, electrons, muons, photons, generated particles)
//  - REveCompound      (jets, missing transverse energy)
//  - DelphesCaloData7  (calorimeter towers)
template <typename EveContainer>
class DelphesBranchElement7: public DelphesBranchBase7
{
public:
  DelphesBranchElement7(const char *name = "", TClonesArray *branch = nullptr, const enum EColor color = kBlack, Float_t maxPt = 50.);

  // the container is owned by the EVE scene it is added to
  ~DelphesBranchElement7() override {}

  EveContainer *GetContainer() { return data_; }
  ROOT::Experimental::REveElement *GetElement() override { return data_; }

  void SetTrackingVolume(Float_t r, Float_t l, Float_t Bz = 0.) override;
  void Reset() override;
  void ReadBranch() override;

private:
  EveContainer *data_;
};

// calorimeter towers
template <>
DelphesBranchElement7<DelphesCaloData7>::DelphesBranchElement7(const char *name, TClonesArray *branch, const enum EColor color, Float_t maxPt);
template <>
void DelphesBranchElement7<DelphesCaloData7>::SetTrackingVolume(Float_t r, Float_t l, Float_t Bz);
template <>
void DelphesBranchElement7<DelphesCaloData7>::Reset();
template <>
void DelphesBranchElement7<DelphesCaloData7>::ReadBranch();

// element lists (jets, missing transverse energy)
template <>
DelphesBranchElement7<ROOT::Experimental::REveCompound>::DelphesBranchElement7(const char *name, TClonesArray *branch, const enum EColor color, Float_t maxPt);
template <>
void DelphesBranchElement7<ROOT::Experimental::REveCompound>::SetTrackingVolume(Float_t r, Float_t l, Float_t Bz);
template <>
void DelphesBranchElement7<ROOT::Experimental::REveCompound>::Reset();
template <>
void DelphesBranchElement7<ROOT::Experimental::REveCompound>::ReadBranch();

// track lists
template <>
DelphesBranchElement7<ROOT::Experimental::REveTrackList>::DelphesBranchElement7(const char *name, TClonesArray *branch, const enum EColor color, Float_t maxPt);
template <>
void DelphesBranchElement7<ROOT::Experimental::REveTrackList>::SetTrackingVolume(Float_t r, Float_t l, Float_t Bz);
template <>
void DelphesBranchElement7<ROOT::Experimental::REveTrackList>::Reset();
template <>
void DelphesBranchElement7<ROOT::Experimental::REveTrackList>::ReadBranch();

#endif // DelphesBranchElement7_h
