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

/** Display7LinkDef
 *
 *  Lists classes to be included in the dictionary of the EVE7 event display.
 *  DelphesEventNavigator7 must be in the dictionary: the web interface calls
 *  its methods through the interpreter.
 *
 */

#include "display/Delphes3DGeometry.h"
#include "display7/DelphesBranchElement7.h"
#include "display7/DelphesCaloData7.h"
#include "display7/DelphesEventDisplay7.h"

#ifdef __CINT__

#pragma link off all globals;
#pragma link off all classes;
#pragma link off all functions;

#pragma link C++ class Delphes3DGeometry;
#pragma link C++ class DelphesCaloData7;
#pragma link C++ class DelphesBranchBase7;
#pragma link C++ class DelphesBranchElement7<DelphesCaloData7>;
#pragma link C++ class DelphesBranchElement7<ROOT::Experimental::REveCompound>;
#pragma link C++ class DelphesBranchElement7<ROOT::Experimental::REveTrackList>;
#pragma link C++ class DelphesEventNavigator7;
#pragma link C++ class DelphesEventDisplay7;

#endif
