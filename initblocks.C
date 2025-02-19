/*                                                                           
Developed by Sandeep Sharma and Garnet K.-L. Chan, 2012                      
Copyright (c) 2012, Garnet K.-L. Chan                                        
                                                                             
This program is integrated in Molpro with the permission of 
Sandeep Sharma and Garnet K.-L. Chan
*/


#include "global.h"
#include "initblocks.h"
#include "pario.h"
#include "Stackspinblock.h"

void SpinAdapted::InitBlocks::InitStartingBlock (StackSpinBlock& startingBlock, const bool &forward, int leftState, int rightState,
						 const int & forward_starting_size, const int &backward_starting_size,
						 const int& restartSize, const bool &restart, const bool& warmUp, int integralIndex)
{
  if (restart && restartSize != 1)
  {
    int len = restart? restartSize : forward_starting_size;
    vector<int> sites(len);
    if (forward)
      for (int i=0; i<len; i++)
	sites[i] = i;
    else
      for (int i=0; i<len; i++) 
	sites[i] = dmrginp.last_site() - len +i ;
    
    if (restart)
      StackSpinBlock::restore (forward, sites, startingBlock, leftState, rightState);
    else
      StackSpinBlock::restore (true, sites, startingBlock, leftState, rightState);
    startingBlock.set_twoInt(integralIndex);
  }
  else if (forward)
  {
    //startingBlock = singleSiteBlocks[integralIndex][0];//StackSpinBlock(0, forward_starting_size - 1, integralIndex, leftState==rightState, true);
    startingBlock = StackSpinBlock(0, forward_starting_size - 1, integralIndex, leftState==rightState, true);
    if (dmrginp.add_noninteracting_orbs() && dmrginp.molecule_quantum().get_s().getirrep() != 0 && dmrginp.spinAdapted())
    {
      SpinQuantum s = dmrginp.molecule_quantum();
      s = SpinQuantum(s.get_s().getirrep(), s.get_s(), IrrepSpace(0));
      int qs = 1, ns = 1;
      StateInfo addstate(ns, &s, &qs); 
      StackSpinBlock dummyblock(addstate, integralIndex);
      StackSpinBlock newstartingBlock;
      newstartingBlock.set_integralIndex() = integralIndex;
      newstartingBlock.default_op_components(false, true, true, leftState==rightState);
      newstartingBlock.setstoragetype(LOCAL_STORAGE);
      newstartingBlock.BuildSumBlock(NO_PARTICLE_SPIN_NUMBER_CONSTRAINT, startingBlock, dummyblock);

      {
	long memoryToFree = newstartingBlock.getdata() - startingBlock.getdata();
	long newsysmem = newstartingBlock.memoryUsed();
	newstartingBlock.moveToNewMemory(startingBlock.getdata());
	Stackmem[omprank].deallocate(newstartingBlock.getdata()+newsysmem, memoryToFree);
      }
      startingBlock = newstartingBlock;
    }
  }
  else
  {
    std::vector<int> backwardSites;
    if(dmrginp.spinAdapted()) {
      for (int i = 0; i < backward_starting_size; ++i) 
	backwardSites.push_back (dmrginp.last_site() - i - 1);
    }
    else {
      for (int i = 0; i < backward_starting_size; ++i) 
	backwardSites.push_back (dmrginp.last_site()/2 - i - 1);
    }
    sort (backwardSites.begin (), backwardSites.end ());
    //startingBlock = singleSiteBlocks[integralIndex][backwardSites[0]];
    startingBlock.set_integralIndex() = integralIndex;
    startingBlock.default_op_components(false, leftState==rightState);
    startingBlock.BuildTensorProductBlock (backwardSites);
  }
}


void SpinAdapted::InitBlocks::InitNewSystemBlock(StackSpinBlock &system, StackSpinBlock &systemDot, StackSpinBlock &newSystem, int leftState, int rightState, const int& sys_add, const bool &direct, int integralIndex, const Storagetype &storage, bool haveNormops, bool haveCompops, int constraint)
{
  newSystem.set_integralIndex() = integralIndex;
  newSystem.default_op_components(direct, haveNormops, haveCompops, leftState==rightState);
  newSystem.setstoragetype(storage);
  newSystem.BuildSumBlock (constraint, system, systemDot);

  //p2out << "\t\t\t NewSystem block " << endl << newSystem << endl;
  //newSystem.printOperatorSummary();
}

