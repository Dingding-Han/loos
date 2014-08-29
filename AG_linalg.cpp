/*
  AG_linalg.cpp

  Linear-algebra routines (requiring ATLAS or vecLib) for AtomicGroup
  class
*/

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



#include <ios>
#include <sstream>
#include <iomanip>

#include <assert.h>
#include <algorithm>

#include <boost/random.hpp>

#include <AtomicGroup.hpp>



namespace loos {


  std::vector<GCoord> AtomicGroup::momentsOfInertia(void) const throw(loos::NumericalError) {
    Math::Matrix<double, Math::ColMajor> I(3, 3);  // This gets initialized to zero...
    GCoord c = centerOfMass();

    for (uint i = 0; i < atoms.size(); ++i) {

      GCoord u = atoms[i]->coords() - c;
      double m = atoms[i]->mass();
      I(0,0) += m * (u.y() * u.y() + u.z() * u.z());
      I(1,0) += m * u.x() * u.y();
      I(2,0) += m * u.x() * u.z();
      I(1,1) += m * (u.x() * u.x() + u.z() * u.z());
      I(2,1) += m * u.y() * u.z();
      I(2,2) += m * (u.x() * u.x() + u.y() * u.y());
    }

    I(1,0) = I(0,1) = -I(1,0);
    I(2,0) = I(0,2) = -I(2,0);
    I(2,1) = I(1,2) = -I(2,1);

    
    // Now compute the eigen-decomp...
    char jobz = 'V', uplo = 'U';
    f77int nn;
    f77int lda = 3;
    double W[3], work[128];
    f77int lwork = 128;   // ???  Just a guess for sufficient storage
    f77int info;
    nn = 3;

    dsyev_(&jobz, &uplo, &nn, I.get(), &lda, W, work, &lwork, &info);
    if (info < 0)
      throw(NumericalError("dsyev_ reported an argument error.", info));

    if (info > 0)
      throw(NumericalError("dsyev_ failed to converge.", info));

    std::vector<GCoord> results(4);

    for (int i=0; i<3; i++)
      results[2-i] = GCoord( I(0,i), I(1,i), I(2,i) );


    // Now push the eigenvalues on as a GCoord...
    c[0] = W[2];
    c[1] = W[1];
    c[2] = W[0];

    c /= size();

    results[3] = c;

    return(results);
  }


  std::vector<GCoord> AtomicGroup::principalAxes(void) const throw(loos::NumericalError) {
    // Extract out the group's coordinates...
    int i;
    int n = size();
    double M[3] = {0.0, 0.0, 0.0};
    int k = 0;

    double *A = coordsAsArray();
    for (i=0; i<n; i++) {
      M[0] += A[k++];
      M[1] += A[k++];
      M[2] += A[k++];
    }

    M[0] /= n;
    M[1] /= n;
    M[2] /= n;

    // Subtract off the mean...
    for (i=k=0; i<n; i++) {
      A[k++] -= M[0];
      A[k++] -= M[1];
      A[k++] -= M[2];
    }

    // Multiply A*A'...
    double C[9];
#if defined(__linux__) || defined(__CYGWIN__) || defined(__FreeBSD__)
    char ta = 'N';
    char tb = 'T';
    f77int three = 3;
    double zero = 0.0;
    double one = 1.0;

    dgemm_(&ta, &tb, &three, &three, &n, &one, A, &three, A, &three, &zero, C, &three);
    
#else
    cblas_dgemm(CblasColMajor, CblasNoTrans, CblasTrans,
                3, 3, n, 1.0, A, 3, A, 3, 0.0, C, 3);
#endif

    delete[] A;

    // Now compute the eigen-decomp...
    char jobz = 'V', uplo = 'U';
    f77int nn;
    f77int lda = 3;
    double W[3], work[128];
    f77int lwork = 128;   // ???  Just a guess for sufficient storage to be
    // efficient... 
    f77int info;
    nn = 3;

    dsyev_(&jobz, &uplo, &nn, C, &lda, W, work, &lwork, &info);
    if (info < 0)
      throw(NumericalError("dsyev_ reported an argument error.", info));

    if (info > 0)
      throw(NumericalError("dsyev_ failed to converge.", info));

    std::vector<GCoord> results(4);
    GCoord c;

    k = 0;
    for (i=0; i<3; i++) {
      c[0] = C[k++];
      c[1] = C[k++];
      c[2] = C[k++];
      results[2-i] = c;
    }

    // Now push the eigenvalues on as a GCoord...
    c[0] = W[2];
    c[1] = W[1];
    c[2] = W[0];
    c /= size();   // Scale by # of atoms...

    results[3] = c;

    return(results);
  }




