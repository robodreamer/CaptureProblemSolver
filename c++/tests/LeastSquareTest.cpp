#include <LeastSquare.h>
#include <QuadraticObjective.h>

#include <iostream>

// boost
#define BOOST_TEST_MODULE LeastSquareTests
#include <boost/test/unit_test.hpp>

using namespace Eigen;
using namespace bms;

BOOST_AUTO_TEST_CASE(LeastSquareFeasibilityTest)
{
  VectorXd l = -VectorXd::Random(10).cwiseAbs();
  VectorXd u = VectorXd::Random(10).cwiseAbs();
  LinearConstraints lc(l, u, -1, 1);

  VectorXd j = VectorXd::Random(10);
  double c = -10;

  LeastSquare ls(10);
  auto s = ls.solveFeasibility(j, c, lc);
  auto x = ls.x();
  auto lambda = ls.lambda();

  BOOST_CHECK(s == SolverStatus::Converge);
  BOOST_CHECK(lc.checkPrimal(x));
  BOOST_CHECK(lc.checkDual(lambda));
  VectorXd kkt = (c + j.dot(x))*j + lc.matrix().transpose()*lambda;
  BOOST_CHECK(kkt.isZero(1e-8));
  VectorXd Cx = lc.matrix()*x;
  for (DenseIndex i = 0; i < 10; ++i)
  {
    if (lambda(i) > 0)
    {
      BOOST_CHECK(std::abs(lambda(i)*(Cx(i) - u(i))) <= 1e-12);
    }
    else
    {
      BOOST_CHECK(std::abs(lambda(i)*(Cx(i) - l(i))) <= 1e-12);
    }
  }
  if (lambda(10) > 0)
  {
    BOOST_CHECK(std::abs(lambda(10)*(Cx(10) - 1)) <= 1e-12);
  }
  else
  {
    BOOST_CHECK(std::abs(lambda(10)*(Cx(10) + 1)) <= 1e-12);
  }
}

BOOST_AUTO_TEST_CASE(LeastSquareTest)
{
  VectorXd l = -VectorXd::Random(10).cwiseAbs();
  VectorXd u = VectorXd::Random(10).cwiseAbs();
  LinearConstraints lc(l, u, -1, 1);

  VectorXd j = 100*VectorXd::Random(10);
  double c = -200;

  VectorXd delta = VectorXd::LinSpaced(10, 0.01, 0.19);
  LeastSquareObjective obj(delta);
  MatrixXd J = obj.matrix();

  LeastSquare ls(10);

  auto s = ls.solve(obj, j, c, lc);

  auto x = ls.x();
  auto lambda = ls.lambda();

  //std::cout << x.transpose() << std::endl;
  //std::cout << lambda.transpose() << std::endl;

  double tl = 1e-8*lambda.lpNorm<Infinity>();
  BOOST_CHECK(s == SolverStatus::Converge);
  BOOST_CHECK(lc.checkPrimal(x));
  BOOST_CHECK(lc.checkDual(lambda, tl));
  VectorXd kkt = (c + j.dot(x))*j + J.transpose()*(J*x) + lc.matrix().transpose()*lambda;
  BOOST_CHECK(kkt.isZero(1e-8));
  VectorXd Cx = lc.matrix()*x;
  for (DenseIndex i = 0; i < 10; ++i)
  {
    if (lambda(i) > 0)
    {
      BOOST_CHECK(std::abs(lambda(i)*(Cx(i) - u(i))) <= tl);
    }
    else
    {
      BOOST_CHECK(std::abs(lambda(i)*(Cx(i) - l(i))) <= tl);
    }
  }
  if (lambda(10) > 0)
  {
    BOOST_CHECK(std::abs(lambda(10)*(Cx(10) - 1)) <= tl);
  }
  else
  {
    BOOST_CHECK(std::abs(lambda(10)*(Cx(10) + 1)) <= tl);
  }
}