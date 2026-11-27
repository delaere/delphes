# Delphes EVE7 event display (prototype)

Web-based event display built on ROOT EVE7 (`ROOT::Experimental::REve*`).
It sits next to the classic EVE display in `display/` and does not replace it.

## Requirements

ROOT built with `root7` and `webgui` (it provides `libROOTEve`).
CMake builds `libDelphesDisplay7` only when `ROOT::ROOTEve` is available.
The legacy `./configure && make` build does not build it yet.

## Usage

```
root -l examples/EventDisplay7.C'("cards/delphes_card_CMS.tcl","delphes_output.root")'
```

The display opens in a web browser: a 3D view, R-Phi and Rho-Z projections,
an eta-phi lego of the calorimeter towers and the particle list. The toolbar
buttons navigate through the events. From the ROOT prompt:

```
delphesDisplay7->GotoEvent(42);
delphesDisplay7->NextEvent();
delphesDisplay7->PreviousEvent();
```

### Particle list

The "Table" view lists the objects of each branch (choose the branch in the
drop-down list). Selecting a row highlights the object in all views, and
picking an object in a view selects its row. "Edit Collection Columns" adds
columns from C++ expressions on the object `i`, e.g. `i.BTag` for a jet.
Hovering an object in a view shows the first columns as a tooltip.

### Summary page

The "Summary" button opens a second page with, for each branch, the
multiplicity, pT, eta and phi of the objects of the current event. "Fill from
sample" reads all events and adds the sample distributions (all, leading and
subleading objects, per event) for comparison. The page also navigates
through the events (buttons, event number, left/right arrow keys).

```
delphesDisplay7->ShowSummary();  // open the page
delphesDisplay7->FillSummary();  // fill the sample distributions now
```

With `root --web=server`, the address of the page is printed when it is
opened.

## Differences with the classic display

- Missing transverse energy is a dashed line whose length is proportional to
  MET (capped at the tracker radius); EVE7 has no arrow primitive.
- The lego is drawn by JSROOT from a `THStack` of two `TH2F` (ECAL, HCAL),
  filled at the tower centres; EVE7 has no native lego renderer.
- The calorimeter and muon endcaps are not drawn in the R-Phi projection.
- The HTML summary tables are replaced by the "Table" view, linked to the
  other views, and the summary plots by the summary page.

## Implementation notes

- `DelphesEventNavigator7` is the element whose methods the toolbar buttons
  call; the EVE7 server runs them with the scenes open for changes. Calls from
  the ROOT prompt go through `DelphesEventDisplay7::GotoEvent`, which takes
  `REveManager::ChangeGuard`.
- The `TClass` of every displayed type is built in the main thread before
  `REveManager::Show()`. Otherwise the web server threads build them when a
  browser connects, which runs the interpreter in a thread with a small stack
  and crashes.
- The detector is not projected by EVE7 in the 2D views: its projection
  rebuilds outlines from the tessellated mesh, which fails for the composite
  (boolean) shapes of `Delphes3DGeometry`. The volumes are axially symmetric,
  so their R-Z cross-sections are computed by casting radial rays through the
  TGeo shapes, and drawn as filled polygons (Rho-Z) and rings (R-Phi).
- The particle list uses `REveDataCollection` and `REveTableProxyBuilder`,
  one collection per branch, pointing to the objects of the current entry. A
  selection deviator maps picked objects to (collection, row), and the rows
  back to the drawn objects and their projections.
- The summary page is an `RWebWindow` served by the same server, with its
  HTML embedded in the library (`DelphesSummaryPage7.inc`). It is updated
  from the main thread by a ROOT timer, and fills the sample with its own
  tree reader, in short slices so that the display stays responsive. The page
  draws the histograms with JSROOT and sets their style.
- `Delphes3DGeometry` (pure TGeo) is shared with `display/` and compiled into
  both libraries.
