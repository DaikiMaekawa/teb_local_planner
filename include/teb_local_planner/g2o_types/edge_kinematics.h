/*********************************************************************
 *
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2016,
 *  TU Dortmund - Institute of Control Theory and Systems Engineering.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the institute nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 * 
 * Notes:
 * The following class is derived from a class defined by the
 * g2o-framework. g2o is licensed under the terms of the BSD License.
 * Refer to the base class source for detailed licensing information.
 *
 * Author: Christoph Rösmann
 *********************************************************************/

#ifndef _EDGE_KINEMATICS_H
#define _EDGE_KINEMATICS_H

#include <teb_local_planner/g2o_types/vertex_pose.h>
#include <teb_local_planner/g2o_types/penalties.h>
#include <teb_local_planner/teb_config.h>

#include "g2o/core/base_binary_edge.h"

#include <cmath>

namespace teb_local_planner
{

/**
 * @class EdgeKinematicsDiffDrive
 * @brief Edge defining the cost function for satisfying the non-holonomic kinematics of a differential drive mobile robot.
 * 
 * The edge depends on two vertices \f$ \mathbf{s}_i, \mathbf{s}_{ip1} \f$ and minimizes a geometric interpretation
 * of the non-holonomic constraint: 
 * 	- C. Rösmann et al.: Trajectory modification considering dynamic constraints of autonomous robots, ROBOTIK, 2012.
 * 
 * The \e weight can be set using setInformation(): Matrix element 1,1: (Choose a very high value: ~1000). \n
 * A second equation is implemented to penalize backward motions (second element of the error /cost vector). \n
 * The \e weight can be set using setInformation(): Matrix element 2,2: (A value ~1 allows backward driving, but penalizes it slighly). \n
 * The dimension of the error / cost vector is 2: the first component represents the nonholonomic constraint cost, 
 * the second one backward-drive cost.
 * @see TebOptimalPlanner::AddEdgesKinematics, EdgeKinematicsCarlike
 * @remarks Do not forget to call setTebConfig()
 */    
class EdgeKinematicsDiffDrive : public g2o::BaseBinaryEdge<2, double, VertexPose, VertexPose>
{
public:
  
  /**
   * @brief Construct edge.
   */  
  EdgeKinematicsDiffDrive()
  {
      this->setMeasurement(0.);
      _vertices[0] = _vertices[1] = NULL;
  }
  
  /**
   * @brief Destruct edge.
   * 
   * We need to erase vertices manually, since we want to keep them even if TebOptimalPlanner::clearGraph() is called.
   * This is necessary since the vertices are managed by the Timed_Elastic_Band class.
   */   
  virtual ~EdgeKinematicsDiffDrive()
  {
    for(unsigned int i=0;i<2;i++) 
    {
      if(_vertices[i])
        _vertices[i]->edges().erase(this);
    }
  }

