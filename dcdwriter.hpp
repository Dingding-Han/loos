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



#if !defined(DCDWRITER_HPP)
#define DCDWRITER_HPP

#include <ios>
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <exception>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <loos_defs.hpp>

#include <AtomicGroup.hpp>
#include <dcd.hpp>

namespace loos {

  //! A <I>very</I> lightweight class for writing simple DCDs
  class DCDWriter : public boost::noncopyable {

    // Use a union to convert data to appropriate type...
    typedef union { unsigned int ui; int i; char c[4]; float f; } DataOverlay; 

  public:

    //! Setup for writing to a file named by \a s
    /** You can opt to write the header explicitly, i.e.
     * \code
     DCDWriter dcd("output.dcd");
     dcd.setHeader(500, 10, 1e-3, no);
     dcd.setTitles("MY TITLE HERE");
     dcd.writeHeader();
     dcd.writeFrame(frame);
     \endcode
     * Or you can let dcdwriter create the header from the first frame written...
     */
    explicit DCDWriter(const std::string& s, const bool append = false) : _natoms(0), _nsteps(0),
                                                                          _timestep(0.001), _current(0),
                                                                          _has_box(false), nodelete(false),
                                                                          _header_written(false)
    {
      struct stat statbuf;

      if (append && !stat(s.c_str(), &statbuf)) {
        readExistingHeader(s);
        openStream(s, true);
      } else {
        openStream(s);
        _titles.push_back("AUTO GENERATED BY LOOS");
      }

    }

    //! Setup for writing to a stream
    explicit DCDWriter(std::iostream& fs) : _natoms(0), _nsteps(0), _timestep(0.001), _current(0),
                                            _has_box(false), _stream(&fs), nodelete(true), _header_written(false) {
      _titles.push_back("AUTO GENERATED BY LOOS");
    }

    //! Writes the coordinates of \a grps to file \a s as a DCD.
    DCDWriter(const std::string& s, const std::vector<AtomicGroup>& grps, const bool append = false) :
      _natoms(grps[0].size()),
      _nsteps(grps.size()),
      _timestep(1e-3),
      _current(0),
      _has_box(grps[0].isPeriodic()),
      nodelete(false),
      _header_written(false)
    {
      struct stat statbuf;

      if (append && !stat(s.c_str(), &statbuf)) {
        readExistingHeader(s);
        openStream(s, true);
      } else {
        openStream(s);
        _titles.push_back("AUTO GENERATED BY LOOS");
      }
    
      writeHeader();
      writeFrame(grps);
    }

    //! Writes coordinates of \a grps adding \a comment as a TITLE record
    DCDWriter(const std::string& s, const std::vector<AtomicGroup>& grps, const std::string& comment, const bool append = false) :
      _natoms(grps[0].size()),
      _nsteps(grps.size()),
      _timestep(1e-3),
      _current(0),
      _has_box(grps[0].isPeriodic()),
      nodelete(false),
      _header_written(false)
    {
      struct stat statbuf;
      
      if (append && !stat(s.c_str(), &statbuf)) {
        readExistingHeader(s);
        openStream(s, true);
      } else {
        openStream(s);
        _titles.push_back(comment);
      }
    
      writeHeader();
      writeFrame(grps);
    }

    //! Writes coordinates of \a grps adding \a comments as TITLE records
    DCDWriter(const std::string& s, const std::vector<AtomicGroup>& grps, const std::vector<std::string>& comments, const bool append = false) :
      _natoms(grps[0].size()),
      _nsteps(grps.size()),
      _timestep(1e-3),
      _current(0),
      _has_box(grps[0].isPeriodic()),
      nodelete(false),
      _header_written(false)
    {
      struct stat statbuf;

      if (append && !stat(s.c_str(), &statbuf)) {
        readExistingHeader(s);
        openStream(s, true);
      } else
        openStream(s);

      _titles = comments;

      writeHeader();
      writeFrame(grps);
    }

    ~DCDWriter() {
      if (!nodelete)
        delete _stream;
    }


    //! Sets header parameters
    /** These must be set prior to writing a header or frame, but it is
     *  not an error to not do so...
     *  Arguments:
     *  \arg \c na Number of atoms
     *  \arg \c ns Number of steps (total frames)
     *  \arg \c ts Timestep of each frame
     *  \arg \c bf Flag for whether or not each frame will include periodic box info as Xtal data
     */
    void setHeader(const int na, const int ns, const greal ts, const bool bf) {
      if (_header_written)
        throw(std::logic_error("Cannot set header after having written it"));

      _natoms = na;
      _nsteps = ns;
      _timestep = ts;
      _has_box = bf;
    }

    void setTitles(const std::vector<std::string>& titles) {
      if (_header_written)
        throw(std::logic_error("Cannot set header after having written it"));
      _titles = titles;
    }

    void setTitle(const std::string& s) {
      if (_header_written)
        throw(std::logic_error("Cannot set header after having written it"));
      _titles.clear(); addTitle(s);
    }

    void addTitle(const std::string& s) {
      if (_header_written)
        throw(std::logic_error("Cannot set header after having written it"));
      _titles.push_back(s);
    }

    //! Writes a frame or a group of frames to a growing DCD
    /** writeFrame() will automatically extend the DCD for you if you
     *  write past the initially specified number of frames.
     *  Alternatively, you can just begin writing frames without
     *  explicitly writing a header and let writeFrame() handle it for
     *  you.  As the DCD grows, writeFrame() will automatically update
     *  the header information for you.
     */
    void writeFrame(const AtomicGroup& grp);
    void writeFrame(const std::vector<AtomicGroup>& grps);

    void writeHeader(void);

    int framesWritten(void) const { return(_current); }

  private:
    void writeF77Line(const char* const data, const unsigned int len); 
    std::string fixStringSize(const std::string& s, const unsigned int size);
    void writeBox(const GCoord& box);

    void readExistingHeader(const std::string& fname);

    void openStream(const std::string& fname, const bool append = false) {
      std::ios_base::openmode mode = std::ios_base::out | std::ios_base::binary;
      if (append)
        mode |= std::ios_base::in;
      else
        mode |= std::ios_base::trunc;

      _stream = new std::fstream(fname.c_str(), mode);
      if (!_stream->good())
        throw(std::runtime_error("Error while opening output DCD file"));
      std::streampos p = _stream->tellg();
      nodelete = false;
    }

  private:
    uint _natoms, _nsteps;
    greal _timestep;
    uint _current;
    bool _has_box;
    std::iostream* _stream;
    bool nodelete;
    bool _header_written;
    std::vector<std::string> _titles;
  };

}


#endif
