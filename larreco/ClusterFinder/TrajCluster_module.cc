/**
 * @file   TrajCluster_module.cc
 * @brief  Cluster finder using trajectories
 * @author Bruce Baller (baller@fnal.gov)
 * 
*
 */


// C/C++ standard libraries
#include <string>
#include <utility> // std::unique_ptr<>

// Framework libraries
#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "canvas/Utilities/InputTag.h"
#include "art/Framework/Services/Optional/TFileService.h"

//LArSoft includes
#include "larreco/RecoAlg/TrajClusterAlg.h"
#include "larreco/RecoAlg/TCAlg/DataStructs.h"
#include "lardataobj/RecoBase/SpacePoint.h"
#include "lardataobj/AnalysisBase/CosmicTag.h"

//root includes
#include "TTree.h"

// ... more includes in the implementation section

namespace cluster {
  /**
   * @brief Produces clusters by the TrajCluster algorithm
   * 
   * Configuration parameters
   * -------------------------
   * 
   * - *HitFinderModuleLabel* (InputTag, mandatory): label of the hits to be
   *   used as input (usually the label of the producing module is enough)
   * - *TrajClusterAlg* (parameter set, mandatory): full configuration for
   *   TrajClusterAlg algorithm
   * 
   */
  class TrajCluster: public art::EDProducer {
    
  public:
    explicit TrajCluster(fhicl::ParameterSet const & pset);
    
    void reconfigure(fhicl::ParameterSet const & pset) ;
    void produce(art::Event & evt) override;
    void beginJob() override;
    void endJob() override;
    
  private:
//    bool SortHits(HitLoc const& h1, HitLoc const& h2);

    std::unique_ptr<tca::TrajClusterAlg> fTCAlg; // define TrajClusterAlg object
    TTree* showertree;
//    TTree* crtree;

    art::InputTag fHitModuleLabel;
    art::InputTag fSlicerModuleLabel;
    
    bool fDoWireAssns;
    bool fDoRawDigitAssns;
    
  }; // class TrajCluster
  
} // namespace cluster

//******************************************************************************
//*** implementation
//***

// C/C++ standard libraries
#include <vector>
#include <memory> // std::move()

// Framework libraries
#include "canvas/Utilities/Exception.h"
#include "art/Framework/Principal/Handle.h"
#include "canvas/Persistency/Common/Assns.h"

//LArSoft includes
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"
#include "lardata/Utilities/AssociationUtil.h"
#include "lardata/ArtDataHelper/HitCreator.h" // recob::HitCollectionAssociator
#include "lardataobj/RecoBase/Cluster.h"
#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/RecoBase/EndPoint2D.h"
#include "lardataobj/RecoBase/Vertex.h"
#include "lardataobj/RecoBase/Shower.h"


namespace cluster {
  
  struct HitLoc {
    unsigned int index; // index of this entry in a sort vector
    unsigned int ctp;   // encoded Cryostat, TPC and Plane
    unsigned int wire;  
    int tick;           // hit StartTick using typedef int TDCtick_t in RawTypes.h
    short localIndex;   // defined in Hit.h
  };

  //----------------------------------------------------------------------------
  bool SortHits(HitLoc const& h1, HitLoc const& h2)
  {
    // sort by hit location (Cryostat, TPC, Plane, Wire, StartTick, hit LocalIndex)
    if(h1.ctp != h2.ctp) return h1.ctp < h2.ctp;
    if(h1.wire != h2.wire) return h1.wire < h2.wire;
    if(h1.tick != h2.tick) return h1.tick < h2.tick;
    return h1.localIndex < h2.localIndex;
  } // SortHits