  GMatrix AtomicGroup::superposition(const AtomicGroup& grp) throw(loos::NumericalError){
    int i, j;
    XForm W;

    int n = size();
    // Center both groups at the origin...

    GCoord xc = centroid();
    W.translate(-xc);
    double *X = transformedCoordsAsArray(W);
  

    GCoord yc = grp.centroid();
    W.identity();
    W.translate(-yc);
    double *Y = grp.transformedCoordsAsArray(W);

    // Compute correlation matrix...
    double R[9];
#if defined(__linux__) || defined(__CYGWIN__) || defined(__FreeBSD__)
    char ta = 'N';
    char tb = 'T';
    f77int three = 3;
    double one = 1.0;
    double zero = 0.0;
    
    dgemm_(&ta, &tb, &three, &three, &n, &one, X, &three, Y, &three, &zero, R, &three);

#else

    cblas_dgemm(CblasColMajor, CblasNoTrans, CblasTrans, 3, 3, n, 1.0, X, 3, Y, 3, 0.0, R, 3);

#endif

    double det = R[0]*R[4]*R[8] + R[3]*R[7]*R[2] + R[6]*R[1]*R[5] -
      R[0]*R[7]*R[5] - R[3]*R[1]*R[8] - R[6]*R[4]*R[2];

    // Now compute the SVD of R...
    char jobu = 'A', jobvt = 'A';
    f77int m = 3, lda = 3, ldu = 3, ldvt = 3, lwork=100, info;
    double work[lwork];
    f77int nn = 3;
    double S[3], U[9], Vt[9];
  
    dgesvd_(&jobu, &jobvt, &m, &nn, R, &lda, S, U, &ldu, Vt, &ldvt, work, &lwork, &info);
    if (info != 0)
      throw(NumericalError("SVD in AtomicGroup::superposition returned an error", info));


    // if (S[2] < superposition_zero_singular_value)
    //   throw(NumericalError("Superposition is indeterminate...try using more atoms"));

    // Adjust U (if necessary)
    if (det < 0.0) {
      U[6] = -U[6];
      U[7] = -U[7];
      U[8] = -U[8];
    }

    // Compute the rotation matrix...
    double M[9];

#if defined(__linux__) || defined(__CYGWIN__) || defined(__FreeBSD__)

    dgemm_(&ta, &ta, &three, &three, &three, &one, U, &three, Vt, &three, &zero, M, &three);

#else

    cblas_dgemm(CblasColMajor, CblasNoTrans, CblasNoTrans, 3, 3, 3, 1.0, U, 3, Vt, 3, 0.0, M, 3);

#endif

    // Construct the new transformation matrix...  (W = M')
    GMatrix Z;
    for (i=0; i<3; i++)
      for (j=0; j<3; j++)
        Z(i,j) = M[i*3+j];

    //W(0,3) = yc.x();
    //W(1,3) = yc.y();
    //W(2,3) = yc.z();


    W.identity();
    W.translate(yc);
    W.concat(Z);
    W.translate(-xc);

    delete[] X;
    delete[] Y;


    return(W.current());
  }


  GMatrix AtomicGroup::alignOnto(const AtomicGroup& grp) {
    XForm W;
    GMatrix M = superposition(grp);

    W.load(M);
    applyTransform(W);

    return(M);
  }

}