void SpinAdapted::InitBlocks::InitBigBlock(StackSpinBlock &leftBlock, StackSpinBlock &rightBlock, StackSpinBlock &big)
{
  //set big block components
  big.set_integralIndex() = leftBlock.get_integralIndex();
  
  big.set_big_components(); 
  // build the big block
  if (dmrginp.hamiltonian() == BCS) {
    big.BuildSumBlock(SPIN_NUMBER_CONSTRAINT, leftBlock, rightBlock);
  } else {
    big.BuildSumBlock(PARTICLE_SPIN_NUMBER_CONSTRAINT, leftBlock, rightBlock);
  }
}


void SpinAdapted::InitBlocks::InitNewEnvironmentBlock(StackSpinBlock &environment, StackSpinBlock& environmentDot, StackSpinBlock &newEnvironment, 
						      const StackSpinBlock &system, StackSpinBlock &systemDot, int leftState, int rightState,
						      const int &sys_add, const int &env_add, const bool &forward, const bool &direct, 
						      const bool &onedot, const bool &nexact, const bool &useSlater, int integralIndex, 
						      bool haveNormops, bool haveCompops, const bool& dot_with_sys, int constraint) {
  // now initialise environment Dot
  int systemDotStart, systemDotEnd, environmentDotStart, environmentDotEnd, environmentStart, environmentEnd;
  int systemDotSize = sys_add - 1;
  int environmentDotSize = env_add - 1;
  if (forward) {
    systemDotStart = dmrginp.spinAdapted() ? *system.get_sites().rbegin () + 1 : (*system.get_sites().rbegin ())/2 + 1 ;
    systemDotEnd = systemDotStart + systemDotSize;
    environmentDotStart = systemDotEnd + 1;
    environmentDotEnd = environmentDotStart + environmentDotSize;
    environmentStart = environmentDotEnd + 1;
    environmentEnd = dmrginp.spinAdapted() ? dmrginp.last_site() - 1 : dmrginp.last_site()/2 - 1;
  } else {
    systemDotStart = dmrginp.spinAdapted() ? system.get_sites()[0] - 1 : (system.get_sites()[0])/2 - 1 ;
    systemDotEnd = systemDotStart - systemDotSize;
    environmentDotStart = systemDotEnd - 1;
    environmentDotEnd = environmentDotStart - environmentDotSize;
    environmentStart = environmentDotEnd - 1;
    environmentEnd = 0;
  }

  std::vector<int> environmentSites;
  environmentSites.resize(abs(environmentEnd - environmentStart) + 1);
  for (int i = 0; i < abs(environmentEnd - environmentStart) + 1; ++i) *(environmentSites.begin () + i) = min(environmentStart,environmentEnd) + i;


  // now initialise environment
  if (useSlater) { // for FCI
    StateInfo system_stateinfo = system.get_stateInfo();
    StateInfo sysdot_stateinfo = systemDot.get_stateInfo();
    StateInfo tmp;
    TensorProduct (system_stateinfo, sysdot_stateinfo, tmp, NO_PARTICLE_SPIN_NUMBER_CONSTRAINT);
    // tmp has the system+dot quantum numbers
    tmp.CollectQuanta ();
    
    if (dmrginp.warmup() == LOCAL2 || dmrginp.warmup() == LOCAL3 || dmrginp.warmup() == LOCAL4) {
      int nactiveSites, ncoreSites;
      if (dmrginp.warmup() == LOCAL2) {
        nactiveSites = 1;
      } else if (dmrginp.warmup() == LOCAL3) {
        nactiveSites = 2;
      } else if (dmrginp.warmup() == LOCAL4) {
        nactiveSites = 3;
      }
      if (dot_with_sys && onedot) {
        nactiveSites += 1;
      }

      if (nactiveSites > environmentSites.size()) {
        nactiveSites = environmentSites.size();
      }
      ncoreSites = environmentSites.size() - nactiveSites;

      // figure out what sites are in the active and core sites
      int environmentActiveEnd = forward ? environmentStart + nactiveSites - 1 : environmentStart - nactiveSites + 1;
      int environmentCoreStart = forward ? environmentActiveEnd + 1 : environmentActiveEnd - 1;
      
      std::vector<int> activeSites(nactiveSites), coreSites(ncoreSites);
      for (int i = 0; i < nactiveSites; ++i) {
        activeSites[i] = min(environmentStart,environmentActiveEnd) + i;
      }
      for (int i = 0; i < ncoreSites; ++i) {
        coreSites[i] = min(environmentCoreStart,environmentEnd) + i;
      }

      StackSpinBlock environmentActive, environmentCore;
      if (coreSites.size() > 0) {
	      environmentActive.set_integralIndex() = integralIndex;
	      environmentCore.set_integralIndex() = integralIndex;
        environmentActive.default_op_components(!forward, leftState==rightState);
        environmentActive.setstoragetype(DISTRIBUTED_STORAGE);
        environmentCore.default_op_components(!forward, leftState==rightState);      
        environmentCore.setstoragetype(DISTRIBUTED_STORAGE);

        environmentActive.BuildTensorProductBlock(activeSites);
        environmentCore.BuildSingleSlaterBlock(coreSites);

        environmentCore.addAdditionalOps();
        environmentActive.addAdditionalOps();

        if ((!dot_with_sys && onedot) || !onedot) {
	        environment.set_integralIndex() = integralIndex;
          environment.default_op_components(!forward, leftState == rightState);
          environment.setstoragetype(DISTRIBUTED_STORAGE);
          environment.BuildSumBlock(constraint, environmentActive, environmentCore);
        } else {
	        newEnvironment.set_integralIndex() = integralIndex;
          newEnvironment.default_op_components(direct, haveNormops, haveCompops, leftState == rightState);
          newEnvironment.setstoragetype(DISTRIBUTED_STORAGE);
          newEnvironment.BuildSumBlock(constraint, environmentActive, environmentCore);
	        //p2out << "\t\t\t NewEnvironment block " << endl << newEnvironment << endl;
	        //newEnvironment.printOperatorSummary();
        }
      } else { // no core
        if ((!dot_with_sys && onedot) || !onedot) {
	  environment.set_integralIndex() = integralIndex;
          environment.default_op_components(!forward, leftState==rightState);
          environment.setstoragetype(DISTRIBUTED_STORAGE);
          environment.BuildTensorProductBlock(environmentSites); // exact block
        } else {
	  newEnvironment.set_integralIndex() = integralIndex;
          newEnvironment.default_op_components(!forward, leftState==rightState);
          newEnvironment.setstoragetype(DISTRIBUTED_STORAGE);
          newEnvironment.BuildTensorProductBlock(environmentSites);
        }
      }
    } else { //used for warmup guess environemnt
      std::vector<SpinQuantum> quantumNumbers;
      std::vector<int> distribution;
      std::map<SpinQuantum, int> quantaDist;
      std::map<SpinQuantum, int>::iterator quantaIterator;
      bool environmentComplementary = !forward;
      StateInfo tmp2;

      // tmp is the quantum numbers of newSystem (sys + sysdot)
      if (onedot) tmp.quanta_distribution (quantumNumbers, distribution, true);
      else {
        StateInfo environmentdot_stateinfo = environmentDot.get_stateInfo();
        TensorProduct (tmp, environmentdot_stateinfo, tmp2, constraint);
        tmp2.CollectQuanta ();
        tmp2.quanta_distribution (quantumNumbers, distribution, true);

      }
      
      for (int i = 0; i < distribution.size (); ++i) {
	quantaIterator = quantaDist.find(quantumNumbers[i]);
	if (quantaIterator != quantaDist.end()) distribution[i] += quantaIterator->second;
        distribution [i] /= 4; distribution [i] += 1;
        if (distribution [i] > dmrginp.nquanta()) distribution [i] = dmrginp.nquanta();	
	if(quantaIterator != quantaDist.end()) {
          quantaIterator->second = distribution[i];
        } else {
          quantaDist[quantumNumbers[i]] = distribution[i];
        }
      }
      
      p2out << "\t\t\t Quantum numbers and states used for warm up :: " << endl << "\t\t\t ";
      quantumNumbers.clear(); quantumNumbers.reserve(distribution.size());
      distribution.clear();distribution.reserve(quantumNumbers.size());
      std::map<SpinQuantum, int>::iterator qit = quantaDist.begin();

      for (; qit != quantaDist.end(); qit++) {
	quantumNumbers.push_back( qit->first); distribution.push_back(qit->second); 
	p2out << quantumNumbers.back() << " = " << distribution.back() << ", ";
	if (! (quantumNumbers.size() - 6) % 6) p2out << endl << "\t\t\t ";
      }
      p2out << endl;

      if(dot_with_sys && onedot) {
	newEnvironment.set_integralIndex() = integralIndex;
        newEnvironment.BuildSlaterBlock (environmentSites, quantumNumbers, distribution, haveCompops, haveNormops);
      } else {
	environment.set_integralIndex() = integralIndex;
        environment.BuildSlaterBlock (environmentSites, quantumNumbers, distribution, haveCompops, haveNormops);
      }
    }
  } 
  else {
    p2out << "\t\t\t Restoring block of size " << environmentSites.size () << " from previous iteration" << endl;
    
    if(dot_with_sys && onedot) {
      newEnvironment.set_integralIndex() = integralIndex;
      StackSpinBlock::restore (!forward, environmentSites, newEnvironment, leftState, rightState);
      newEnvironment.set_twoInt(integralIndex);
      if (haveCompops && !newEnvironment.has(CRE_DESCOMP))
	newEnvironment.addAllCompOps();
    } else {
      environment.set_integralIndex() = integralIndex;
      StackSpinBlock::restore (!forward, environmentSites, environment, leftState, rightState);
      environment.set_twoInt(integralIndex);
      if (haveCompops && !environment.has(CRE_DESCOMP))
	environment.addAllCompOps();
    }
    if (dmrginp.outputlevel() > 0)
      mcheck("");
  }

  
 // now initialise newEnvironment
  if (!dot_with_sys || !onedot) {
    environment.addAdditionalOps();

    newEnvironment.set_integralIndex() = integralIndex;
    newEnvironment.default_op_components(direct, haveNormops, haveCompops, leftState==rightState);
    newEnvironment.setstoragetype(DISTRIBUTED_STORAGE);
    newEnvironment.BuildSumBlock (constraint, environment, environmentDot);
    //p2out << "\t\t\t Environment block " << endl << environment << endl;
    //environment.printOperatorSummary();
    //p2out << "\t\t\t NewEnvironment block " << endl << newEnvironment << endl;
    //newEnvironment.printOperatorSummary();
  } 
}


