//
// Create a compressed representation of Calorimeter StepPointMCs.
//
// We recollect all StepPointMCs attached to a SimParticle ancestor and collapse them into
// CaloShowerStep objects. A SimParticle entering the calorimeter is an "ancestor" SimParticle;
// it generates a shower of SimParticles and StepPointMCs in the crystal, which is compressed.
// At the end all StepPointMCs can be dropped, as well as a large fraction of SimParticles, with
// negligible loss of information.
//
// Note: if a SimParticle enters the calorimeter and generates a secondary that hits another disk
// (e.g. a photon leaks from disk 1 into disk 2), the SimParticle hitting the second disk is itself
// considered an ancestor.
//
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Principal/Event.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/Sequence.h"

#include "Offline/CalorimeterGeom/inc/Calorimeter.hh"
#include "Offline/GeometryService/inc/GeomHandle.hh"
#include "Offline/CaloMC/inc/ShowerStepUtil.hh"
#include "Offline/MCDataProducts/inc/StepPointMC.hh"
#include "Offline/MCDataProducts/inc/SimParticle.hh"
#include "Offline/MCDataProducts/inc/CaloShowerStep.hh"
#include "cetlib_except/exception.h"

#include "CLHEP/Vector/ThreeVector.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <iomanip>
#include <ostream>
#include <streambuf>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>


namespace {

   // Reject bytes before allocating them; ostream exceptions make every insertion fail closed.
   class MembershipBuffer : public std::streambuf
   {
       public:
           void reset(std::size_t limit) { text_.clear(); limit_ = limit; }
           const std::string& text() const { return text_; }
       protected:
           std::streamsize xsputn(const char* data, std::streamsize count) override
           {
               if (count < 0 || static_cast<std::size_t>(count) > limit_ - text_.size())
                   throw cet::exception("CALOMEMBERSHIP") << "Membership ledger byte cap exceeded; discard job ledger\n";
               text_.append(data, static_cast<std::size_t>(count));
               return count;
           }
           int_type overflow(int_type ch) override
           {
               if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
               const char value = traits_type::to_char_type(ch);
               xsputn(&value, 1);
               return ch;
           }
       private:
           std::string text_;
           std::size_t limit_{0};
   };

   // Accumulates the steps of one ancestor SimParticle together with the set of
   // (secondary) SimParticles that were folded into it.
   class CaloCompressUtil
   {
       public:
           using SimPtr = art::Ptr<mu2e::SimParticle>;

           const std::vector<const mu2e::StepPointMC*>& steps() const {return steps_;}
           const std::set<SimPtr>&                      sims()  const {return sims_;}

           void fill(const mu2e::StepPointMC* step, const std::vector<SimPtr>& sims)
           {
               steps_.push_back(step);
               sims_.insert(sims.begin(), sims.end());
           }

        private:
           std::vector<const mu2e::StepPointMC*> steps_;
           std::set<SimPtr>                      sims_;
   };

   struct DiagSummary
   {
       float    totalEdep_{0.f};
       unsigned totalStep_{0}, totalSim_{0}, totalChk_{0}, nCompress_{0}, nCompressInfo_{0};
       void reset() {*this = DiagSummary{};}
   };

}


namespace mu2e {

  class CaloShowerStepMaker : public art::EDProducer
  {
      public:
         struct Config
         {
             using Name    = fhicl::Name;
             using Comment = fhicl::Comment;
             fhicl::Sequence<std::string> caloStepPointCollection { Name("caloStepPointCollection"), Comment("Calo crystal stepPointMC collection name") };
             fhicl::Atom<unsigned>        numZSlices              { Name("numZSlices"),              Comment("Number of crystal longitudinal slices") };
             fhicl::Atom<float>           deltaTime               { Name("deltaTime"),               Comment("Max time difference to be inside a ShowerStep") };
             fhicl::Atom<bool>            compressData            { Name("compressData"),            Comment("Compress stepPointMC and SimParticles in crystal") };
             fhicl::Atom<double>          eDepThreshold           { Name("eDepThreshold"),           Comment("Threshold on energy deposited by SimParticle to keep it") };
             fhicl::Atom<std::string>     membershipLedger        { Name("membershipLedger"),        Comment("Optional exclusive-create text membership ledger; empty disables"), "" };
             fhicl::Atom<std::size_t>     membershipMaxRawRowsPerEvent { Name("membershipMaxRawRowsPerEvent"), Comment("Positive raw-row cap when ledger enabled"), 2000000 };
             fhicl::Atom<std::size_t>     membershipMaxBytesPerJob { Name("membershipMaxBytesPerJob"), Comment("Positive total ledger byte cap including header"), 268435456 };
             fhicl::Atom<int>             diagLevel               { Name("diagLevel"),               Comment("Debug"),0 };
         };

