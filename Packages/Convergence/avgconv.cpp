/*
  avgconv

  Usage- avgconv model traj selection range
*/



/*

  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2011, Tod D. Romo
  Department of Biochemistry and Biophysics
  School of Medicine & Dentistry, University of Rochester

  This package (LOOS) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation under version 3 of the License.

  This package is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/





#include <loos.hpp>
#include <boost/format.hpp>

using namespace std;
using namespace loos;


bool locally_optimal = false;



string fullHelpMessage(void) {
  string msg =
    "\n"
    "SYNOPSIS\n"
    "\tConvergence of the average structure\n"
    "\n"
    "DESCRIPTION\n"
    "\n"
    "\tThe convergence of the average structure from a trajectory is determined by first\n"
    "dividing the trajectory into blocks.  An average structure is computed for the i'th and\n"
    "the i+1'th block.  These two average structure are superimposed using a Kabsch alignment\n"
    "algorithm.  The RMSD is calculated.  This is then repeated for all blocks.\n"
    "\tInitially, the whole trajectory is optimally aligned first using an iterative alignment\n"
    "process (described in Grossfield, et al. Proteins 67, 31–40 (2007)).  Optionally,\n"
    "each block may be optimally aligned independently by using the 'locally optimal' flag.\n"
    "\n"
    "EXAMPLES\n"
    "\n"
    "\tavgconv sim.psf traj.dcd 'segid == \"PE1\"' >avgconv.asc\n"
    "This example uses automatic block-sizes for the subsamples and calculates the RMSD and\n"
    "superpositions using all atoms from the PE1 segment.  The output is placed in avgconv.asc\n"
    "\n"
    "\tavgconv sim.psf traj.dcd 'name == \"CA\"' 10:10:500 >avgconv.asc\n"
    "This example uses all alpha-carbons and explicitly sets the block sizes to 10,\n"
    "20, ..., 100\n"
    "\n"
    "\tavgconv sim.psf traj.dcd '!hydrogen' 10:10:500 1 >avgconv.asc\n"
    "This example uses all non-hydrogen atoms with block sizes of 10, 20, 30, ..., 5000,\n"
    "and the blocks are all iteratively aligned prior to computing the average.\n"
    "SEE ALSO\n"
    "\tblock_average, block_avgconv\n";

  return(msg);
}


AtomicGroup calcAverage(const vector<AtomicGroup>& ensemble, const uint size) {

  vector<AtomicGroup> subsample(size);
  for (uint i=0; i<size; ++i)
    subsample[i] = ensemble[i];

  if (locally_optimal)
    (void)iterativeAlignment(subsample);

  AtomicGroup avg = averageStructure(subsample);
  return(avg);
}



int main(int argc, char *argv[]) {
  if (argc < 4 || argc > 6) {
    cerr << "Usage- avgconv model traj selection [range [1 = local optimal avg]]\n";
    cerr << fullHelpMessage();
    exit(-1);
  }

  string hdr = invocationHeader(argc, argv);
  cout << "# " << hdr << endl;
  cout << "# n\trmsd\n";

  int k = 1;
  AtomicGroup model = createSystem(argv[k++]);
  pTraj traj = createTrajectory(argv[k++], model);
  string sel = string(argv[k++]);
  
  vector<uint> blocks;
  if (argc == 4) {

    uint step = traj->nframes() / 100;
    if (step == 0) {
      cerr << "Error- too few frames for auto-blocksizes.  Please specify block sizes explicitly\n";
      exit(-1);
    }
    for (uint i=step; i<traj->nframes(); i += step)
      blocks.push_back(i);

  } else {

    blocks = parseRangeList<uint>(argv[k++]);
    if (argc == k+1)
      locally_optimal = (argv[k][0] == '1');
  }
  
  AtomicGroup subset = selectAtoms(model, sel);
  cout << boost::format("# Subset has %d atoms\n") % subset.size();
  vector<AtomicGroup> ensemble;
  readTrajectory(ensemble, subset, traj);
  cout << boost::format("# Trajectory has %d frames\n") % ensemble.size();

  cout << boost::format("# Blocks = %d\n") % blocks.size();

  if (!locally_optimal) {
    boost::tuple<vector<XForm>, greal, int> result = iterativeAlignment(ensemble);
    cout << boost::format("# Iterative alignment converged to RMSD of %g with %d iterations\n") % boost::get<1>(result) % boost::get<2>(result);
  }

  AtomicGroup preceding = calcAverage(ensemble, blocks[0]);
  for (vector<uint>::const_iterator ci = blocks.begin()+1; ci != blocks.end(); ++ci) {
    AtomicGroup avg = calcAverage(ensemble, *ci);
    avg.alignOnto(preceding);
    double rmsd = preceding.rmsd(avg);

    cout << boost::format("%d\t%f\n") % *ci % rmsd;

    preceding = avg;
  }

}