  /**
   * @brief Actual cost function
   */    
  void computeError()
  {
    ROS_ASSERT_MSG(cfg_, "You must call setTebConfig on EdgeKinematicsDiffDrive()");
    const VertexPose* conf1 = static_cast<const VertexPose*>(_vertices[0]);
    const VertexPose* conf2 = static_cast<const VertexPose*>(_vertices[1]);
    
    Eigen::Vector2d deltaS = conf2->position() - conf1->position();

    // non holonomic constraint
    _error[0] = fabs( ( cos(conf1->theta())+cos(conf2->theta()) ) * deltaS[1] - ( sin(conf1->theta())+sin(conf2->theta()) ) * deltaS[0] );

    // positive-drive-direction constraint
    Eigen::Vector2d angle_vec ( cos(conf1->theta()), sin(conf1->theta()) );	   
    _error[1] = penaltyBoundFromBelow(deltaS.dot(angle_vec), 0,0);
    // epsilon=0, otherwise it pushes the first bandpoints away from start

    ROS_ASSERT_MSG(std::isfinite(_error[0]) && std::isfinite(_error[1]), "EdgeKinematicsDiffDrive::computeError() _error[0]=%f _error[1]=%f\n",_error[0],_error[1]);
  }

#ifdef USE_ANALYTIC_JACOBI
#if 1
  /**
   * @brief Jacobi matrix of the cost function specified in computeError().
   */
  void linearizeOplus()
  {
    ROS_ASSERT_MSG(cfg_, "You must call setTebConfig on EdgeKinematicsDiffDrive()");
    const VertexPose* conf1 = static_cast<const VertexPose*>(_vertices[0]);
    const VertexPose* conf2 = static_cast<const VertexPose*>(_vertices[1]);
    
    Eigen::Vector2d deltaS = conf2->position() - conf1->position();
	    
    double cos1 = cos(conf1->theta());
    double cos2 = cos(conf2->theta());
    double sin1 = sin(conf1->theta());
    double sin2 = sin(conf2->theta());
    double aux1 = sin1 + sin2;
    double aux2 = cos1 + cos2;
    
    double dd_error_1 = deltaS[0]*cos1;
    double dd_error_2 = deltaS[1]*sin1;
    double dd_dev = penaltyBoundFromBelowDerivative(dd_error_1+dd_error_2, 0,0);
    
    double dev_nh_abs = g2o::sign( ( cos(conf1->theta())+cos(conf2->theta()) ) * deltaS[1] - 
	      ( sin(conf1->theta())+sin(conf2->theta()) ) * deltaS[0] );
	    
    // conf1
    _jacobianOplusXi(0,0) = aux1 * dev_nh_abs; // nh x1
    _jacobianOplusXi(0,1) = -aux2 * dev_nh_abs; // nh y1
    _jacobianOplusXi(1,0) = -cos1 * dd_dev; // drive-dir x1
    _jacobianOplusXi(1,1) = -sin1 * dd_dev; // drive-dir y1
    _jacobianOplusXi(0,2) = (-dd_error_2 - dd_error_1) * dev_nh_abs; // nh angle
    _jacobianOplusXi(1,2) = ( -sin1*deltaS[0] + cos1*deltaS[1] ) * dd_dev; // drive-dir angle1
    
    // conf2
    _jacobianOplusXj(0,0) = -aux1 * dev_nh_abs; // nh x2
    _jacobianOplusXj(0,1) = aux2 * dev_nh_abs; // nh y2
    _jacobianOplusXj(1,0) = cos1 * dd_dev; // drive-dir x2
    _jacobianOplusXj(1,1) = sin1 * dd_dev; // drive-dir y2
    _jacobianOplusXj(0,2) = (-sin2*deltaS[1] - cos2*deltaS[0]) * dev_nh_abs; // nh angle
    _jacobianOplusXj(1,2) = 0; // drive-dir angle1					
  }
#endif
#endif
    
  /**
  * @brief Compute and return error / cost value.
  * 
  * This method is called by TebOptimalPlanner::computeCurrentCost to obtain the current cost.
  * @return 2D Cost / error vector [nh cost, backward drive dir cost]^T
  */     
  ErrorVector& getError()
  {
    computeError();
    return _error;
  }

  /**
   * @brief Read values from input stream
   */    
  virtual bool read(std::istream& is)
  {
    is >> _measurement;
    //inverseMeasurement() = measurement() * -1;
    is >> information()(0,0);
    return true;
  }

  /**
   * @brief Write values to an output stream
   */    
  virtual bool write(std::ostream& os) const
  {
    //os << measurement() << " ";
    os << information()(0,0) << " Error NH-Constraint: " << _error[0] << ", Error PosDriveDir: " << _error[1];
    return os.good();
  }

  /**
   * @brief Assign the TebConfig class for parameters.
   * @param cfg TebConfig class
   */ 
  void setTebConfig(const TebConfig& cfg)
  {
    cfg_ = &cfg;
  }

protected:
  