         explicit CaloShowerStepMaker(const art::EDProducer::Table<Config>& config);
         void produce(art::Event& e) override;


      private:
         using SimPtr      = art::Ptr<SimParticle>;
         using StepHandles = std::vector<art::ValidHandle<StepPointMCCollection>>;

         void makeCompressedHits      (const StepHandles&, CaloShowerStepCollection&, SimParticlePtrCollection&);
         void collectStepBySimAncestor(const Calorimeter&, const StepHandles&, std::map<SimPtr,CaloCompressUtil>&);
         void compressSteps           (const Calorimeter&, CaloShowerStepCollection&, int volId, const SimPtr&, std::vector<const StepPointMC*>& steps);
         void dumpAllInfo             (const StepHandles&, const Calorimeter&) const;

         std::vector<art::ProductToken<StepPointMCCollection>> stepTokens_;
         unsigned    numZSlices_;
         double      deltaTime_;
         bool        compressData_;
         double      eDepThreshold_;
         int         diagLevel_;
         double      zSliceSize_{0};
         DiagSummary diag_;

         // Membership diagnostics never participate in the stock grouping or accumulation.
         struct LedgerRaw
         {
             std::size_t collection, index;
             unsigned product;
             unsigned disposition{0};
         };
         std::unique_ptr<std::FILE, decltype(&std::fclose)> ledgerFile_{nullptr, &std::fclose};
         std::vector<std::string> ledgerTags_;
         std::size_t ledgerMaxRawRows_{0}, ledgerMaxBytes_{0}, ledgerBytesWritten_{0};
         MembershipBuffer ledgerBuffer_;
         std::ostream ledgerText_{&ledgerBuffer_};
         std::unordered_map<const StepPointMC*, LedgerRaw> ledgerRaw_;
         std::size_t ledgerGroup_{0}, ledgerAccepted_{0}, ledgerRejected_{0};
         void ledgerBegin(const art::Event&, const StepHandles&);
         void ledgerReject(const std::vector<const StepPointMC*>&);
         void ledgerEmit(std::size_t, int, const SimPtr&, unsigned, std::size_t,
                         const char*, const std::vector<const StepPointMC*>&, unsigned);
         void ledgerFinish(std::size_t);
         void ledgerWrite(const std::string&);
  };



  CaloShowerStepMaker::CaloShowerStepMaker(const art::EDProducer::Table<Config>& config) :
     art::EDProducer{config},
     numZSlices_   (config().numZSlices()),
     deltaTime_    (config().deltaTime()),
     compressData_ (config().compressData()),
     eDepThreshold_(config().eDepThreshold()),
     diagLevel_    (config().diagLevel())
  {
      if (numZSlices_ == 0)
         throw cet::exception("CONFIG") << "[CaloShowerStepMaker] numZSlices must be > 0\n";

      for (const auto& tag : config().caloStepPointCollection())
         stepTokens_.push_back(consumes<StepPointMCCollection>(art::InputTag(tag)));

      produces<CaloShowerStepCollection>();
      produces<SimParticlePtrCollection>();

      if (!config().membershipLedger().empty())
      {
          ledgerMaxRawRows_ = config().membershipMaxRawRowsPerEvent();
          ledgerMaxBytes_ = config().membershipMaxBytesPerJob();
          if (!ledgerMaxRawRows_ || !ledgerMaxBytes_)
              throw cet::exception("CALOMEMBERSHIP") << "Membership caps must both be positive\n";
          ledgerText_.exceptions(std::ios::badbit | std::ios::failbit);
          ledgerBuffer_.reset(ledgerMaxBytes_);
          ledgerTags_ = config().caloStepPointCollection();
          ledgerFile_.reset(std::fopen(config().membershipLedger().c_str(), "wx"));
          if (!ledgerFile_) throw cet::exception("CALOMEMBERSHIP") << "Cannot exclusively create membership ledger\n";
          ledgerText_ << "H calo-membership 1\n"
                 << "P numZSlices unsigned " << numZSlices_ << '\n'
                 << "P deltaTime float " << std::hexfloat << deltaTime_ << '\n'
                 << "P compressData bool " << compressData_ << '\n'
                 << "P eDepThreshold double " << eDepThreshold_ << '\n'
                 << "P membershipMaxRawRowsPerEvent size_t " << ledgerMaxRawRows_ << '\n'
                 << "P membershipMaxBytesPerJob size_t " << ledgerMaxBytes_ << '\n';
          ledgerWrite(ledgerBuffer_.text());
      }
  }


