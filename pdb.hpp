/*
  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2008, Tod D. Romo, Alan Grossfield
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




#if !defined(PDB_HPP)
#define PDB_HPP




#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>

#include <tr1/unordered_set>

#include <loos_defs.hpp>
#include <Atom.hpp>
#include <AtomicGroup.hpp>
#include <pdb_remarks.hpp>
#include <cryst.hpp>
#include <utils.hpp>


//! PDB reading/writing class
/**This class models a basic PDB file format.  Special handling is
 *included for dealing with periodic boundary conditions.  If there is
 *a special REMARK header, then the box size is extracted from this
 *and stored in the parent AtomicGroup.  If not, but a CRYST1 record
 *is present, then the a, b, c parameters are used to set the periodic
 *box.
 *
 * This class can handle some minor variations in the PDB format
 * (since there are so many different standards out there).  This is
 * determined by the strictness_policy setting.  Be default, a weak
 * strictness is used.  This will allow variations like frame-shifts
 * in the resid field to be accepted.  If you would rather have the
 * strict formatting honored, you'll need to set each PDB object to
 * strict:
\verbatim
PDB pdb;
pdb.strict(true);
\endverbatim
 */
class PDB : public AtomicGroup {
public:
  class BadConnectivity : public std::runtime_error {
  public:
    explicit BadConnectivity(const std::string& msg) : runtime_error(msg) { };
  };

public:
  PDB() : _show_charge(false), _auto_ter(true), _has_cryst(false), strictness_policy(false) { }
  virtual ~PDB() {}

  //! Read in PDB from a filename
  explicit PDB(const std::string fname) : _show_charge(false), _auto_ter(true),
                                     _has_cryst(false), strictness_policy(false) {
    std::ifstream ifs(fname.c_str());
    if (!ifs)
      throw(std::runtime_error("Cannot open PDB file " + fname));
    read(ifs);
  }

  //! Read in a PDB from a filename
  explicit PDB(const char* fname) : _show_charge(false), _auto_ter(true),
                                    _has_cryst(false), strictness_policy(false) {
    std::ifstream ifs(fname);
    if (!ifs)
      throw(std::runtime_error("Cannot open PDB file " + std::string(fname)));
    read(ifs);
  }

  //! Read in a PDB from an ifstream
  explicit PDB(std::ifstream& ifs) : _show_charge(false), _auto_ter(true),
                                _has_cryst(false), strictness_policy(false) { read(ifs); }


  //! Clones an object for polymorphism (see AtomicGroup::clone() for more info)
  virtual PDB* clone(void) const {
    return(new PDB(*this));
  }

  //! Creates a deep copy (see AtomicGroup::copy() for more info)
  PDB copy(void) const {
    AtomicGroup grp = this->AtomicGroup::copy();
    PDB p(grp);

    p._show_charge = _show_charge;
    p._auto_ter = _auto_ter;
    p._has_cryst = _has_cryst;
    p._remarks = _remarks;
    p.cell = cell;

    return(p);
  }

  //! Class method for creating a PDB from an AtomicGroup
  /** There should probably be some internal checks to make sure we
   *  have enough info to actually write out a PDB, but currently
   *  there are no such checks...
   */
  static PDB fromAtomicGroup(const AtomicGroup& g) {
    PDB p(g);
    
    return(p);
  }

  bool showCharge(void) const { return(_show_charge); }
  //! Special handling for charges since the PDB form is daft
  void showCharge(bool b = true) { _show_charge = b; }

  bool strict(void) const { return(strictness_policy); }
  //! Determins how strict the input parser is to the '96 PDB standard.
  void strict(const bool b) { strictness_policy = b; }

  bool autoTerminate(void) const { return(_auto_ter); }

  //! Automatically insert TER record at end of output.
  /*! Controls whether or not a "TER" record is automatically added
      when printing out the group/PDB
  */
  void autoTerminate(bool b = true) { _auto_ter = b; }

  //! Accessor for the remarks object...
  Remarks& remarks(void) { return(_remarks); }
  //! Accessor for the remarks object...
  void remarks(const Remarks& r) { _remarks = r; }

  UnitCell& unitCell(void) { return(cell); }
  void unitCell(const UnitCell& c) { cell = c; }

  //! Output as a PDB
  friend std::ostream& operator<<(std::ostream& os, PDB& p);

  //! Read in PDB from an ifstream
  void read(std::istream& is);

private:

  //! Create a PDB from an AtomicGroup (i.e. upcast)
  PDB(const AtomicGroup& grp) : AtomicGroup(grp), _show_charge(false), _auto_ter(true), _has_cryst(false) { }

  // Internal routines for parsing...
  greal parseFloat(const std::string&, const uint, const uint);
  greal parseFloat(const std::string&);
  gint parseInt(const std::string&, const uint, const uint);
  gint parseInt(const std::string&);
  std::string parseString(const std::string&, const uint, const uint);
  bool emptyString(const std::string&);

  // These will modify the PDB upon a successful parse...
  void parseRemark(const std::string&);
  void parseAtomRecord(const std::string&);
  void parseConectRecord(const std::string&);
  void parseCryst1Record(const std::string&);

  // Convert an Atom to a string representation in PDB format...
  std::string atomAsString(const pAtom p) const;

  friend std::ostream& FormatConectRecords(std::ostream&, PDB&);

private:
  bool _show_charge;
  bool _auto_ter;
  bool _has_cryst;
  bool strictness_policy;
  Remarks _remarks;
  UnitCell cell;

};



#endif

