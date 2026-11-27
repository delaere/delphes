/* Web-based event display (ROOT EVE7). Requires ROOT built with root7 and webgui.
 *
 * Example:
 * root -l examples/EventDisplay7.C'("cards/delphes_card_CMS.tcl","delphes_output.root")'
 * root -l examples/EventDisplay7.C'("cards/delphes_card_FCC_basic.tcl","delphes_output.root","ParticlePropagator","ChargedHadronTrackingEfficiency","MuonTrackingEfficiency","Ecal,Hcal")'
 *
 * The display opens in a web browser. Navigate with the buttons at the top of the page,
 * or from the ROOT prompt:
 *   delphesDisplay7->GotoEvent(42);
 *   delphesDisplay7->NextEvent();
 *   delphesDisplay7->PreviousEvent();
 */

#ifdef __CLING__
R__LOAD_LIBRARY(libGeom)
R__LOAD_LIBRARY(libROOTEve)
R__LOAD_LIBRARY(libDelphesDisplay7)
#include "display/Delphes3DGeometry.h"
#include "display7/DelphesEventDisplay7.h"
#endif

DelphesEventDisplay7 *delphesDisplay7 = nullptr;

void EventDisplay7(const char *configfile = "delphes_card_CMS.tcl",
                   const char *datafile = "delphes_output.root",
                   const char *ParticlePropagator = "ParticlePropagator",
                   const char *TrackingEfficiency = "ChargedHadronTrackingEfficiency",
                   const char *MuonEfficiency = "MuonEfficiency",
                   const char *Calorimeters = "HCal")
{
  // create the detector representation
  Delphes3DGeometry det3D(new TGeoManager("delphes", "Delphes geometry"), true);
  det3D.readFile(configfile, ParticlePropagator, TrackingEfficiency, MuonEfficiency, Calorimeters);

  // create the application
  delphesDisplay7 = new DelphesEventDisplay7(configfile, datafile, det3D);
}
