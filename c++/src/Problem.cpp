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

#include <fstream>
#include <map>
#include <sstream>
#include <vector>

#include <cps/Problem.h>

namespace
{
  // trim from left
  inline std::string& ltrim(std::string& s, const char* t = " \t\n\r\f\v")
  {
    s.erase(0, s.find_first_not_of(t));
    return s;
  }

  // trim from right
  inline std::string& rtrim(std::string& s, const char* t = " \t\n\r\f\v")
  {
    s.erase(s.find_last_not_of(t) + 1);
    return s;
  }

  // trim from left & right
  inline std::string& trim(std::string& s, const char* t = " \t\n\r\f\v")
  {
    return ltrim(rtrim(s, t), t);
  }

  double parseDouble_(const std::string& s)
  {
    std::stringstream ss(s);
    double d;
    ss >> d;
    if (ss.fail())
      throw std::runtime_error("Failed to read double value");

    return d;
  }

  Eigen::VectorXd parseVector_(const std::string& s)
  {
    std::stringstream ss(s);
    double d;
    char c;
    std::vector<double> v;
    
    ss >> c;
    while (c != ']')
    {
      ss >> d;
      ss >> c;
      if (ss.fail())
        throw std::runtime_error("Failed to read vector value");

      v.push_back(d);
    }

    return Eigen::Map<Eigen::VectorXd>(&v[0], static_cast<Eigen::DenseIndex>(v.size()));
  }

  double parseDouble(const std::map<std::string, std::string>& table, const std::string& key, bool optional = false, double defaultValue = 0)
  {
    if (table.count(key))
      return parseDouble_(table.at(key));
    else
    {
      if (optional)
        return defaultValue;
      else
        throw std::runtime_error("No element " + key + " found in the file");
    }
  }

  Eigen::VectorXd parseVector(const std::map<std::string, std::string>& table, const std::string& key, bool optional = false)
  {
    if (table.count(key))
      return parseVector_(table.at(key));
    else
    {
      if (optional)
        return Eigen::VectorXd();
      else
        throw std::runtime_error("No element " + key + " found in the file");
    }
  }
}

namespace cps
{
  void RawProblem::read(const std::string& filepath)
  {
    std::ifstream aif(filepath);
    if (aif.is_open())
    {
      std::map<std::string, std::string> table;
      for (std::string line; std::getline(aif, line); )
      {
        auto i = line.find("=");
        if (i != std::string::npos && i>0)
        {
          std::string substr = line.substr(0, i);
          std::string token = trim(substr);
          auto j = line.find(";", i + 1);
          if (j != std::string::npos)
          {
            substr = line.substr(i + 1, j - i - 1);
            std::string value = trim(substr);
            table[token] = value;
          }
          else
            throw std::runtime_error("error in reading line\n" + line);
        }
      }

      g = parseDouble(table, "g");
      lambda_min = parseDouble(table, "lambda_min");
      lambda_max = parseDouble(table, "lambda_max");
      delta = parseVector(table, "Delta");
      init_omega_min = parseDouble(table, "omega_i_min");
      init_omega_max = parseDouble(table, "omega_i_max");
      init_zbar = parseDouble(table, "z_bar");
      init_zbar_deriv = parseDouble(table, "zd_bar");
      target_height = parseDouble(table, "z_f");
      Phi_ = parseVector(table, "Phi", true);
    }
    else
    {
      throw std::runtime_error("Unable to open" + filepath);
    }
  }

  Problem::Problem(const RawProblem& pb)
    : lso_(pb.delta)
    , lc_(pb.lambda_min*pb.delta, pb.lambda_max*pb.delta, pb.init_omega_min*pb.init_omega_min, pb.init_omega_max*pb.init_omega_max)
    , bc_(pb.delta, pb.init_zbar / pb.g, pb.init_zbar_deriv / pb.g)
    , raw_(pb)
  {
    double d = pb.delta[0] * pb.g / pb.target_height;
    lc_.changeBounds(0, d, d);
  }

  const LeastSquareObjective& Problem::objective() const
  {
    return lso_;
  }

  LeastSquareObjective& Problem::objective()
  {
    return lso_;
  }

  const BoundenessConstraint& Problem::nonLinearConstraint() const
  {
    return bc_;
  }

  BoundenessConstraint& Problem::nonLinearConstraint()
  {
    return bc_;
  }

  const LinearConstraints& Problem::linearConstraints() const
  {
    return lc_;
  }

  LinearConstraints& Problem::linearConstraints()
  {
    return lc_;
  }

  Eigen::VectorXd::Index Problem::size() const
  {
    return raw_.delta.size();
  }

  void Problem::set_target_height(double target_height)
  {
    raw_.target_height = target_height;
    computeAndSetBounds0();
  }

  void Problem::set_init_zbar(double init_zbar)
  {
    raw_.init_zbar = init_zbar;
    computeAndSetAlpha();
  }

  void Problem::set_init_zbar_deriv(double init_zbar_deriv)
  {
    raw_.init_zbar_deriv = init_zbar_deriv;
    computeAndSetb();
  }

  void Problem::set_lambda_min(double lambda_min)
  {
    raw_.lambda_min = lambda_min;
    computeAndSetZonotopeBounds();
  }

  void Problem::set_lambda_max(double lambda_max)
  {
    raw_.lambda_max = lambda_max;
    computeAndSetZonotopeBounds();
  }

  void Problem::set_lambdas(double lambda_min, double lambda_max)
  {
    raw_.lambda_min = lambda_min;
    raw_.lambda_max = lambda_max;
    computeAndSetZonotopeBounds();
  }

  void Problem::set_init_omega_min(double init_omega_min)
  {
    raw_.init_omega_min = init_omega_min;
    computeAndSetBoundsN();
  }

  void Problem::set_init_omega_max(double init_omega_max)
  {
    raw_.init_omega_max = init_omega_max;
    computeAndSetBoundsN();
  }

  void Problem::set_init_omega(double init_omega_min, double init_omega_max)
  {
    raw_.init_omega_min = init_omega_min;
    raw_.init_omega_max = init_omega_max;
    computeAndSetBoundsN();
  }

  void Problem::computeAndSetBounds0()
  {
    double d = raw_.delta[0] * raw_.g / raw_.target_height;
    lc_.changeBounds(0, d, d);
  }

  void Problem::computeAndSetZonotopeBounds()
  {
    lc_.changeBounds(raw_.lambda_min*raw_.delta, raw_.lambda_max*raw_.delta);
    computeAndSetBounds0();
  }

  void Problem::computeAndSetBoundsN()
  {
    lc_.changeBounds(raw_.delta.size(), raw_.init_omega_min*raw_.init_omega_min, raw_.init_omega_max*raw_.init_omega_max);
  }

  void Problem::computeAndSetAlpha()
  {
    bc_.setAlpha(raw_.init_zbar / raw_.g);
  }

  void Problem::computeAndSetb()
  {
    bc_.setb(raw_.init_zbar_deriv / raw_.g);
  }

  void Problem::precompute()
  {
    objective().precompute(1);
  }
}