  //----------------------------------------------------------------------------
  void TrajCluster::reconfigure(fhicl::ParameterSet const & pset)
  {
    // this trick avoids double configuration on construction
    if (fTCAlg)
      fTCAlg->reconfigure(pset.get< fhicl::ParameterSet >("TrajClusterAlg"));
    else {
      fTCAlg.reset(new tca::TrajClusterAlg(pset.get< fhicl::ParameterSet >("TrajClusterAlg")));
    }


    fHitModuleLabel = "NA";
    if(pset.has_key("HitModuleLabel")) fHitModuleLabel = pset.get<art::InputTag>("HitModuleLabel");
    fSlicerModuleLabel = "NA";
    if(pset.has_key("SlicerModuleLabel")) fSlicerModuleLabel = pset.get<art::InputTag>("SlicerModuleLabel");
    
    if(fHitModuleLabel != "NA" && fSlicerModuleLabel != "NA") {
      throw art::Exception(art::errors::Configuration)<<"Error: you specified both sliced hits '"<<fSlicerModuleLabel.label()<<"' and un-sliced hits '"<<fHitModuleLabel.label()<<"' for input. ";
    }

    fDoWireAssns = pset.get<bool>("DoWireAssns",true);
    fDoRawDigitAssns = pset.get<bool>("DoRawDigitAssns",true);

  } // TrajCluster::reconfigure()
  
  //----------------------------------------------------------------------------
  TrajCluster::TrajCluster(fhicl::ParameterSet const& pset) {
    
    reconfigure(pset);
    
    // let HitCollectionAssociator declare that we are going to produce
    // hits and associations with wires and raw digits
    // (with no particular product label)
    recob::HitCollectionAssociator::declare_products(*this,"",fDoWireAssns,fDoRawDigitAssns);
    
    produces< std::vector<recob::Cluster> >();
    produces< std::vector<recob::Vertex> >();
    produces< std::vector<recob::EndPoint2D> >();
    produces< std::vector<recob::Shower> >();
    produces< art::Assns<recob::Cluster, recob::Hit> >();
    produces< art::Assns<recob::Cluster, recob::EndPoint2D, unsigned short> >();
    produces< art::Assns<recob::Cluster, recob::Vertex, unsigned short> >();
    produces< art::Assns<recob::Shower, recob::Hit> >();
    
    produces< std::vector<recob::PFParticle> >();
    produces< art::Assns<recob::PFParticle, recob::Cluster> >();
    produces< art::Assns<recob::PFParticle, recob::Shower> >();
    produces< art::Assns<recob::PFParticle, recob::Vertex> >();

    produces< std::vector<anab::CosmicTag>>();
    produces< art::Assns<recob::PFParticle, anab::CosmicTag>>();
  } // TrajCluster::TrajCluster()

  //----------------------------------------------------------------------------
  void TrajCluster::beginJob()
  {
    art::ServiceHandle<art::TFileService> tfs;

    showertree = tfs->make<TTree>("showervarstree", "showerVarsTree");
    fTCAlg->DefineShTree(showertree);
//    crtree = tfs->make<TTree>("crtree", "Cosmic removal variables");
//    fTCAlg->DefineCRTree(crtree);
  }
  
  //----------------------------------------------------------------------------
  void TrajCluster::endJob()
  {
    std::vector<unsigned int> const& fAlgModCount = fTCAlg->GetAlgModCount();
    std::vector<std::string> const& fAlgBitNames = fTCAlg->GetAlgBitNames();
    if(fAlgBitNames.size() != fAlgModCount.size()) return;
    mf::LogVerbatim myprt("TC");
    myprt<<"TrajCluster algorithm counts\n";
    unsigned short icol = 0;
    for(unsigned short ib = 0; ib < fAlgModCount.size(); ++ib) {
      if(ib == tca::kKilled) continue;
      myprt<<std::left<<std::setw(16)<<fAlgBitNames[ib]<<std::right<<std::setw(10)<<fAlgModCount[ib]<<" ";
      ++icol;
      if(icol == 4) { myprt<<"\n"; icol = 0; }
    } // ib
  } // endJob
  
