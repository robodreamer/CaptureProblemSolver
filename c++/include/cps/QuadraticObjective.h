#pragma once
/* Copyright 2018 CNRS-AIST JRL, CNRS-UM LIRMM
 *
 * This file is part of CPS.
 *
 * CPS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CPS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with CPS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Eigen/Core>

#include <cps/cps_api.h>
#include <cps/CondensedOrthogonalMatrix.h>
#include <cps/defs.h>
#include <cps/QRAlgorithms.h>

namespace cps
{
  /** A class for the computations related to the objective function ||J x||^2.*/
  class CPS_DLLAPI LeastSquareObjective
  {
  public:
    using Index = Eigen::DenseIndex;

    /** Build the objective from the vector delta.*/
    LeastSquareObjective(const Eigen::VectorXd& delta);

    /** Size of the problem.*/
    Index size() const;

    /** Evaluate the objective at x.*/
    double value(const VectorConstRef& x) const;

    /** Y = J X
      * Note that the constness of Y is a trick to allow the compiler to accept 
      * temporary views (like block). The constness is not honored.
      */
    template<typename Derived1, typename Derived2>
    void applyJToTheLeft(const Eigen::MatrixBase<Derived1>& Y, const Eigen::MatrixBase<Derived2>& X) const;
    /** Y = J^T X
    * Note that the constness of Y is a trick to allow the compiler to accept
    * temporary views (like block). The constness is not honored.
    */
    template<typename Derived1, typename Derived2>
    void applyJTransposeToTheLeft(const Eigen::MatrixBase<Derived1>& Y, const Eigen::MatrixBase<Derived2>& X) const;


    /** Returns the QR decomposition of J_A for the given active-set act.
      * The computation is performed only of no precomputation were carried,
      * otherwise the precomputed decomposition is returned.
      *
      * If J_A is a subset of (continuous) rows in a bigger matrix, shift is
      * the number of line above J_A.
      */
    void qr(MatrixRef R, CondensedOrthogonalMatrix& Q, const std::vector<bool>& act, Index shift = 0) const;
    /** Same as above, were the number of active constraints in act is already
      * given by nact, to  avoid re-scanning the vector.
      */
    void qr(MatrixRef R, CondensedOrthogonalMatrix& Q, Index nact, const std::vector<bool>& act, Index shift = 0) const;

    /** Return J. For Debug*/
    Eigen::MatrixXd matrix() const;
    /** Return J*Na. For debug*/
    Eigen::MatrixXd projectedMatrix(const std::vector<bool>& act) const;
    /** Same as above, where the number of active constraint in act is given by nact.*/
    Eigen::MatrixXd projectedMatrix(Index nact, const std::vector<bool>& act) const;

    /** Given e = d_(dstart:dend), build a matrix with main body of the form
      *  S x
      *  x + *
      *    * + *
      *      ...
      *       * + y
      *         y E
      * where S and E are specified by startType and endType, the [x * * ... * y] 
      * are a sub-vector of e, and the + are such that a line * + * (or x + * or
      * y + *) has the form [e_i  -e_i-e_{i+1}  e_{i+1}] .
      * startType:
      * -1: S = [e_0; -e_0 - e_1],  x = e_1
      * -2: S = -e_0 - e_1,         x = e_1
      * -3: S = -e_0,               x = e_0 
      * stopType:
      * -1: E = [same; e_k],        y = e_{k-1}
      * -2: E = same,               y = e_{k-1}
      * -3: E = [same, e_k],        y = e_{k-1}
      * -4: E = same,               y = e_k (except for corner cases, this means E=-e_k)
      * where same means that the value in place is not changed.
      */
    void buildJj(MatrixRef Jj, Index dstart, Index dend, StartType startType, EndType endType) const;
    
    /** Return the QR of the above matrix.
      * The GivensSequence Q is extended by extend.
      */
    void qrJj(MatrixRef R, GivensSequence& Q, Index extend, Index dstart, Index dend, StartType startType, EndType endType) const;

    /** Precompute the decompositions for all the possible active set value.*/
    void precompute(Index shift);

  private:
    /** Precomputed QR decomposition.*/
    struct Precomputation
    {
      Eigen::MatrixXd R;
      CondensedOrthogonalMatrix Q;
    };

    /** Performe the QR decomposition of J_A in qr(R,Q,act,shift) when it was not precomputed. */
    void qrComputation(MatrixRef R, CondensedOrthogonalMatrix& Q, const std::vector<bool>& act, Index shift = 0) const;
    /** Same as above for qr(R,Q,nact,act,shift)*/
    void qrComputation(MatrixRef R, CondensedOrthogonalMatrix& Q, Index nact, const std::vector<bool>& act, Index shift = 0) const;

    //data
    Eigen::DenseIndex n_;
    Eigen::VectorXd delta_;
    Eigen::VectorXd d_;     // 1/delta

    //computation data
    mutable Eigen::VectorXd e_;
    SpecialQR qr_;
    bool precomputed_;

    //precomputations
    std::vector<Precomputation> precomputations_;
  };


  template<typename Derived1, typename Derived2>
  inline void LeastSquareObjective::applyJToTheLeft(const Eigen::MatrixBase<Derived1>& Y_, const Eigen::MatrixBase<Derived2>& X) const
  {
    Eigen::MatrixBase<Derived1>& Y = const_cast<Eigen::MatrixBase<Derived1>&>(Y_);

    assert(X.rows() == n_);
    assert(Y.rows() == n_ - 1 && Y.cols() == X.cols());

    Y.row(0) = d_[1] * X.row(1) - (d_[0] + d_[1])*X.row(0);
    for (Index i = 1; i < n_ - 1; ++i)
      Y.row(i) = d_[i] * X.row(i - 1) - (d_[i] + d_[i + 1])*X.row(i) + d_[i + 1] * X.row(i + 1);
  }


  template<typename Derived1, typename Derived2>
  inline void LeastSquareObjective::applyJTransposeToTheLeft(const Eigen::MatrixBase<Derived1>& Y_, const Eigen::MatrixBase<Derived2>& X) const
  {
    Eigen::MatrixBase<Derived1>& Y = const_cast<Eigen::MatrixBase<Derived1>&>(Y_);


    assert(X.rows() == n_ - 1);
    assert(Y.rows() == n_ && Y.cols() == X.cols());

    Y.row(0) = d_[1] * X.row(1) - (d_[0] + d_[1])*X.row(0);
    for (Index i = 1; i < n_ - 2; ++i)
      Y.row(i) = d_[i] * X.row(i - 1) - (d_[i] + d_[i + 1])*X.row(i) + d_[i + 1] * X.row(i + 1);
    Y.row(n_ - 2) = d_[n_ - 2] * X.row(n_ - 3) - (d_[n_ - 2] + d_[n_ - 1])*X.row(n_ - 2);
    Y.row(n_ - 1) = d_[n_ - 1] * X.row(n_ - 2);
  }
}