void SpinAdapted::InitBlocks::InitNewOverlapEnvironmentBlock(StackSpinBlock &environment, StackSpinBlock& environmentDot, StackSpinBlock &newEnvironment, 
							     const StackSpinBlock &system, StackSpinBlock &systemDot, int leftState, int rightState,
							     const int &sys_add, const int &env_add, const bool &forward, int integralIndex,
							     const bool &onedot, const bool& dot_with_sys, int constraint)
{
  // now initialise environment Dot
  int systemDotStart, systemDotEnd, environmentDotStart, environmentDotEnd, environmentStart, environmentEnd;
  int systemDotSize = sys_add - 1;
  int environmentDotSize = env_add - 1;
  if (forward)
  {
    systemDotStart = dmrginp.spinAdapted() ? *system.get_sites().rbegin () + 1 : (*system.get_sites().rbegin ())/2 + 1 ;
    systemDotEnd = systemDotStart + systemDotSize;
    environmentDotStart = systemDotEnd + 1;
    environmentDotEnd = environmentDotStart + environmentDotSize;
    environmentStart = environmentDotEnd + 1;
    environmentEnd = dmrginp.spinAdapted() ? dmrginp.last_site() - 1 : dmrginp.last_site()/2 - 1;
  }
  else
  {
    systemDotStart = dmrginp.spinAdapted() ? system.get_sites()[0] - 1 : (system.get_sites()[0])/2 - 1 ;
    systemDotEnd = systemDotStart - systemDotSize;
    environmentDotStart = systemDotEnd - 1;
    environmentDotEnd = environmentDotStart - environmentDotSize;
    environmentStart = environmentDotEnd - 1;
    environmentEnd = 0;
  }

  std::vector<int> environmentSites;
  environmentSites.resize(abs(environmentEnd - environmentStart) + 1);
  for (int i = 0; i < abs(environmentEnd - environmentStart) + 1; ++i) *(environmentSites.begin () + i) = min(environmentStart,environmentEnd) + i;


  p2out << "\t\t\t Restoring block of size " << environmentSites.size () << " from previous iteration" << endl;

  if(dot_with_sys && onedot) {
    newEnvironment.set_integralIndex() = integralIndex;
    StackSpinBlock::restore (!forward, environmentSites, newEnvironment, leftState, rightState);
  }
  else {
    environment.set_integralIndex() = integralIndex;
    StackSpinBlock::restore (!forward, environmentSites, environment, leftState, rightState);
  }
  if (dmrginp.outputlevel() > 0)
    mcheck("");

  // now initialise newEnvironment
  if (!dot_with_sys || !onedot)
  {
    newEnvironment.set_integralIndex() = integralIndex;
    newEnvironment.initialise_op_array(OVERLAP, false);
    //newEnvironment.set_op_array(OVERLAP) = boost::shared_ptr<Op_component<Overlap> >(new Op_component<Overlap>(false));
    newEnvironment.setstoragetype(DISTRIBUTED_STORAGE);
      
    newEnvironment.BuildSumBlock (constraint, environment, environmentDot);
    //p2out << "\t\t\t Environment block " << endl << environment << endl;
    //environment.printOperatorSummary();
    //p2out << "\t\t\t NewEnvironment block " << endl << newEnvironment << endl;
    //newEnvironment.printOperatorSummary();
  }

}

