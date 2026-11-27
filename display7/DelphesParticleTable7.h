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

#ifndef DelphesParticleTable7_h
#define DelphesParticleTable7_h

#include <map>
#include <memory>
#include <set>
#include <vector>

#include <ROOT/REveElement.hxx>

class DelphesBranchBase7;

namespace ROOT {
namespace Experimental {
class REveDataCollection;
class REveManager;
class REveScene;
class REveTableProxyBuilder;
class REveTableViewInfo;
class REveViewContext;
} // namespace Experimental
} // namespace ROOT

/** \class DelphesParticleTable7
 *
 *  Particle list of the EVE7 display: the objects of each displayed branch
 *  in the "Table" view of the web interface, one collection per branch.
 *
 *  Selection is shared with the 3D and projected views: picking an object
 *  selects its row, and selecting a row highlights the object.
 */

class DelphesParticleTable7
{
public:
  DelphesParticleTable7(ROOT::Experimental::REveManager *eveMng, const std::vector<DelphesBranchBase7 *> &branches);
  ~DelphesParticleTable7();

  // refill the collections from the current entry of the branches
  // (to be called after the branches have been read)
  void LoadEvent();

  // the collection and the row of a displayed element, if any
  bool FindItem(ROOT::Experimental::REveElement *el, ROOT::Experimental::REveElement *&itemList, int &index) const;

private:
  struct Entry
  {
    DelphesBranchBase7 *branch;
    ROOT::Experimental::REveDataCollection *collection;
    ROOT::Experimental::REveTableProxyBuilder *builder;
  };

  void defineColumns();
  void fillCollection(Entry &entry);
  void fillImpliedSelected(const Entry &entry, ROOT::Experimental::REveElement::Set_t &impSelSet, const std::set<int> &indices) const;
  void itemsChanged(const Entry &entry, const std::vector<int> &indices);

  ROOT::Experimental::REveManager *eveMng_;
  ROOT::Experimental::REveViewContext *viewContext_;
  ROOT::Experimental::REveTableViewInfo *tableInfo_;
  ROOT::Experimental::REveScene *collectionScene_;
  ROOT::Experimental::REveScene *tableScene_;
  std::vector<Entry> entries_;
  // displayed element -> (item list, index); rebuilt for every event
  std::map<ROOT::Experimental::ElementId_t, std::pair<ROOT::Experimental::REveElement *, int>> elementToItem_;
};

#endif // DelphesParticleTable7_h
