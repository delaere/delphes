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

#include "TClass.h"
#include "TClonesArray.h"
#include "TString.h"

#include <ROOT/REveDataCollection.hxx>
#include <ROOT/REveManager.hxx>
#include <ROOT/REveProjectionBases.hxx>
#include <ROOT/REveScene.hxx>
#include <ROOT/REveSelection.hxx>
#include <ROOT/REveTableInfo.hxx>
#include <ROOT/REveTableProxyBuilder.hxx>
#include <ROOT/REveViewContext.hxx>
#include <ROOT/REveViewer.hxx>

#include "display7/DelphesBranchElement7.h"
#include "display7/DelphesParticleTable7.h"

using namespace ROOT::Experimental;

namespace
{

// Routes picking of displayed objects to the rows of the particle table, so
// that the table and the views share one selection. Selecting a row (or an
// object) then highlights the object through FillImpliedSelected.
class DelphesSelectionDeviator : public REveSelection::Deviator
{
public:
  explicit DelphesSelectionDeviator(const DelphesParticleTable7 *table) : table_(table) {}

  using REveSelection::Deviator::DeviateSelection;
  bool DeviateSelection(REveSelection *selection, REveElement *el, bool multi, bool, const std::set<int> &) override
  {
    if(!el) return false;
    if(auto *items = dynamic_cast<REveDataItemList *>(el))
    {
      ExecuteNewElementPicked(selection, items, multi, true, items->RefSelectedSet());
      return true;
    }
    REveElement *items = nullptr;
    int index = -1;
    if(table_->FindItem(el, items, index))
    {
      ExecuteNewElementPicked(selection, items, multi, true, {index});
      return true;
    }
    return false;
  }

private:
  const DelphesParticleTable7 *table_;
};

// apply f to an element and to its projected replicas
template <typename F>
void withReplicas(REveElement *el, F f)
{
  f(el);
  if(auto *projectable = dynamic_cast<REveProjectable *>(el))
    for(auto *projected : projectable->RefProjecteds()) f(projected->GetProjectedAsElement());
}

} // namespace

//------------------------------------------------------------------------------

DelphesParticleTable7::DelphesParticleTable7(REveManager *eveMng, const std::vector<DelphesBranchBase7 *> &branches) :
  eveMng_(eveMng), viewContext_(new REveViewContext()), tableInfo_(new REveTableViewInfo()),
  collectionScene_(nullptr), tableScene_(nullptr)
{
  defineColumns();
  viewContext_->SetTableViewInfo(tableInfo_);

  // the "Table" viewer of the web interface shows the "Tables" scene,
  // and lists the collections of the "Collections" scene
  tableScene_ = eveMng_->SpawnNewScene("Tables", "Tables");
  tableScene_->AddElement(tableInfo_);
  auto *tableView = eveMng_->SpawnNewViewer("Table", "Particle list");
  tableView->AddScene(tableScene_);
  collectionScene_ = eveMng_->SpawnNewScene("Collections", "Collections");

  for(auto *branch : branches)
  {
    TString type = branch->GetType();
    if(type == "Tower" || tableInfo_->RefTableEntries(type.Data()).empty()) continue;

    auto *collection = new REveDataCollection(branch->GetName());
    collection->SetItemClass(TClass::GetClass(type));
    collection->SetMainColor(branch->GetColor());
    if(type == "GenParticle") collection->SetFilterExpr("i.Status == 1"); // the drawn ones
    collectionScene_->AddElement(collection);

    auto *builder = new REveTableProxyBuilder();
    builder->SetHaveAWindow(true);
    builder->SetCollection(collection);
    tableScene_->AddElement(builder->CreateProduct("table-type", viewContext_));
    builder->Build();
    tableInfo_->AddDelegate([builder]() { builder->ConfigChanged(); });

    // tooltip of the objects in the views: the first columns of the table
    auto &columns = tableInfo_->RefTableEntries(type.Data());
    for(size_t c = 0; c < std::min<size_t>(columns.size(), 4); ++c)
      collection->GetItemList()->AddTooltipExpression(columns[c].fName, columns[c].fExpression);

    entries_.push_back({branch, collection, builder});
  }

  for(size_t i = 0; i < entries_.size(); ++i)
  {
    REveDataItemList *items = entries_[i].collection->GetItemList();
    items->SetFillImpliedSelectedDelegate(
      [this, i](REveDataItemList *, REveElement::Set_t &impSelSet, const std::set<int> &indices) {
        fillImpliedSelected(entries_[i], impSelSet, indices);
      });
    items->SetItemsChangeDelegate(
      [this, i](REveDataItemList *, const std::vector<int> &indices) {
        itemsChanged(entries_[i], indices);
      });
  }

  // show the displayed jets first, if any
  for(auto &entry : entries_)
  {
    if(TString(entry.branch->GetType()) == "Jet" && entry.branch->GetElement()->GetRnrSelf())
    {
      tableInfo_->SetDisplayedCollection(entry.collection->GetElementId());
      break;
    }
  }
  if(!entries_.empty() && !tableInfo_->GetDisplayedCollection())
    tableInfo_->SetDisplayedCollection(entries_.front().collection->GetElementId());

  auto deviator = std::make_shared<DelphesSelectionDeviator>(this);
  eveMng_->GetSelection()->SetDeviator(deviator);
  eveMng_->GetHighlight()->SetDeviator(deviator);
}

