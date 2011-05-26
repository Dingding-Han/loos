/*
  anm

  (c) 2008 Tod D. Romo, Grossfield Lab
      Department of Biochemistry
      University of Rochster School of Medicine and Dentistry

  Computes the anisotropic network model for a structure.  It does
  this by building a hessian for the structure, then computing the SVD
  of it and the corresponding pseudo-inverse (ignoring the 6 lowest
  modes).

  Usage:
    anm [selection string] radius model-name output-prefix

  Examples:
    anm 'resid >= 10 && resid <= 50 && name == "CA"' 15.0 foo.pdb foo

    This creates the following files:
      foo_H.asc      == The hessian
      foo_U.asc      == Left singular vectors
      foo_s.asc      == Singular values
      foo_V.asc      == Right singular vectors
      foo_Hi.asc     == Pseudo-inverse of H

  Notes:
    o The default selection (if none is specified) is to pick CA's
    o The output is in ASCII format suitable for use with Matlab/Octave/Gnuplot
      
  
*/


/*
  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2008,2009 Tod D. Romo
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

#include <limits>


#include "hessian.hpp"
#include "enm-lib.hpp"
#include "anm-lib.hpp"


using namespace std;
using namespace loos;
using namespace ENM;


namespace opts = loos::OptionsFramework;
namespace po = loos::OptionsFramework::po;


// Globals...  Icky poo!

string prefix;

int verbosity;
bool debug;

string spring_desc;
string bound_spring_desc;

string fullHelpMessage() {

  string s = 
    "\n"
    "Computes the anisotropic network model for a structure.  It does\n"
    "this by building a hessian for the structure, then computing the SVD\n"
    "of it and the corresponding pseudo-inverse (ignoring the 6 lowest\n"
    "modes).\n"
    "\n"
    "This creates the following files:\n"
    "\tfoo_H.asc      == The hessian\n"
    "\tfoo_U.asc      == Left singular vectors\n"
    "\tfoo_s.asc      == Singular values\n"
    "\tfoo_V.asc      == Right singular vectors\n"
    "\tfoo_Hi.asc     == Pseudo-inverse of H\n"
    "\n"
    "\n"
    "* Spring Constant Control *\n\n"
    "The spring constant used is controlled by the --spring option.\n"
    "If only the name for the spring function is given, then the default\n"
    "parameters are used.  Alternatively, the name may include a\n"
    "comma-separated list of parameters to be passed to the spring\n"
    "function, i.e. --spring=distance,15.0\n\n"
    "Available spring functions:\n";

  vector<string> names = springNames();
  for (vector<string>::const_iterator i = names.begin(); i != names.end(); ++i)
    s = s + "\t" + *i + "\n";

  s += 
    "\n\n"
    "Examples:\n\n"
    "Compute the ANM for residues #10 through #50 with a 15 Angstrom cutoff\n"
    "\tanm 'resid >= 10 && resid <= 50 && name == \"CA\"' 15.0 foo.pdb foo\n";

  return(s);
}

// @cond TOOLS_INTERNAL
class ToolOptions : public opts::OptionsPackage {
public:
  
  void addGeneric(po::options_description& o) {
    o.add_options()
      ("debug", po::value<bool>(&debug)->default_value(false), "Turn on debugging (output intermediate matrices)")
      ("spring", po::value<string>(&spring_desc)->default_value("distance"),"Spring function to use")
      ("bound", po::value<string>(&bound_spring_desc), "Bound spring");
  }

  string print() const {
    ostringstream oss;
    oss << boost::format("debug=%d, spring='%s', bound='%s'") % debug % spring_desc % bound_spring_desc;
    return(oss.str());
  }
};

// @endcond



loos::Math::Matrix<int> buildConnectivity(const AtomicGroup& model) {
  uint n = model.size();
  loos::Math::Matrix<int> M(n, n);
  
  for (uint j=0; j<n-1; ++j)
    for (uint i=j; i<n; ++i)
      if (i == j)
        M(j, i) = 1;
      else {
        M(j, i) = model[j]->isBoundTo(model[i]);
        M(i, j) = M(j, i);
      }
  
  return(M);
}



int main(int argc, char *argv[]) {

  string header = invocationHeader(argc, argv);
  
  opts::BasicOptions* bopts = new opts::BasicOptions(fullHelpMessage());
  opts::BasicSelection* sopts = new opts::BasicSelection;
  opts::ModelWithCoords* mopts = new opts::ModelWithCoords;
  ToolOptions* topts = new ToolOptions;
  opts::RequiredArguments* ropts = new opts::RequiredArguments("prefix", "output-prefix");

  opts::AggregateOptions options;
  options.add(bopts).add(sopts).add(mopts).add(topts).add(ropts);
  if (!options.parse(argc, argv))
    exit(-1);

  AtomicGroup model = mopts->model;
  AtomicGroup subset = selectAtoms(model, sopts->selection);

  verbosity = bopts->verbosity;
  prefix = ropts->value("prefix");


  if (verbosity > 0)
    cerr << boost::format("Selected %d atoms from %s\n") % subset.size() % mopts->model_name;

  // Determine which kind of scaling to apply to the Hessian...
  vector<SpringFunction*> springs;
  SpringFunction* spring = 0;
  spring = springFactory(spring_desc);
  springs.push_back(spring);

  vector<SuperBlock*> blocks;
  SuperBlock* blocker = new SuperBlock(spring, subset);
  blocks.push_back(blocker);


  // Handle Decoration (if necessary)
  if (!bound_spring_desc.empty()) {
    if (! model.hasBonds()) {
      cerr << "Error- cannot use bound springs unless the model has connectivity\n";
      exit(-10);
    }
    loos::Math::Matrix<int> M = buildConnectivity(subset);
    SpringFunction* bound_spring = springFactory(bound_spring_desc);
    springs.push_back(bound_spring);

    BoundSuperBlock* decorator = new BoundSuperBlock(blocker, bound_spring, M);
    blocks.push_back(decorator);

    blocker = decorator;
  }


  ANM anm(blocker);
  anm.debugging(debug);
  anm.prefix(prefix);
  anm.meta(header);
  anm.verbosity(verbosity);

  anm.solve();


  // Write out the LSVs (or eigenvectors)
  writeAsciiMatrix(prefix + "_U.asc", anm.eigenvectors(), header, false);
  writeAsciiMatrix(prefix + "_s.asc", anm.eigenvalues(), header, false);

  writeAsciiMatrix(prefix + "_Hi.asc", anm.inverseHessian(), header, false);

  for (vector<SuperBlock*>::iterator i = blocks.begin(); i != blocks.end(); ++i)
    delete *i;
  for (vector<SpringFunction*>::iterator i = springs.begin(); i != springs.end(); ++i)
    delete *i;
  
}