  //------------------------------------------------------------------------------------------------------------
  void CaloShowerStepMaker::produce(art::Event& event)
  {
      diag_.reset();
      if (diagLevel_ > 0) std::cout << "[CaloShowerStepMaker::produce] begin" << std::endl;

      auto caloShowerStepMCs = std::make_unique<CaloShowerStepCollection>();
      auto simsToKeep        = std::make_unique<SimParticlePtrCollection>();

      StepHandles crystalStepsHandles;
      crystalStepsHandles.reserve(stepTokens_.size());
      for (const auto& token : stepTokens_) crystalStepsHandles.push_back(event.getValidHandle(token));

      if (ledgerFile_) ledgerBegin(event, crystalStepsHandles);
      makeCompressedHits(crystalStepsHandles, *caloShowerStepMCs, *simsToKeep);
      if (ledgerFile_) ledgerFinish(caloShowerStepMCs->size());

      event.put(std::move(caloShowerStepMCs));
      event.put(std::move(simsToKeep));
      if (ledgerFile_) ledgerWrite(ledgerBuffer_.text());

      if (diagLevel_ > 0) std::cout << "[CaloShowerStepMaker::produce] end" << std::endl;
  }


  //------------------------------------------------------------------------------------------------------------------
  void CaloShowerStepMaker::makeCompressedHits(const StepHandles& crystalStepsHandles,
                                               CaloShowerStepCollection& caloShowerStepMCs, SimParticlePtrCollection& simsToKeep)
  {
      const Calorimeter& cal = *(GeomHandle<Calorimeter>());
      zSliceSize_            = cal.G4Info().get<double>("crystalZLength")/float(numZSlices_) + 1e-5;
      if (ledgerFile_) ledgerText_ << "D zSliceSize double " << std::hexfloat << zSliceSize_ << '\n';

      // Collect the StepPointMCs produced by each ancestor SimParticle
      std::map<SimPtr,CaloCompressUtil> crystalAncestorsMap;
      collectStepBySimAncestor(cal, crystalStepsHandles, crystalAncestorsMap);

      if (diagLevel_ > 2) dumpAllInfo(crystalStepsHandles, cal);

      // Loop over ancestor SimParticles, check if they are compressible, and produce the corresponding CaloShowerStep
      std::set<SimPtr> simsToKeepUnique;
      for (const auto& [sim, info] : crystalAncestorsMap)
      {
          diag_.totalSim_ += info.sims().size();

          std::map<unsigned,std::vector<const StepPointMC*>> crystalMap;
          for (const StepPointMC* step : info.steps()) crystalMap[step->volumeId()].push_back(step);

          for (auto& [crid, steps] : crystalMap)
          {
              //Filter very small energy deposits at this stage
              double eDep(0);
              for (const auto* step : steps) eDep += step->totalEDep();
              if (ledgerFile_)
              {
                  ++ledgerGroup_;
                  ledgerText_ << "G " << ledgerGroup_ << ' ' << sim.id().value() << ' ' << sim.key()
                              << ' ' << crid << ' ' << std::hexfloat << eDep << ' ' << steps.size() << '\n';
              }
              if (eDep < eDepThreshold_)
              {
                  if (ledgerFile_) ledgerReject(steps);
                  continue;
              }

              if (compressData_)
              {
                  simsToKeepUnique.insert(sim);
                  compressSteps(cal, caloShowerStepMCs, crid, sim, steps);
              }
              else
              {
                  std::map<SimPtr, std::vector<const StepPointMC*>> newSimStepMap;
                  for (const StepPointMC* step : steps) newSimStepMap[step->simParticle()].push_back(step);
                  for (auto& [stepSim, stepVec] : newSimStepMap)
                  {
                      compressSteps(cal, caloShowerStepMCs, crid, stepSim, stepVec);
                      simsToKeepUnique.insert(stepSim);
                  }
              }
          }
          ++diag_.nCompressInfo_;
          if (compressData_) ++diag_.nCompress_;
      }

      simsToKeep.assign(simsToKeepUnique.begin(), simsToKeepUnique.end());

      // Detailed diag info
      if (diagLevel_ > 1)
      {
          std::cout<<"CaloShowerStepMaker summary"<<std::endl;

          std::set<int> volIds;
          for (const auto& css : caloShowerStepMCs) volIds.insert(css.volumeG4ID());

          for (int volId : volIds)
          {
             std::map<art::Ptr<SimParticle>, double> simMap;
             for (const auto& css : caloShowerStepMCs)
                if (css.volumeG4ID()==volId) simMap[css.simParticle()] += css.energyDepG4();

             for (const auto& [simPtr, energy] : simMap)
                std::cout<<"Vol id: "<<volId<<"  Sim id: "<<simPtr.id()<<"   energy="<<energy<<std::endl;
          }
      }

      if (diagLevel_ > 0)
        std::cout << "[CaloShowerStepMaker] compressed "<<diag_.nCompress_<<" / "<<diag_.nCompressInfo_<<" incoming SimParticles"<<std::endl
                  << "[CaloShowerStepMaker] keeping "<<simsToKeep.size()<<" SimParticles"<<std::endl
                  << "[CaloShowerStepMaker] Total sims init: "<<diag_.totalSim_<<std::endl
                  << "[CaloShowerStepMaker] Total caloShower steps: "<<caloShowerStepMCs.size()<<std::endl
                  << "[CaloShowerStepMaker] Total energy deposited / number of stepPointMC: "<<diag_.totalEdep_<<" / "<<diag_.totalStep_<<std::endl
                  << "[CaloShowerStepMaker] Total stepPointMCs seen: "<<diag_.totalChk_<<std::endl;
  }