DelphesParticleTable7::~DelphesParticleTable7()
{
  delete viewContext_;
}

//------------------------------------------------------------------------------

void DelphesParticleTable7::defineColumns()
{
  // columns are C++ expressions on the object "i"; more can be added from
  // the web interface
  tableInfo_->table("Jet")
    .column("pT", 2, "i.PT")
    .column("eta", 3, "i.Eta")
    .column("phi", 3, "i.Phi")
    .column("mass", 2, "i.Mass")
    .column("charge", 0, "i.Charge")
    .column("b-tag", 0, "i.BTag")
    .column("tau-tag", 0, "i.TauTag");
  tableInfo_->table("Track")
    .column("PID", 0, "i.PID")
    .column("charge", 0, "i.Charge")
    .column("pT", 2, "i.PT")
    .column("eta", 3, "i.Eta")
    .column("phi", 3, "i.Phi")
    .column("d0", 3, "i.D0")
    .column("dz", 3, "i.DZ");
  tableInfo_->table("Electron")
    .column("charge", 0, "i.Charge")
    .column("pT", 2, "i.PT")
    .column("eta", 3, "i.Eta")
    .column("phi", 3, "i.Phi")
    .column("isolation", 3, "i.IsolationVar")
    .column("Ehad/Eem", 3, "i.EhadOverEem");
  tableInfo_->table("Muon")
    .column("charge", 0, "i.Charge")
    .column("pT", 2, "i.PT")
    .column("eta", 3, "i.Eta")
    .column("phi", 3, "i.Phi")
    .column("isolation", 3, "i.IsolationVar");
  tableInfo_->table("Photon")
    .column("E", 2, "i.E")
    .column("pT", 2, "i.PT")
    .column("eta", 3, "i.Eta")
    .column("phi", 3, "i.Phi")
    .column("isolation", 3, "i.IsolationVar")
    .column("Ehad/Eem", 3, "i.EhadOverEem");
  tableInfo_->table("GenParticle")
    .column("PID", 0, "i.PID")
    .column("status", 0, "i.Status")
    .column("charge", 0, "i.Charge")
    .column("mass", 3, "i.Mass")
    .column("E", 2, "i.E")
    .column("pT", 2, "i.PT")
    .column("eta", 3, "i.Eta")
    .column("phi", 3, "i.Phi");
  tableInfo_->table("MissingET")
    .column("MET", 2, "i.MET")
    .column("phi", 3, "i.Phi");
}

//------------------------------------------------------------------------------

void DelphesParticleTable7::LoadEvent()
{
  elementToItem_.clear();
  for(auto &entry : entries_)
  {
    fillCollection(entry);
    entry.builder->Build();
  }
}

void DelphesParticleTable7::fillCollection(Entry &entry)
{
  REveDataCollection *collection = entry.collection;
  collection->ClearItems();
  TClonesArray *branch = entry.branch->GetBranch();
  Int_t n = branch ? branch->GetEntriesFast() : 0;
  collection->ReserveItems(n);
  REveDataItemList *items = collection->GetItemList();
  for(Int_t i = 0; i < n; ++i)
  {
    collection->AddItem(branch->At(i), Form("%s %d", collection->GetCName(), i), "");
    if(REveElement *el = entry.branch->GetItemElement(i))
      withReplicas(el, [&](REveElement *e) { elementToItem_[e->GetElementId()] = {items, i}; });
  }
  collection->ApplyFilter();
}

bool DelphesParticleTable7::FindItem(REveElement *el, REveElement *&itemList, int &index) const
{
  auto it = elementToItem_.find(el->GetElementId());
  if(it == elementToItem_.end()) return false;
  itemList = it->second.first;
  index = it->second.second;
  return true;
}

void DelphesParticleTable7::fillImpliedSelected(const Entry &entry, REveElement::Set_t &impSelSet, const std::set<int> &indices) const
{
  for(int i : indices)
    if(REveElement *el = entry.branch->GetItemElement(i))
      withReplicas(el, [&](REveElement *e) { impSelSet.insert(e); });
}

void DelphesParticleTable7::itemsChanged(const Entry &entry, const std::vector<int> &indices)
{
  // visibility and colour changed from the web interface
  for(int i : indices)
  {
    if(i < 0 || i >= entry.collection->GetNItems()) continue;
    const REveDataItem *item = entry.collection->GetDataItem(i);
    if(REveElement *el = entry.branch->GetItemElement(i))
      withReplicas(el, [&](REveElement *e) {
        e->SetRnrSelf(item->GetRnrSelf());
        e->SetMainColor(item->GetMainColor());
      });
  }
}