  const TebConfig* cfg_; //!< Store TebConfig class for parameters
  
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW   
};




/**
 * @class EdgeKinematicsCarlike
 * @brief Edge defining the cost function for satisfying the non-holonomic kinematics of a carlike mobile robot.
 * 
 * The edge depends on two vertices \f$ \mathbf{s}_i, \mathbf{s}_{ip1} \f$ and minimizes a geometric interpretation
 * of the non-holonomic constraint: 
 *  - C. Rösmann et al.: Trajectory modification considering dynamic constraints of autonomous robots, ROBOTIK, 2012.
 * 
 * The definition is identically to the one of the differential drive robot.
 * Additionally, this edge incorporates a minimum turning radius that is required by carlike robots.
 * The turning radius is defined by \f$ r=v/omega \f$.
 * 
 * The \e weight can be set using setInformation(): Matrix element 1,1: (Choose a very high value: ~1000). \n
 * The second equation enforces a minimum turning radius.
 * The \e weight can be set using setInformation(): Matrix element 2,2. \n
 * The dimension of the error / cost vector is 3: the first component represents the nonholonomic constraint cost, 
 * the second one backward-drive cost and the third one the minimum turning radius
 * @see TebOptimalPlanner::AddEdgesKinematics, EdgeKinematicsDiffDrive
 * @remarks Bounding the turning radius from below is not affected by the penalty_epsilon parameter, 
 *          the user might add an extra margin to the min_turning_radius param.
 * @remarks Do not forget to call setTebConfig()
 */    
class EdgeKinematicsCarlike : public g2o::BaseBinaryEdge<2, double, VertexPose, VertexPose>
{
public:
  
  /**
   * @brief Construct edge.
   */  
  EdgeKinematicsCarlike()
  {
      this->setMeasurement(0.);
      _vertices[0] = _vertices[1] = NULL;
  }
  
  /**
   * @brief Destruct edge.
   * 
   * We need to erase vertices manually, since we want to keep them even if TebOptimalPlanner::clearGraph() is called.
   * This is necessary since the vertices are managed by the Timed_Elastic_Band class.
   */   
  virtual ~EdgeKinematicsCarlike()
  {
    for(unsigned int i=0;i<2;i++) 
    {
      if(_vertices[i])
        _vertices[i]->edges().erase(this);
    }
  }

  /**
   * @brief Actual cost function
   */    
  void computeError()
  {
    ROS_ASSERT_MSG(cfg_, "You must call setTebConfig on EdgeKinematicsCarlike()");
    const VertexPose* conf1 = static_cast<const VertexPose*>(_vertices[0]);
    const VertexPose* conf2 = static_cast<const VertexPose*>(_vertices[1]);
    
    Eigen::Vector2d deltaS = conf2->position() - conf1->position();

    // non holonomic constraint
    _error[0] = fabs( ( cos(conf1->theta())+cos(conf2->theta()) ) * deltaS[1] - ( sin(conf1->theta())+sin(conf2->theta()) ) * deltaS[0] );

    // limit minimum turning radius
    double omega_t = g2o::normalize_theta( conf2->theta() - conf1->theta() );
    if (omega_t == 0)
      _error[1] = 0; // straight line motion
    else
      _error[1] = penaltyBoundFromBelow(deltaS.norm() / fabs(omega_t), cfg_->robot.min_turning_radius, 0.0); 
    // This edge is not affected by the epsilon parameter, the user might add an exra margin to the min_turning_radius parameter.
    
    ROS_INFO_STREAM("omega_t = " << omega_t);
    ROS_INFO_STREAM("error = (" << _error[0] << ", " << _error[1] << ")");


    ROS_ASSERT_MSG(std::isfinite(_error[0]) && std::isfinite(_error[1]), "EdgeKinematicsCarlike::computeError() _error[0]=%f _error[1]=%f\n",_error[0],_error[1]);
  }
    
  /**
  * @brief Compute and return error / cost value.
  * 
  * This method is called by TebOptimalPlanner::computeCurrentCost to obtain the current cost.
  * @return 2D Cost / error vector [nh cost, backward drive dir cost]^T
  */     
  ErrorVector& getError()
  {
    computeError();
    return _error;
  }

  /**
   * @brief Read values from input stream
   */    
  virtual bool read(std::istream& is)
  {
    is >> _measurement;
    //inverseMeasurement() = measurement() * -1;
    is >> information()(0,0);
    return true;
  }

  /**
   * @brief Write values to an output stream
   */    
  virtual bool write(std::ostream& os) const
  {
    //os << measurement() << " ";
    os << information()(0,0) << " Error NH-Constraint: " << _error[0] << ", Error PosDriveDir: " << _error[1];
    return os.good();
  }

  /**
   * @brief Assign the TebConfig class for parameters.
   * @param cfg TebConfig class
   */ 
  void setTebConfig(const TebConfig& cfg)
  {
    cfg_ = &cfg;
  }

protected:
  
  const TebConfig* cfg_; //!< Store TebConfig class for parameters
  
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW   
};




} // end namespace

#endif
