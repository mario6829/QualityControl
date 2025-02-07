// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// \file   RawData.cxx
/// \author Sean Murray
///

#include "TCanvas.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TMath.h"

#include "QualityControl/QcInfoLogger.h"
#include "TRD/RawData.h"
#include "DataFormatsTRD/RawDataStats.h"
#include "DataFormatsTRD/Digit.h"
#include "DataFormatsTRD/Tracklet64.h"
#include "DataFormatsTRD/TriggerRecord.h"
#include "DataFormatsTRD/HelperMethods.h"
#include <Framework/InputRecord.h>
#include <Framework/InputRecordWalker.h>
#include <gsl/span>

using namespace o2::trd;
using namespace o2::trd::constants;

namespace o2::quality_control_modules::trd
{

RawData::~RawData()
{
}

void RawData::buildHistograms()
{
  std::array<std::string, 10> linkerrortitles = { "Count of Link had no errors during tf",
                                                  "Count of # times Linkerrors 0x1 seen per tf",
                                                  "Count of # time Linkerrors 0x2 seen per tf",
                                                  "Count of any Linkerror seen during tf",
                                                  "Link was seen with no data (empty) in a tf",
                                                  "Link was seen with data during a tf",
                                                  "Links seen with corrupted data during tf",
                                                  "Links seen with out corrupted data during tf",
                                                  "Accepted Data volume on link",
                                                  "Rejected Data volume on link" };

  mStats = new TH1F("stats", "Data reader statistics;;counts", 5, 0, 5);
  getObjectsManager()->startPublishing(mStats);
  getObjectsManager()->setDefaultDrawOptions(mStats->GetName(), "logy");
  mStats->GetXaxis()->SetBinLabel(1, "nTF");
  mStats->GetXaxis()->SetBinLabel(2, "nTrig");
  mStats->GetXaxis()->SetBinLabel(3, "nCalTrig");
  mStats->GetXaxis()->SetBinLabel(4, "nTrklts");
  mStats->GetXaxis()->SetBinLabel(5, "nDigits");
  mStats->LabelsOption("v");
  mDataAcceptance = new TH1F("dataacceptance", "Data Accepted and Rejected;;MBytes", 2, -0.5, 1.5);
  getObjectsManager()->startPublishing(mDataAcceptance);
  mDataAcceptance->GetXaxis()->SetBinLabel(1, "Accepted");
  mDataAcceptance->GetXaxis()->SetBinLabel(2, "Rejected");
  constexpr int nLogBins = 100;
  float xBins[nLogBins + 1];
  float xBinLogMin = 1.f;
  float xBinLogMax = 6.f;
  float logBinWidth = (xBinLogMax - xBinLogMin) / nLogBins;
  for (int iBin = 0; iBin <= nLogBins; ++iBin) {
    xBins[iBin] = TMath::Power(10, xBinLogMin + iBin * logBinWidth);
  }
  mTimeFrameTime = new TH1F("timeframetime", "Time taken per time frame;Time taken [us];Counts", nLogBins, xBins);
  getObjectsManager()->startPublishing(mTimeFrameTime);
  getObjectsManager()->setDefaultDrawOptions(mTimeFrameTime->GetName(), "logx");
  mTrackletParsingTime = new TH1F("tracklettime", "Time taken per tracklet block;Time taken [us];Counts", nLogBins, xBins);
  getObjectsManager()->startPublishing(mTrackletParsingTime);
  getObjectsManager()->setDefaultDrawOptions(mTrackletParsingTime->GetName(), "logx");
  mDigitParsingTime = new TH1F("digittime", "Time taken per digit block;Time taken [us];Counts", nLogBins, xBins);
  getObjectsManager()->startPublishing(mDigitParsingTime);
  getObjectsManager()->setDefaultDrawOptions(mDigitParsingTime->GetName(), "logx");
  mDataVersionsMajor = new TH1F("dataversionsmajor", "Data versions major seen in the data (half chamber header required);Version;Counts", 256, -0.5, 255.5);
  getObjectsManager()->startPublishing(mDataVersionsMajor);
  mParsingErrors = new TH1F("parseerrors", "Parsing Errors seen in data;;Counts", TRDLastParsingError, 0, TRDLastParsingError);
  getObjectsManager()->startPublishing(mParsingErrors);
  getObjectsManager()->setDefaultDrawOptions(mParsingErrors->GetName(), "logy");

  mDataVolumePerHalfChamber = new TH2F("datavolumeperhalfchamber", "Data sizes from HalfCRU header;Half Chamber ID;Data Volume [kB/TF]", 1080, -0.5, 1079.5, 1000, 0, 100);
  getObjectsManager()->startPublishing(mDataVolumePerHalfChamber);
  getObjectsManager()->setDefaultDrawOptions("datavolumeperhalfchamber", "COLZ");
  getObjectsManager()->setDisplayHint(mDataVolumePerHalfChamber->GetName(), "logz");

  mDataVolumePerSector = new TH2F("datavolumepersector", "Data sizes from HalfCRU header;Sector;Data Volume [kB/TF]", 18, -0.5, 17.5, 1000, 0, 100);
  mDataVolumePerSector->SetStats(0);
  getObjectsManager()->startPublishing(mDataVolumePerSector);
  getObjectsManager()->setDefaultDrawOptions("datavolumepersector", "COLZ");
  getObjectsManager()->setDisplayHint(mDataVolumePerSector->GetName(), "logz");

  for (int count = 0; count < o2::trd::TRDLastParsingError; ++count) {
    std::string label = fmt::format("parsingerrors_{0}", count);
    std::string title = ParsingErrorsString.at(count);
    TH2F* h = new TH2F(label.c_str(), title.c_str(), 36, 0, 36, 30, 0, 30);
    mParsingErrors2d[count] = h;
    getObjectsManager()->startPublishing(h);
    getObjectsManager()->setDefaultDrawOptions(h->GetName(), "COLZ");
    getObjectsManager()->setDisplayHint(h->GetName(), "logz");
  }
  for (int count = 0; count < 10; ++count) {
    std::string label = fmt::format("linkstatus_{0}", count);
    std::string title = linkerrortitles[count];
    TH2F* h = new TH2F(label.c_str(), title.c_str(), 36, 0, 36, 30, 0, 30);
    mLinkErrors[count] = h;
    getObjectsManager()->startPublishing(h);
    getObjectsManager()->setDefaultDrawOptions(h->GetName(), "COLZ");
    getObjectsManager()->setDisplayHint(h->GetName(), "logz");
  }

  for (int i = 0; i < TRDLastParsingError; ++i) {
    std::string label = fmt::format("{1:.3}_{0}", i, ParsingErrorsString.at(i));
    mParsingErrors->GetXaxis()->SetBinLabel(i + 1, label.c_str());
  }
  mParsingErrors->LabelsOption("v");
  for (int count = 0; count < o2::trd::TRDLastParsingError; ++count) {
    TH2F* h = mParsingErrors2d[count];
    h->GetXaxis()->SetTitle("Sector_Side");
    h->GetXaxis()->CenterTitle(kTRUE);
    h->GetYaxis()->SetTitle("Stack_Layer");
    h->GetYaxis()->CenterTitle(kTRUE);
    for (int s = 0; s < NSTACK; ++s) {
      for (int l = 0; l < NLAYER; ++l) {
        std::string label = fmt::format("{0}_{1}", s, l);
        int pos = s * NLAYER + l + 1;
        h->GetYaxis()->SetBinLabel(pos, label.c_str());
      }
      getObjectsManager()->setDisplayHint(h->GetName(), "logz");
    }
    for (int sm = 0; sm < NSECTOR; ++sm) {
      for (int side = 0; side < 2; ++side) {
        std::string label = fmt::format("{0}_{1}", sm, side == 0 ? "A" : "B");
        int pos = sm * 2 + side + 1;
        h->GetXaxis()->SetBinLabel(pos, label.c_str());
      }
    }
  }
  for (int count = 0; count < 10; ++count) {
    TH2F* h = mLinkErrors[count];
    h->GetXaxis()->SetTitle("Sector_Side");
    h->GetXaxis()->CenterTitle(kTRUE);
    h->GetYaxis()->SetTitle("Stack_Layer");
    h->GetYaxis()->CenterTitle(kTRUE);
    getObjectsManager()->setDisplayHint(h->GetName(), "logz");
    for (int s = 0; s < NSTACK; ++s) {
      for (int l = 0; l < NLAYER; ++l) {
        std::string label = fmt::format("{0}_{1}", s, l);
        int pos = s * NLAYER + l + 1;
        h->GetYaxis()->SetBinLabel(pos, label.c_str());
      }
    }
    for (int sm = 0; sm < NSECTOR; ++sm) {
      for (int side = 0; side < 2; ++side) {
        std::string label = fmt::format("{0}_{1}", sm, side == 0 ? "A" : "B");
        int pos = sm * 2 + side + 1;
        h->GetXaxis()->SetBinLabel(pos, label.c_str());
      }
    }
  }
}

void RawData::initialize(o2::framework::InitContext& /*ctx*/)
{

  ILOG(Debug, Devel) << "initialize TRD RawData QC" << ENDM;
  if (auto param = mCustomParameters.find("fillHeaderVersionHist"); param != mCustomParameters.end()) {
    mCheckDigitHCHeaderVersion = std::stoi(param->second);
  }

  buildHistograms();
  ILOG(Info, Support) << "TRD RawData QC histograms built" << ENDM;

  // ILOG(Warning, Support) << "Metadata could not be added to " << mHistogram->GetName() << ENDM;
}

void RawData::startOfActivity(const Activity& activity)
{
  ILOG(Debug, Devel) << "startOfActivity " << activity.mId << ENDM;
  resetHistograms();
}

void RawData::startOfCycle()
{
  ILOG(Debug, Devel) << "startOfCycle" << ENDM;
}

void RawData::monitorData(o2::framework::ProcessingContext& ctx)
{

  auto rawdatastats = ctx.inputs().get<o2::trd::TRDDataCountersPerTimeFrame*>("rawstats");
  mStats->AddBinContent(1, 1);                             // count number of TFs seen
  mStats->AddBinContent(2, rawdatastats->mNTriggersTotal); // count total number of triggers seen
  mStats->AddBinContent(3, rawdatastats->mNTriggersCalib); // count total number of calibration triggers seen
  mStats->AddBinContent(4, rawdatastats->mTrackletsFound); // count total number of tracklets seen
  mStats->AddBinContent(5, rawdatastats->mDigitsFound);    // count total number of digits seen

  // data per TF per link.
  for (int hcid = 0; hcid < MAXHALFCHAMBER; ++hcid) {
    if (rawdatastats->mLinkWords[hcid] > 0) {
      int sec = hcid / NHCPERSEC;
      mDataVolumePerHalfChamber->Fill(hcid, rawdatastats->mLinkWords[hcid] / 32.f); // one link word is 32 bytes, we want to display in units of kB per TF
      mDataVolumePerSector->Fill(sec, rawdatastats->mLinkWords[hcid] / 32.f);
    }
  }

  // parsing errors
  for (int error = 0; error < TRDLastParsingError; ++error) {
    mParsingErrors->AddBinContent(error + 1, rawdatastats->mParsingErrors[error]);
  }
  for (auto e : rawdatastats->mParsingErrorsByLink) {
    int hcid = e / TRDLastParsingError;
    int errorIdx = e % TRDLastParsingError;
    int stackLayer = HelperMethods::getStack(hcid / 2) * NLAYER + HelperMethods::getLayer(hcid / 2);
    int sectorSide = (hcid / NHCPERSEC) * 2 + (hcid % 2);
    mParsingErrors2d[errorIdx]->Fill(sectorSide, stackLayer);
  }
  for (int hcid = 0; hcid < MAXHALFCHAMBER; ++hcid) {
    if (rawdatastats->mParsingOK[hcid] > 0) {
      int stackLayer = HelperMethods::getStack(hcid / 2) * NLAYER + HelperMethods::getLayer(hcid / 2);
      int sectorSide = (hcid / NHCPERSEC) * 2 + (hcid % 2);
      mParsingErrors2d[0]->SetBinContent(sectorSide + 1, stackLayer + 1, mParsingErrors2d[0]->GetBinContent(sectorSide + 1, stackLayer + 1) + rawdatastats->mParsingOK[hcid]);
    }
  }

  // link statistics
  for (int hcid = 0; hcid < MAXHALFCHAMBER; ++hcid) {
    int stackLayer = HelperMethods::getStack(hcid / 2) * NLAYER + HelperMethods::getLayer(hcid / 2);
    int sectorSide = (hcid / NHCPERSEC) * 2 + (hcid % 2);
    if (rawdatastats->mLinkErrorFlag[hcid] == 0) { //"Count of Link had no errors during tf",
      mLinkErrors[0]->Fill((double)sectorSide, (double)stackLayer);
    }
    if (rawdatastats->mLinkErrorFlag[hcid] & 0x1) { //"Count of # times Linkerrors 0x1 seen per tf",
      mLinkErrors[1]->Fill((double)sectorSide, (double)stackLayer, rawdatastats->mLinkErrorFlag[hcid]);
    }
    if (rawdatastats->mLinkErrorFlag[hcid] & 0x2) { //"Count of # time Linkerrors 0x2 seen per tf",
      mLinkErrors[2]->Fill((double)sectorSide, (double)stackLayer, rawdatastats->mLinkErrorFlag[hcid]);
    }
    if (rawdatastats->mLinkErrorFlag[hcid] != 0) { //"Count of any Linkerror seen during tf",
      mLinkErrors[3]->Fill((double)sectorSide, (double)stackLayer, rawdatastats->mLinkErrorFlag[hcid]);
    }
    if (rawdatastats->mLinkWordsRejected[hcid] + rawdatastats->mLinkWordsRead[hcid] == 0) {
      mLinkErrors[4]->Fill((double)sectorSide, (double)stackLayer);
    }
    if (rawdatastats->mLinkWordsRead[hcid] > 0) { //"Link was seen with data during a tf",
      mLinkErrors[5]->Fill((double)sectorSide, (double)stackLayer, rawdatastats->mLinkWordsRead[hcid]);
    }
    if (rawdatastats->mLinkWordsRejected[hcid] > 0) { //"Links seen with corrupted data during tf"
      mLinkErrors[6]->Fill((double)sectorSide, (double)stackLayer);
    }
    if (rawdatastats->mLinkWordsRejected[hcid] == 0) { //"Links seen with out corrupted data during tf", "",
      mLinkErrors[7]->Fill((double)sectorSide, (double)stackLayer);
    }
    if (rawdatastats->mLinkWordsRead[hcid] != 0) {
      ILOG(Debug, Devel) << "Accepted Data volume on link: " << rawdatastats->mLinkWordsRead[hcid] << ENDM;
      mLinkErrors[8]->Fill((double)sectorSide, (double)stackLayer, rawdatastats->mLinkWordsRead[hcid]);
      mDataAcceptance->AddBinContent(1, (4.f * rawdatastats->mLinkWordsRead[hcid]) / (1024.f * 1024.f)); // each word is 32 bits
    }
    if (rawdatastats->mLinkWordsRejected[hcid] != 0) {
      ILOG(Debug, Devel) << "Rejected Data volume on link: " << rawdatastats->mLinkWordsRejected[hcid] << ENDM;
      mLinkErrors[9]->Fill((double)sectorSide, (double)stackLayer, rawdatastats->mLinkWordsRejected[hcid]);
      mDataAcceptance->AddBinContent(2, (4.f * rawdatastats->mLinkWordsRejected[hcid]) / (1024.f * 1024.f)); // each word is 32 bits
    }
  }
  // time graphs
  mTimeFrameTime->Fill(rawdatastats->mTimeTaken);
  mDigitParsingTime->Fill(rawdatastats->mTimeTakenForDigits);
  mTrackletParsingTime->Fill(rawdatastats->mTimeTakenForTracklets);

  if (mCheckDigitHCHeaderVersion) {
    for (int i = 0; i < rawdatastats->mDataFormatRead.size(); ++i) {
      mDataVersionsMajor->Fill(i, rawdatastats->mDataFormatRead[i]);
    }
  }
}

void RawData::endOfCycle()
{
  ILOG(Debug, Devel) << "endOfCycle" << ENDM;
}

void RawData::endOfActivity(const Activity& /*activity*/)
{
  ILOG(Debug, Devel) << "endOfActivity" << ENDM;
}

void RawData::reset()
{
  // clean all the monitor objects here

  ILOG(Debug, Devel) << "Resetting the histograms" << ENDM;
  resetHistograms();
}
void RawData::resetHistograms()
{
  ILOG(Debug, Devel) << "Resetting the histograms " << ENDM;
  for (auto hist : mLinkErrors) {
    hist->Reset();
  }
  for (auto hist : mParsingErrors2d) {
    hist->Reset();
  }
  mStats->Reset();
  mDataAcceptance->Reset();
  mTimeFrameTime->Reset();
  mTrackletParsingTime->Reset();
  mDigitParsingTime->Reset();
  mDataVersionsMajor->Reset();
  mParsingErrors->Reset();
  mDataVolumePerHalfChamber->Reset();
  mDataVolumePerSector->Reset();
}

} // namespace o2::quality_control_modules::trd