  //------------------------------------------------------------------------------------------------------------------
  void CaloShowerStepMaker::collectStepBySimAncestor(const Calorimeter& cal,
                                                     const StepHandles& stepsHandles,
                                                     std::map<SimPtr,CaloCompressUtil>& ancestorsMap)
  {
     std::vector<SimPtr>               inspectedSims;
     std::unordered_map<SimPtr,SimPtr> simToAncestorMap;

     for (const auto& handle : stepsHandles)
     {
         const StepPointMCCollection& steps(*handle);

         for (const auto& step : steps)
         {
             inspectedSims.clear();
             SimPtr sim = step.simParticle();
             while (sim->hasParent())
             {
                 const auto alreadyInspected = simToAncestorMap.find(sim);
                 if (alreadyInspected != simToAncestorMap.end()) {sim = alreadyInspected->second; break;}
                 inspectedSims.push_back(sim);

                 if (!cal.isInsideAnyCrystal(sim->startPosition()))                    break;
                 if (!cal.isInsideSameDisk(sim->startPosition(),sim->endPosition()))   break;

                 sim = sim->parent();
             }

             for (const SimPtr& inspectedSim : inspectedSims) simToAncestorMap[inspectedSim] = sim;
             ancestorsMap[sim].fill(&step, inspectedSims);

             diag_.totalEdep_ += step.totalEDep();
         }
         diag_.totalStep_ += steps.size();
      }
  }


