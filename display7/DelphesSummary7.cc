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
#include <chrono>
#include <cmath>
#include <set>
#include <sstream>

#include "TBufferJSON.h"
#include "TChain.h"
#include "TClonesArray.h"
#include "TColor.h"
#include "TH1D.h"
#include "TLine.h"
#include "TMath.h"
#include "TROOT.h"
#include "TTimer.h"

#include <ROOT/RWebWindow.hxx>

#include "display7/DelphesBranchElement7.h"
#include "display7/DelphesSummary7.h"

#include "external/ExRootAnalysis/ExRootTreeReader.h"

namespace
{

const char *kPage =
#include "display7/DelphesSummaryPage7.inc"
  ;

// colours of the display that read badly on a white background
Color_t plotColor(Color_t color)
{
  switch(color)
  {
  case kYellow: return kOrange - 3;
  case kCyan: return kAzure + 7;
  case kGreen: return kGreen + 2;
  case kRed: return kRed + 1;
  case kBlue: return kBlue + 1;
  case kViolet: return kViolet + 1;
  case kBlack: return kGray + 2;
  default: return color;
  }
}

std::string hexColor(Color_t color)
{
  TColor *c = gROOT->GetColor(color);
  return c ? c->AsHexString() : "#808080";
}

std::string jsonString(const std::string &s)
{
  std::string out = "\"";
  for(char c : s)
  {
    if(c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

const char *kVariables[3] = {"pt", "eta", "phi"};

// log binning in pT, linear in eta and phi
TH1D *makeHistogram(const std::string &name, int variable)
{
  TH1D *h = nullptr;
  if(variable == 0)
  {
    const int n = 60;
    double edges[n + 1];
    for(int i = 0; i <= n; ++i) edges[i] = 0.1 * std::pow(10., 5. * i / n); // 0.1 GeV to 10 TeV
    h = new TH1D(name.c_str(), "", n, edges);
  }
  else if(variable == 1)
    h = new TH1D(name.c_str(), "", 60, -6., 6.);
  else
    h = new TH1D(name.c_str(), "", 36, -TMath::Pi(), TMath::Pi());
  h->SetDirectory(nullptr);
  return h;
}

bool byPt(const TLorentzVector &a, const TLorentzVector &b) { return a.Pt() > b.Pt(); }

} // namespace

//------------------------------------------------------------------------------

class DelphesSummary7Timer : public TTimer
{
public:
  DelphesSummary7Timer(DelphesSummary7 *summary) : TTimer(100, kTRUE), summary_(summary) {}
  Bool_t Notify() override
  {
    summary_->poll();
    Reset();
    return kTRUE;
  }

private:
  DelphesSummary7 *summary_;
};

//------------------------------------------------------------------------------

DelphesSummary7::DelphesSummary7(const char *inputFile, const std::vector<DelphesBranchBase7 *> &branches) :
  timer_(nullptr), event_(0), entries_(0), eventChanged_(false), showRequested_(false),
  inputFile_(inputFile), chain_(new TChain("Delphes")), reader_(nullptr),
  sampleNext_(0), sampleEvents_(0), sampleRunning_(false), lastProgress_(0.)
{
  chain_->Add(inputFile);
  reader_ = new ExRootTreeReader(chain_);

  for(auto *branch : branches)
  {
    Collection c;
    c.name = branch->GetName();
    c.type = branch->GetType();
    c.color = plotColor(branch->GetColor());
    c.branch = branch;
    c.sampleBranch = reader_->UseBranch(c.name.c_str());
    for(int v = 0; v < 3; ++v)
    {
      c.sample.all[v] = makeHistogram(c.name + "_all_" + kVariables[v], v);
      c.sample.leading[v] = makeHistogram(c.name + "_leading_" + kVariables[v], v);
      c.sample.subleading[v] = makeHistogram(c.name + "_subleading_" + kVariables[v], v);
    }
    collections_.push_back(c);
  }

  window_ = ROOT::RWebWindow::Create();
  window_->SetDefaultPage(kPage);
  window_->SetGeometry(1400, 900);
  window_->SetConnLimit(0);
  window_->SetCallBacks(
    [](unsigned) {},
    [this](unsigned connid, const std::string &message) { processMessage(connid, message); },
    [this](unsigned connid) { selected_.erase(connid); });

  timer_ = new DelphesSummary7Timer(this);
  timer_->TurnOn();
}

DelphesSummary7::~DelphesSummary7()
{
  if(timer_) timer_->TurnOff();
  delete timer_;
  for(auto &c : collections_)
    for(int v = 0; v < 3; ++v)
    {
      delete c.sample.all[v];
      delete c.sample.leading[v];
      delete c.sample.subleading[v];
    }
  delete reader_;
  delete chain_;
}

//------------------------------------------------------------------------------

void DelphesSummary7::SetEvent(Long64_t event, Long64_t entries)
{
  std::lock_guard<std::mutex> lock(eventMutex_);
  event_ = event;
  entries_ = entries;
  for(auto &c : collections_)
  {
    c.event = c.branch->GetVectors();
    std::sort(c.event.begin(), c.event.end(), byPt);
  }
  eventChanged_ = true;
}

void DelphesSummary7::Show()
{
  window_->Show();
}

void DelphesSummary7::FillSample()
{
  sampleNext_ = 0;
  sampleRunning_ = true;
  while(sampleRunning_) fillSampleChunk(1.);
  for(auto &conn : selected_) sendPlot(conn.first);
}

//------------------------------------------------------------------------------
// main thread: timer and messages from the page
//------------------------------------------------------------------------------

void DelphesSummary7::poll()
{
  if(showRequested_.exchange(false)) Show();

  if(sampleRunning_)
  {
    fillSampleChunk(0.05);
    // live update of the plots while filling, and at the end
    double t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if(!sampleRunning_ || t - lastProgress_ > 1.)
    {
      lastProgress_ = t;
      sendProgress();
      for(auto &conn : selected_) sendPlot(conn.first);
    }
  }

  if(eventChanged_.exchange(false))
  {
    for(auto &conn : selected_)
    {
      sendEvent(conn.first);
      sendPlot(conn.first);
    }
  }
}

void DelphesSummary7::processMessage(unsigned connid, const std::string &message)
{
  if(message == "READY")
  {
    selected_[connid] = collections_.empty() ? "" : collections_.front().name;
    sendInit(connid);
    sendEvent(connid);
  }
  else if(message.compare(0, 7, "SELECT:") == 0)
  {
    selected_[connid] = message.substr(7);
    sendPlot(connid);
  }
  else if(message.compare(0, 5, "GOTO:") == 0)
  {
    Long64_t event = std::atoll(message.c_str() + 5);
    if(gotoEvent_ && event >= 0 && event < entries_) gotoEvent_(event);
  }
  else if(message == "FILL" && !sampleRunning_)
  {
    for(auto &c : collections_)
    {
      c.sample.multiplicity.clear();
      for(int v = 0; v < 3; ++v)
      {
        c.sample.all[v]->Reset();
        c.sample.leading[v]->Reset();
        c.sample.subleading[v]->Reset();
      }
    }
    sampleNext_ = 0;
    sampleEvents_ = 0;
    sampleRunning_ = true;
    sendProgress();
  }
}

void DelphesSummary7::fillSampleChunk(double seconds)
{
  auto start = std::chrono::steady_clock::now();
  auto elapsed = [&]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); };
  Long64_t entries = reader_->GetEntries();
  while(sampleNext_ < entries && elapsed() < seconds)
  {
    reader_->ReadEntry(sampleNext_++);
    for(auto &c : collections_)
    {
      std::vector<TLorentzVector> vectors = DelphesBranchBase7::Vectors(c.sampleBranch);
      std::sort(vectors.begin(), vectors.end(), byPt);
      size_t n = vectors.size();
      if(c.sample.multiplicity.size() <= n) c.sample.multiplicity.resize(n + 1, 0);
      ++c.sample.multiplicity[n];
      for(size_t i = 0; i < n; ++i)
      {
        double values[3] = {vectors[i].Pt(), vectors[i].Pt() > 0. ? vectors[i].Eta() : 0., vectors[i].Phi()};
        for(int v = 0; v < 3; ++v)
        {
          c.sample.all[v]->Fill(values[v]);
          if(i == 0) c.sample.leading[v]->Fill(values[v]);
          if(i == 1) c.sample.subleading[v]->Fill(values[v]);
        }
      }
    }
  }
  sampleEvents_ = sampleNext_;
  if(sampleNext_ >= entries) sampleRunning_ = false;
}

//------------------------------------------------------------------------------
// messages to the page
//------------------------------------------------------------------------------

void DelphesSummary7::sendInit(unsigned connid)
{
  std::ostringstream out;
  out << "INIT:{\"collections\":[";
  for(size_t i = 0; i < collections_.size(); ++i)
  {
    const Collection &c = collections_[i];
    out << (i ? "," : "") << "{\"name\":" << jsonString(c.name) << ",\"type\":" << jsonString(c.type)
        << ",\"color\":" << jsonString(hexColor(c.color))
        << ",\"shown\":" << (c.branch->GetElement()->GetRnrSelf() ? "true" : "false") << "}";
  }
  out << "]}";
  window_->Send(connid, out.str());
}

std::string DelphesSummary7::eventJson()
{
  std::lock_guard<std::mutex> lock(eventMutex_);
  std::ostringstream out;
  out << "{\"event\":" << event_ << ",\"entries\":" << entries_ << ",\"sample\":{\"done\":" << sampleEvents_
      << ",\"total\":" << reader_->GetEntries() << ",\"running\":" << (sampleRunning_ ? "true" : "false") << "},\"collections\":[";
  for(size_t i = 0; i < collections_.size(); ++i)
  {
    const Collection &c = collections_[i];
    std::string label;
    if(c.type == "MissingET")
      label = c.event.empty() ? "" : Form("%.1f GeV", c.event.front().Pt());
    else
      label = Form("%zu", c.event.size());
    out << (i ? "," : "") << "{\"name\":" << jsonString(c.name) << ",\"label\":" << jsonString(label) << "}";
  }
  out << "]}";
  return out.str();
}

void DelphesSummary7::sendEvent(unsigned connid)
{
  window_->Send(connid, "EVENT:" + eventJson());
}

void DelphesSummary7::sendProgress()
{
  std::ostringstream out;
  out << "PROGRESS:{\"done\":" << sampleEvents_ << ",\"total\":" << reader_->GetEntries()
      << ",\"running\":" << (sampleRunning_ ? "true" : "false") << "}";
  for(auto &conn : selected_) window_->Send(conn.first, out.str());
}

void DelphesSummary7::sendPlot(unsigned connid)
{
  auto it = selected_.find(connid);
  if(it == selected_.end()) return;
  for(auto &c : collections_)
    if(c.name == it->second) window_->Send(connid, "PLOT:" + c.name + "\n" + plotsJson(c));
}

//------------------------------------------------------------------------------
// the plots of a collection, drawn and styled by JSROOT in the page:
// {"plots":[{"title":..,"logx":..,"logy":..,"objects":[{"role":..,"json":..}]}]}
// The objects are given in drawing order, the first one defines the frame.
//------------------------------------------------------------------------------

std::string DelphesSummary7::plotsJson(const Collection &c)
{
  std::vector<TLorentzVector> event;
  {
    std::lock_guard<std::mutex> lock(eventMutex_);
    event = c.event;
  }
  const bool met = (c.type == "MissingET");
  const bool haveSample = sampleEvents_ > 0;
  const double scale = haveSample ? 1. / sampleEvents_ : 1.;

  auto prepare = [](TH1 *h, const char *xtitle, const char *ytitle) {
    h->SetStats(0);
    h->SetDirectory(nullptr);
    for(TAxis *axis : {h->GetXaxis(), h->GetYaxis()})
    {
      axis->SetLabelFont(42);
      axis->SetTitleFont(42);
      axis->SetLabelSize(0.045);
      axis->SetTitleSize(0.05);
    }
    h->GetXaxis()->SetTitle(xtitle);
    h->GetYaxis()->SetTitle(ytitle);
    h->GetXaxis()->SetTitleOffset(1.2);
    h->GetYaxis()->SetTitleOffset(1.4);
  };
  auto object = [](const char *role, TObject *obj) {
    std::string json = TBufferJSON::ToJSON(obj, TBufferJSON::kNoSpaces).Data();
    return std::string("{\"role\":\"") + role + "\",\"json\":" + jsonString(json) + "}";
  };

  std::ostringstream out;
  out << "{\"plots\":[";
  bool firstPlot = true;
  auto beginPlot = [&](const char *title, bool logx, bool logy) {
    out << (firstPlot ? "" : ",") << "{\"title\":" << jsonString(title) << ",\"logx\":" << (logx ? "true" : "false")
        << ",\"logy\":" << (logy ? "true" : "false") << ",\"objects\":[";
    firstPlot = false;
  };

  // multiplicity
  if(!met)
  {
    Long64_t nmax = std::max<Long64_t>(event.size(), c.sample.multiplicity.empty() ? 0 : c.sample.multiplicity.size() - 1);
    int width = std::max<Long64_t>(1, (nmax + 50) / 50);
    int nbins = std::max<int>(10, nmax / width + 2); // at least 0-9: integer ticks
    TH1D h("multiplicity", "", nbins, -0.5, nbins * width - 0.5);
    prepare(&h, "objects per event", haveSample ? "fraction of events" : "events");
    beginPlot("Multiplicity", false, false);
    if(haveSample)
    {
      for(size_t n = 0; n < c.sample.multiplicity.size(); ++n) h.Fill(n, c.sample.multiplicity[n] * scale);
      h.SetMaximum(1.2 * h.GetMaximum());
      h.SetMinimum(0.);
      TLine line(event.size(), 0., event.size(), h.GetMaximum());
      out << object("all", &h) << "," << object("marker", &line);
    }
    else
    {
      h.Fill(event.size());
      h.SetMaximum(1.5);
      h.SetMinimum(0.);
      out << object("event", &h);
    }
    out << "]}";
  }

  // pT, eta, phi (MET: value and phi)
  const char *titles[3] = {met ? "Missing transverse energy" : "Transverse momentum", "Pseudorapidity", "Azimuth"};
  const char *xtitles[3] = {met ? "E_{T}^{miss} [GeV]" : "p_{T} [GeV]", "#eta", "#phi"};
  for(int v = 0; v < 3; ++v)
  {
    if(met && v == 1) continue;

    std::vector<std::pair<const char *, TH1D *>> drawn;
    if(haveSample)
    {
      TH1D *sample[3] = {c.sample.all[v], c.sample.leading[v], c.sample.subleading[v]};
      const char *roles[3] = {"all", "leading", "subleading"};
      for(int s = 0; s < 3; ++s)
      {
        if(met && s > 0) continue; // a single object per event
        auto *h = (TH1D *)sample[s]->Clone();
        h->Scale(scale);
        prepare(h, xtitles[v], "objects / event");
        drawn.emplace_back(roles[s], h);
      }
    }
    auto *hEvent = (TH1D *)c.sample.all[v]->Clone();
    hEvent->Reset();
    for(auto &vec : event)
    {
      double values[3] = {vec.Pt(), vec.Pt() > 0. ? vec.Eta() : 0., vec.Phi()};
      hEvent->Fill(values[v]);
    }
    prepare(hEvent, xtitles[v], haveSample ? "objects / event" : "objects");
    drawn.emplace_back("event", hEvent);

    // common range: the filled bins of all histograms, with some headroom
    double ymax = 0., ymin = 1e300;
    int first = hEvent->GetNbinsX(), last = 1;
    for(auto &d : drawn)
      for(int b = 1; b <= d.second->GetNbinsX(); ++b)
      {
        double y = d.second->GetBinContent(b);
        if(y <= 0.) continue;
        ymax = std::max(ymax, y);
        ymin = std::min(ymin, y);
        first = std::min(first, b);
        last = std::max(last, b);
      }
    TH1D *frame = drawn.front().second;
    if(ymax > 0.)
    {
      if(v == 0)
      {
        frame->SetMaximum(ymax * 5.);
        frame->SetMinimum(ymin * 0.3);
        frame->GetXaxis()->SetRange(std::max(1, first - 1), std::min(frame->GetNbinsX(), last + 1));
      }
      else
      {
        frame->SetMaximum(ymax * 1.25);
        frame->SetMinimum(0.);
      }
      if(v == 1)
      {
        double eta = std::max(std::fabs(frame->GetXaxis()->GetBinLowEdge(first)), std::fabs(frame->GetXaxis()->GetBinUpEdge(last)));
        frame->GetXaxis()->SetRangeUser(-eta, eta);
      }
    }

    beginPlot(titles[v], v == 0, v == 0);
    for(size_t i = 0; i < drawn.size(); ++i)
    {
      out << (i ? "," : "") << object(drawn[i].first, drawn[i].second);
      delete drawn[i].second;
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}