  //----------------------------------------------------------------------------
  void TrajCluster::produce(art::Event & evt)
  {
    // Get a single hit collection from a HitsModuleLabel or multiple sets of "sliced" hits
    // (aka clusters of hits that are close to each other in 3D) from a SlicerModuleLabel. 
    // A pointer to the full hit collection is passed to TrajClusterAlg. The hits that are 
    // in each slice are tracked to find 2D trajectories (that become clusters), 
    // 2D vertices (EndPoint2D), 3D vertices, PFParticles and Showers. These data products
    // are then collected and written to the event. Each slice is considered as an independent
    // collection of hits with the additional requirement that all hits in a slice reside in
    // one TPC
    
    // Define a vector of indices into inputHits (= evt.allHits in TrajClusterAlg) 
    // for each slice for hits associated with 3D-matched PFParticles that were found 
    // with simple 3D clustering (else just the full collection)
    std::vector<std::vector<unsigned int>> slHitsVec;
    unsigned int nInputHits = 0;
    // get a handle for the hit collection
    auto inputHits = art::Handle<std::vector<recob::Hit>>();
    if(fSlicerModuleLabel != "NA") {
      // Expecting to find sliced hits from PFParticles -> Clusters -> Hits
      auto pfpsHandle = evt.getValidHandle<std::vector<recob::PFParticle>>(fSlicerModuleLabel);
      std::vector<art::Ptr<recob::PFParticle>> pfps;
      art::fill_ptr_vector(pfps, pfpsHandle);
      art::FindManyP <recob::Cluster> cluFromPfp(pfpsHandle, evt, fSlicerModuleLabel);
      auto clusHandle = evt.getValidHandle<std::vector<recob::Cluster>>(fSlicerModuleLabel);
      art::FindManyP <recob::Hit> hitFromClu(clusHandle, evt, fSlicerModuleLabel);
      if(evt.getByLabel(fSlicerModuleLabel, inputHits)) {
        // TODO: Ensure that all hits are in the same TPC. Create separate slices for
        // each TPC if that is not the case
        fTCAlg->SetInputHits(*inputHits);
        nInputHits = (*inputHits).size();
      } else {
        std::cout<<"Failed to get a hits handle for "<<fSlicerModuleLabel<<"\n";
        return;
      }
      for(size_t ii = 0; ii < pfps.size(); ++ii) {
        std::vector<unsigned int> slhits;
        auto& clus_in_pfp = cluFromPfp.at(ii);
        for(auto& clu : clus_in_pfp) {
          auto& hits_in_clu = hitFromClu.at(clu.key());
          for(auto& hit : hits_in_clu) slhits.push_back(hit.key());
//          std::cout<<"slice "<<slhits.size()<<" hits\n";
        } // clu
        if(slhits.size() > 2) slHitsVec.push_back(slhits);
      } // ii
    } else {
      // There was no pre-processing of the hits to define logical slices
      // so just consider all hits as one slice
      // pass a pointer to the full hit collection to TrajClusterAlg
      if(!evt.getByLabel(fHitModuleLabel, inputHits)) {
        std::cout<<"Failed to get a hits handle\n";
        return;
      }
      // This is a pointer to a vector of recob::Hits that exist in the event. The hits
      // are not copied.
      fTCAlg->SetInputHits(*inputHits);
      nInputHits = (*inputHits).size();
      slHitsVec.resize(1);
      slHitsVec[0].resize(nInputHits);
      for(unsigned int iht = 0; iht < nInputHits; ++iht) slHitsVec[0][iht] = iht;
    } // no input PFParticles
    
    if(nInputHits == 0) throw cet::exception("TrajClusterModule")<<"No input hits found";
    
    // do an exhaustive check to ensure that a hit only appears in one slice
    if(slHitsVec.size() > 1) {
      std::vector<bool> inSlice(nInputHits, false);
      unsigned short nHitsInSlices = 0;
      for(auto& slhits : slHitsVec) {
        for(unsigned short indx = 0; indx < slhits.size(); ++indx) {
          if(slhits[indx] > nInputHits - 1) throw cet::exception("TrajClusterModule")<<"Found an invalid slice reference to the input hit collection";
          if(inSlice[slhits[indx]]) throw cet::exception("TrajClusterModule")<<"Found a hit in two different slices";
          inSlice[slhits[indx]] = true;
          ++nHitsInSlices;
        } // indx
      } // slhits
      std::cout<<"Found "<<slHitsVec.size()<<" slices, "<<nInputHits<<" input hits and "<<nHitsInSlices<<" hits in slices\n";
    } // > 1 slice

    // First sort the hits in each slice and then reconstruct
    for(auto& slhits : slHitsVec) {
      // sort the slice hits by Cryostat, TPC, Wire, Plane, Start Tick and LocalIndex.
      // This assumes that hits with larger LocalIndex are at larger Tick.
      std::vector<HitLoc> sortVec(slhits.size());
      bool badHit = false;
      for(unsigned short indx = 0; indx < slhits.size(); ++indx) {
        if(slhits[indx] > nInputHits - 1) {
          badHit = true;
          break;
        }
        auto& hit = (*inputHits)[slhits[indx]];
        sortVec[indx].index = indx;
        sortVec[indx].ctp = tca::EncodeCTP(hit.WireID());
        sortVec[indx].wire = hit.WireID().Wire;
        sortVec[indx].tick = hit.StartTick();
        sortVec[indx].localIndex = hit.LocalIndex();
      } // iht
      if(badHit) {
        std::cout<<"TrajCluster found an invalid slice reference to the input hit collection. Ignoring this slice.\n";
        continue;
      }
      std::sort(sortVec.begin(), sortVec.end(), SortHits);
      // make a temporary copy of slhits
      auto tmp = slhits;
      // put slhits into proper order
      for(unsigned short ii = 0; ii < slhits.size(); ++ii) slhits[ii] = tmp[sortVec[ii].index];
      // clear the temp vector
      tmp.resize(0);
      // reconstruct using the hits in this slice. The data products are stored internally in
      // TrajCluster data structs.
      fTCAlg->RunTrajClusterAlg(slhits);
    } // slhit
    
    // Vectors to hold all data products that will go into the event
    std::vector<recob::Hit> hitCol;       // output hit collection
    std::vector<recob::Cluster> clsCol;
    std::vector<recob::PFParticle> pfpCol;
    std::vector<recob::Vertex> vx3Col;
    std::vector<recob::EndPoint2D> vx2Col;
    std::vector<recob::Shower> shwCol;
    std::vector<anab::CosmicTag> ctCol;
    // a vector to correlate inputHits with hitCol
    std::vector<unsigned int> newIndex(nInputHits, UINT_MAX);
    
    // assns for those data products
    // Cluster -> ...
    std::unique_ptr<art::Assns<recob::Cluster, recob::Hit>> 
      cls_hit_assn(new art::Assns<recob::Cluster, recob::Hit>);
    // unsigned short is the end to which a vertex is attached
    std::unique_ptr<art::Assns<recob::Cluster, recob::EndPoint2D, unsigned short>>  
      cls_vx2_assn(new art::Assns<recob::Cluster, recob::EndPoint2D, unsigned short>);
    std::unique_ptr<art::Assns<recob::Cluster, recob::Vertex, unsigned short>>  
      cls_vx3_assn(new art::Assns<recob::Cluster, recob::Vertex, unsigned short>);
    // Shower -> ...
    std::unique_ptr<art::Assns<recob::Shower, recob::Hit>>
      shwr_hit_assn(new art::Assns<recob::Shower, recob::Hit>);
    // PFParticle -> ...
    std::unique_ptr<art::Assns<recob::PFParticle, recob::Cluster>>
      pfp_cls_assn(new art::Assns<recob::PFParticle, recob::Cluster>);
    std::unique_ptr<art::Assns<recob::PFParticle, recob::Shower>>
      pfp_shwr_assn(new art::Assns<recob::PFParticle, recob::Shower>);
    std::unique_ptr<art::Assns<recob::PFParticle, recob::Vertex>> 
      pfp_vtx_assn(new art::Assns<recob::PFParticle, recob::Vertex>);
    std::unique_ptr<art::Assns<recob::PFParticle, anab::CosmicTag>>
      pfp_cos_assn(new art::Assns<recob::PFParticle, anab::CosmicTag>);

    unsigned short nSlices = fTCAlg->GetSlicesSize();
    // define a hit collection begin index to pass to CreateAssn for each cluster
    unsigned int hitColBeginIndex = 0;
    for(unsigned short isl = 0; isl < nSlices; ++isl) {
      auto& slc = fTCAlg->GetSlice(isl);
      // See if there was a serious reconstruction failure that made the slice invalid
      if(!slc.isValid) continue;
      // Convert the tjs to clusters
      bool badSlice = false;
      for(auto& tj : slc.tjs) {
        if(tj.AlgMod[tca::kKilled]) continue;
        float sumChg = 0;
        float sumADC = 0;
        hitColBeginIndex = hitCol.size();
//        std::cout<<"Slice "<<isl<<" T"<<tj.UID<<" hitColBeginIndex "<<hitColBeginIndex<<"\n";
        for(auto& tp : tj.Pts) {
          if(tp.Chg <= 0) continue;
          // index of inputHits indices  of hits used in one TP
          std::vector<unsigned int> tpHits;
          for(unsigned short ii = 0; ii < tp.Hits.size(); ++ii) {
            if(!tp.UseHit[ii]) continue;
            if(tp.Hits[ii] > slc.slHits.size() - 1) {
              std::cout<<"bad slice\n";
              badSlice = true;
              break;
            } // bad slHits index
            unsigned int allHitsIndex = slc.slHits[tp.Hits[ii]].allHitsIndex;
            if(allHitsIndex > nInputHits - 1) {
              std::cout<<"TrajCluster module invalid slHits index\n";
              badSlice = true;
              break;
            } // bad allHitsIndex
            tpHits.push_back(allHitsIndex);
            if(newIndex[allHitsIndex] != UINT_MAX) {
              std::cout<<"Bad Slice "<<isl<<" tp.Hits "<<tp.Hits[ii]<<" allHitsIndex "<<allHitsIndex;
              std::cout<<" old newIndex "<<newIndex[allHitsIndex];
              auto& oldhit = (*inputHits)[allHitsIndex];
              std::cout<<" old "<<oldhit.WireID().Plane<<":"<<oldhit.WireID().Wire<<":"<<(int)oldhit.PeakTime();
              auto& newhit = hitCol[newIndex[allHitsIndex]];
              std::cout<<" new "<<newhit.WireID().Plane<<":"<<newhit.WireID().Wire<<":"<<(int)newhit.PeakTime();
              std::cout<<" hitCol size "<<hitCol.size();
              std::cout<<"\n";
            }
            newIndex[allHitsIndex] = hitCol.size();
          } // ii
          if(badSlice) break;
          // Let the alg define the hit either by merging multiple hits or by a simple copy
          // of a single hit from inputHits
          recob::Hit newHit = fTCAlg->MergeTPHits(tpHits);
          if(newHit.Channel() == raw::InvalidChannelID) {
            std::cout<<"TrajCluster module failed merging hits\n";
            badSlice = true;
            break;
          } // MergeTPHits failed
          sumChg += newHit.Integral();
          sumADC += newHit.SummedADC();
          // add it to the new hits collection
          hitCol.push_back(newHit);
        } // tp
        if(badSlice) {
          std::cout<<"Bad slice. Need some error recovery code here\n";
          break;
        }
        geo::View_t view = hitCol[0].View();
        auto& firstTP = tj.Pts[tj.EndPt[0]];
        auto& lastTP = tj.Pts[tj.EndPt[1]];
        int clsID = tj.UID;
        if(tj.AlgMod[tca::kShowerTj]) clsID = -clsID;
        unsigned short nclhits = hitCol.size() - hitColBeginIndex + 1;
        clsCol.emplace_back(
                            firstTP.Pos[0],         // Start wire
                            0,                      // sigma start wire
                            firstTP.Pos[1]/tca::tcc.unitsPerTick,         // start tick
                            0,                      // sigma start tick
                            firstTP.AveChg,         // start charge
                            firstTP.Ang,            // start angle
                            0,                      // start opening angle (0 for line-like clusters)
                            lastTP.Pos[0],          // end wire
                            0,                      // sigma end wire
                            lastTP.Pos[1]/tca::tcc.unitsPerTick,           // end tick
                            0,                      // sigma end tick
                            lastTP.AveChg,          // end charge
                            lastTP.Ang,             // end angle
                            0,                      // end opening angle (0 for line-like clusters)
                            sumChg,                 // integral
                            0,                      // sigma integral
                            sumADC,                 // summed ADC
                            0,                      // sigma summed ADC
                            nclhits,                // n hits
                            0,                      // wires over hits
                            0,                      // width (0 for line-like clusters)
                            clsID,                  // ID from TrajClusterAlg
                            view,                   // view
                            tca::DecodeCTP(tj.CTP), // planeID
                            recob::Cluster::Sentry  // sentry
                           );
        if(!util::CreateAssn(*this, evt, clsCol, hitCol, *cls_hit_assn, hitColBeginIndex, hitCol.size()))
        {
          throw art::Exception(art::errors::ProductRegistrationFailure)<<"Failed to associate hits with cluster ID "<<tj.UID;
        } // exception
        // Make cluster -> vertex associations
        for(unsigned short end = 0; end < 2; ++end) {
          if(tj.VtxID[end] <= 0) continue;
          // cluster -> 2D vertex
          for(unsigned short vx2Index = 0; vx2Index < slc.vtxs.size(); ++vx2Index) {
            auto& vx2 = slc.vtxs[vx2Index];
            if(vx2.ID <= 0) continue;
            if(vx2.ID != tj.VtxID[end]) continue;
            if(!util::CreateAssnD(*this, evt, *cls_vx2_assn, clsCol.size() - 1, vx2Index, end))
            {
              throw art::Exception(art::errors::ProductRegistrationFailure)<<"Failed to associate cluster "<<tj.UID<<" with EndPoint2D";
            } // exception
            break;
          } // vx2Index
          for(unsigned short vx3Index = 0; vx3Index < slc.vtx3s.size(); ++vx3Index) {
            auto& vx3 = slc.vtx3s[vx3Index];
            if(vx3.ID <= 0) continue;
            // ignore incomplete vertices
            if(vx3.Wire >= 0) continue;
            if(std::find(vx3.Vx2ID.begin(), vx3.Vx2ID.end(), tj.VtxID[end]) == vx3.Vx2ID.end()) continue;
            if(!util::CreateAssnD(*this, evt, *cls_vx3_assn, clsCol.size() - 1, vx3Index, end))
            {
              throw art::Exception(art::errors::ProductRegistrationFailure)<<"Failed to associate cluster "<<tj.UID<<" with Vertex";
            } // exception
            break;
          } // vx3Index
        } // end
      } // tj (aka cluster)
      // make EndPoint2Ds
      for(auto& vx2 : slc.vtxs) {
        if(vx2.ID == 0) continue;
        unsigned int vtxID = vx2.UID;
        unsigned int wire = std::nearbyint(vx2.Pos[0]);
        geo::PlaneID plID = tca::DecodeCTP(vx2.CTP);
        geo::WireID wID = geo::WireID(plID.Cryostat, plID.TPC, plID.Plane, wire);
        geo::View_t view = tca::tcc.geom->View(wID);
        vx2Col.emplace_back((double)vx2.Pos[1]/tca::tcc.unitsPerTick,  // Time
                            wID,                  // WireID
                            vx2.Score,            // strength = score
                            vtxID,                // ID
                            view,                 // View
                            0);                   // total charge - not relevant
      } // vx2
      // make Vertices
      for(auto& vx3 : slc.vtx3s) {
        if(vx3.ID <= 0) continue;
        // ignore incomplete vertices
        if(vx3.Wire >= 0) continue;
        unsigned int vtxID = vx3.UID;
        double xyz[3];
        xyz[0] = vx3.X;
        xyz[1] = vx3.Y;
        xyz[2] = vx3.Z;
        vx3Col.emplace_back(xyz, vtxID);
      } // vx3
      // make Showers
      for(auto& ss3 : slc.showers) {
        if(ss3.ID <= 0) continue;
        recob::Shower shower;
        shower.set_id(ss3.UID);
        shower.set_total_energy(ss3.Energy);
        shower.set_total_energy_err(ss3.EnergyErr);
        shower.set_total_MIPenergy(ss3.MIPEnergy);
        shower.set_total_MIPenergy_err(ss3.MIPEnergyErr);
        shower.set_total_best_plane(ss3.BestPlane);
        TVector3 dir = {ss3.Dir[0], ss3.Dir[1], ss3.Dir[2]};
        shower.set_direction(dir);
        TVector3 dirErr = {ss3.DirErr[0], ss3.DirErr[1], ss3.DirErr[2]};
        shower.set_direction_err(dirErr);
        TVector3 pos = {ss3.Start[0], ss3.Start[1], ss3.Start[2]};
        shower.set_start_point(pos);
        TVector3 posErr = {ss3.StartErr[0], ss3.StartErr[1], ss3.StartErr[2]};
        shower.set_start_point_err(posErr);
        shower.set_dedx(ss3.dEdx);
        shower.set_dedx_err(ss3.dEdxErr);
        shower.set_length(ss3.Len);
        shower.set_open_angle(ss3.OpenAngle);
        shwCol.push_back(shower);
        // make the shower - hit association
        std::vector<unsigned int> shwHits(ss3.Hits.size());
        for(unsigned int iht = 0; iht < ss3.Hits.size(); ++iht) shwHits[iht] = newIndex[iht];
        if(!util::CreateAssn(*this, evt, *shwr_hit_assn, shwCol.size()-1, shwHits.begin(), shwHits.end()))
        {
          throw art::Exception(art::errors::ProductRegistrationFailure)<<"Failed to associate hits with Shower";
        } // exception
      } // ss3
      // make PFParticles
      for(unsigned short ipfp = 0; ipfp < slc.pfps.size(); ++ipfp) {
        auto& pfp = slc.pfps[ipfp];
        if(pfp.ID <= 0) continue;
        size_t parentIndex = pfp.ID - 1;
        std::vector<size_t> dtrIndices(pfp.DtrIDs.size());
        for(unsigned short idtr = 0; idtr < pfp.DtrIDs.size(); ++idtr) dtrIndices[idtr] = pfp.DtrIDs[idtr] - 1;
        pfpCol.emplace_back(pfp.PDGCode, ipfp, parentIndex, dtrIndices);
        // PFParticle -> clusters
        std::vector<unsigned int> clsIndices;
        for(auto tjid : pfp.TjIDs) {
          unsigned int clsIndex = 0;
          for(auto& tj : slc.tjs) {
            if(tj.AlgMod[tca::kKilled]) continue;
            if(tj.ID == tjid) break;
            ++clsIndex;
          } // tj
          if(clsIndex == clsCol.size()) {
            std::cout<<"TrajCluster module invalid pfp -> tj -> cluster index\n";
            continue;
          }
          clsIndices.push_back(clsIndex);
        } // tjid
        if(!util::CreateAssn(*this, evt, *pfp_cls_assn, pfpCol.size()-1, clsIndices.begin(), clsIndices.end()))
        {
          throw art::Exception(art::errors::ProductRegistrationFailure)<<"Failed to associate clusters with PFParticle";
        } // exception
        // PFParticle -> Shower
        if(pfp.PDGCode == 1111) {
          std::vector<unsigned short> shwIndex(1, 0);
          for(auto& ss3 : slc.showers) {
            if(ss3.ID <= 0) continue;
            if(ss3.PFPIndex == ipfp) break;
            ++shwIndex[0];
          } // ss3
          if(shwIndex[0] < shwCol.size()) {
            if(!util::CreateAssn(*this, evt, *pfp_shwr_assn, pfpCol.size()-1, shwIndex.begin(), shwIndex.end()))
            {
              throw art::Exception(art::errors::ProductRegistrationFailure)<<"Failed to associate shower with PFParticle";
            } // exception
          } // valid shwIndex
        } // pfp -> Shower
        // PFParticle cosmic tag
        if(tca::tcc.modes[tca::kTagCosmics]) {
          std::vector<float> tempPt1, tempPt2;
          tempPt1.push_back(-999);
          tempPt1.push_back(-999);
          tempPt1.push_back(-999);
          tempPt2.push_back(-999);
          tempPt2.push_back(-999);
          tempPt2.push_back(-999);
          ctCol.emplace_back(tempPt1, tempPt2, pfp.CosmicScore, anab::CosmicTagID_t::kNotTagged);
          if (!util::CreateAssn(*this, evt, pfpCol, ctCol, *pfp_cos_assn, ctCol.size()-1, ctCol.size())){
            throw art::Exception(art::errors::ProductRegistrationFailure)<<"Failed to associate CosmicTag with PFParticle";
          }
        } // cosmic tag
      } // ipfp
    } // slice isl
    // add the hits that weren't used in any slice to hitCol
    for(unsigned int allHitsIndex = 0; allHitsIndex < nInputHits; ++allHitsIndex) {
      if(newIndex[allHitsIndex] == UINT_MAX) hitCol.push_back((*inputHits)[allHitsIndex]);
    } // allHitsIndex

    // clear the slices vector
    fTCAlg->ClearResults();

    // convert vectors to unique_ptrs
    std::cout<<"hitCol size "<<hitCol.size()<<"\n";
    std::unique_ptr<std::vector<recob::Hit> > hcol(new std::vector<recob::Hit>(std::move(hitCol)));
    std::unique_ptr<std::vector<recob::Cluster> > ccol(new std::vector<recob::Cluster>(std::move(clsCol)));
    std::unique_ptr<std::vector<recob::EndPoint2D> > v2col(new std::vector<recob::EndPoint2D>(std::move(vx2Col)));
    std::unique_ptr<std::vector<recob::Vertex> > v3col(new std::vector<recob::Vertex>(std::move(vx3Col)));
    std::unique_ptr<std::vector<recob::PFParticle> > pcol(new std::vector<recob::PFParticle>(std::move(pfpCol)));
    std::unique_ptr<std::vector<recob::Shower> > scol(new std::vector<recob::Shower>(std::move(shwCol)));
    std::unique_ptr<std::vector<anab::CosmicTag>> ctgcol(new std::vector<anab::CosmicTag>(std::move(ctCol)));


    // move the cluster collection and the associations into the event:
    if(fHitModuleLabel != "NA") {
      recob::HitRefinerAssociator shcol(*this, evt, fHitModuleLabel, fDoWireAssns, fDoRawDigitAssns);
      shcol.use_hits(std::move(hcol));
      shcol.put_into(evt);
    } else {
      recob::HitRefinerAssociator shcol(*this, evt, fSlicerModuleLabel, fDoWireAssns, fDoRawDigitAssns);
      shcol.use_hits(std::move(hcol));
      shcol.put_into(evt);
    }
    evt.put(std::move(ccol));
    evt.put(std::move(cls_hit_assn));
    evt.put(std::move(v2col));
    evt.put(std::move(v3col));
    evt.put(std::move(scol));
    evt.put(std::move(shwr_hit_assn));
    evt.put(std::move(cls_vx2_assn));
    evt.put(std::move(cls_vx3_assn));
    evt.put(std::move(pcol));
    evt.put(std::move(pfp_cls_assn));
    evt.put(std::move(pfp_shwr_assn));
    evt.put(std::move(pfp_vtx_assn));
    evt.put(std::move(ctgcol));
    evt.put(std::move(pfp_cos_assn));
  } // TrajCluster::produce()
  
  //----------------------------------------------------------------------------
  DEFINE_ART_MODULE(TrajCluster)
  
} // namespace cluster