  //-------------------------------------------------------------------------------------------------------------------------------
  void CaloShowerStepMaker::compressSteps(const Calorimeter& cal, CaloShowerStepCollection& caloShowerStepMCs,
                                          int volId, const SimPtr& sim, std::vector<const StepPointMC*>& steps)
  {
     std::sort(steps.begin(), steps.end(), [](const StepPointMC* a, const StepPointMC* b){return a->time() < b->time();});

     ShowerStepUtil buffer(numZSlices_, ShowerStepUtil::weight_type::energy);
     // Sparse diagnostic buckets consume space only for observed raw members.
     std::map<unsigned, std::vector<const StepPointMC*>> ledgerMembers;
     std::map<unsigned, std::size_t> ledgerGeneration;

     for (const StepPointMC* step : steps)
     {
         const CLHEP::Hep3Vector pos = cal.mu2eToCrystal(volId, step->position());
         const unsigned idx = std::min<unsigned>(unsigned(std::max(1e-6, pos.z())/zSliceSize_), numZSlices_-1);
         if (buffer.entries(idx)>0 && (step->time()-buffer.t0(idx) > deltaTime_))
         {
             if (diagLevel_ > 2) {std::cout<<"[CaloShowerStepMaker::compressSteps] inserted  "; buffer.printBucket(idx);}
             diag_.totalChk_ += buffer.entries(idx);

             caloShowerStepMCs.emplace_back(volId, sim, buffer.entries(idx), buffer.time(idx), buffer.energyG4(idx),
                                            buffer.energyVis(idx), buffer.pIn(idx), buffer.pos(idx));
             if (ledgerFile_)
             {
                 ledgerEmit(caloShowerStepMCs.size()-1, volId, sim, idx, ledgerGeneration[idx],
                            "time_gap", ledgerMembers.at(idx), buffer.entries(idx));
                 ledgerMembers.at(idx).clear();
                 ++ledgerGeneration[idx];
             }
             buffer.reset(idx);
         }

         buffer.add(idx, step->totalEDep(), step->visibleEDep(), step->time(), step->momentum().mag(), pos);
         if (ledgerFile_) ledgerMembers[idx].push_back(step);
     }

     //do not forget to flush the final buffer(s) :-)
     for (unsigned i=0; i<buffer.nBuckets(); ++i)
     {
         if (buffer.entries(i) == 0) continue;

         if (diagLevel_ > 2) {std::cout<<"[CaloShowerStepMaker::compressSteps] inserted "; buffer.printBucket(i);}
         diag_.totalChk_ += buffer.entries(i);

         caloShowerStepMCs.emplace_back(volId, sim, buffer.entries(i), buffer.time(i), buffer.energyG4(i),
                                        buffer.energyVis(i), buffer.pIn(i), buffer.pos(i));
         if (ledgerFile_) ledgerEmit(caloShowerStepMCs.size()-1, volId, sim, i, ledgerGeneration[i],
                                     "final_bucket", ledgerMembers.at(i), buffer.entries(i));
     }
  }

  // All references below are observed source addresses, never a numerical output match.
  void CaloShowerStepMaker::ledgerBegin(const art::Event& event, const StepHandles& handles)
  {
      // Overflow-safe total across all handles, before allocating any raw-address registry.
      std::size_t rawRows = 0;
      for (const auto& handle : handles)
      {
          if (handle->size() > ledgerMaxRawRows_ - rawRows)
              throw cet::exception("CALOMEMBERSHIP") << "Membership raw rows per event cap exceeded\n";
          rawRows += handle->size();
      }
      ledgerBuffer_.reset(ledgerMaxBytes_ - ledgerBytesWritten_);
      ledgerText_.clear();
      ledgerRaw_.clear();
      ledgerGroup_ = ledgerAccepted_ = ledgerRejected_ = 0;
      ledgerText_ << "E " << event.id().run() << ' ' << event.id().subRun() << ' ' << event.id().event()
                  << ' ' << event.getProductID<CaloShowerStepCollection>().value() << ' ' << handles.size() << '\n';
      std::set<unsigned> products;
      for (std::size_t slot = 0; slot < handles.size(); ++slot)
      {
          const auto& handle = handles.at(slot);
          const unsigned product = handle.id().value();
          if (!products.insert(product).second)
              throw cet::exception("CALOMEMBERSHIP") << "Duplicate resolved input ProductID, including empty collections\n";
          ledgerText_ << "C " << slot << ' ' << product << ' ' << handle->size()
                      << ' ' << std::quoted(ledgerTags_.at(slot)) << '\n';
          for (std::size_t index = 0; index < handle->size(); ++index)
              if (!ledgerRaw_.emplace(&handle->at(index), LedgerRaw{slot, index, product, 0}).second)
                  throw cet::exception("CALOMEMBERSHIP") << "Ambiguous raw source address\n";
      }
  }

  void CaloShowerStepMaker::ledgerReject(const std::vector<const StepPointMC*>& steps)
  {
      std::size_t order = 0;
      for (const auto* step : steps)
      {
          auto& raw = ledgerRaw_.at(step);
          if (raw.disposition) throw cet::exception("CALOMEMBERSHIP") << "Repeated raw disposition\n";
          raw.disposition = 2;
          ++ledgerRejected_;
          ledgerText_ << "R " << ledgerGroup_ << ' ' << raw.collection << ' ' << raw.product << ' '
                      << raw.index << ' ' << order++ << " below_ancestor_crystal_threshold\n";
      }
  }

  void CaloShowerStepMaker::ledgerEmit(std::size_t output, int crystal, const SimPtr& sim,
                                      unsigned bucket, std::size_t generation, const char* cause,
                                      const std::vector<const StepPointMC*>& members, unsigned entries)
  {
      if (members.size() != entries)
          throw cet::exception("CALOMEMBERSHIP") << "Shadow membership differs from stock buffer entries\n";
      ledgerText_ << "S " << output << ' ' << ledgerGroup_ << ' ' << sim.id().value() << ' ' << sim.key()
                  << ' ' << crystal << ' ' << bucket << ' ' << generation << ' ' << cause << ' ' << members.size() << '\n';
      for (std::size_t order = 0; order < members.size(); ++order)
      {
          auto& raw = ledgerRaw_.at(members.at(order));
          if (raw.disposition) throw cet::exception("CALOMEMBERSHIP") << "Repeated raw disposition\n";
          raw.disposition = 1;
          ++ledgerAccepted_;
          ledgerText_ << "A " << output << ' ' << order << ' ' << raw.collection << ' ' << raw.product
                      << ' ' << raw.index << '\n';
      }
  }

  void CaloShowerStepMaker::ledgerFinish(std::size_t outputs)
  {
      for (const auto& item : ledgerRaw_)
          if (!item.second.disposition) throw cet::exception("CALOMEMBERSHIP") << "Raw row lacks disposition\n";
      ledgerText_ << "Z " << ledgerRaw_.size() << ' ' << ledgerAccepted_ << ' ' << ledgerRejected_
                  << ' ' << outputs << ' ' << ledgerGroup_ << '\n';
  }

  void CaloShowerStepMaker::ledgerWrite(const std::string& text)
  {
      // Reserve the complete write against the job cap before touching the file.
      if (text.size() > ledgerMaxBytes_ - ledgerBytesWritten_)
          throw cet::exception("CALOMEMBERSHIP") << "Membership ledger byte cap exceeded before write\n";
      ledgerBytesWritten_ += text.size();
      if (std::fwrite(text.data(), 1, text.size(), ledgerFile_.get()) != text.size() ||
          std::fflush(ledgerFile_.get()) != 0)
          throw cet::exception("CALOMEMBERSHIP") << "Membership ledger write/flush failed; discard incomplete ledger\n";
  }

  //-------------------------------------------------------------------------------------------------------------
  void CaloShowerStepMaker::dumpAllInfo(const StepHandles& stepsHandles, const Calorimeter& cal) const
  {
      std::cout<<"Dumping StepPointMCs  Mu2e / crystal / disk / diskFF frames"<<std::endl;
      for (const auto& handle : stepsHandles)
      {
          const StepPointMCCollection& steps(*handle);
          std::cout<<steps.size()<<std::endl;
          for (const auto& step : steps)
            std::cout<<step.volumeId()<<" "<<step.totalEDep()<<" "<<step.position()<<" "
                     <<cal.mu2eToCrystal(step.volumeId(),step.position())<<"   "
                     <<cal.mu2eToDisk(cal.crystal(step.volumeId()).diskID(),step.position())<<"   "
                     <<cal.mu2eToDiskFF(cal.crystal(step.volumeId()).diskID(),step.position())<<std::endl;
      }
  }

}

DEFINE_ART_MODULE(mu2e::CaloShowerStepMaker)
