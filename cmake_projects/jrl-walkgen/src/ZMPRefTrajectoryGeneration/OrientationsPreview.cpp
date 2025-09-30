/*
 * Copyright 2010, 
 *
 * Andrei   Herdt
 * Francois Keith
 * Olivier  Stasse
 *
 * JRL, CNRS/AIST
 *
 * This file is part of walkGenJrl.
 * walkGenJrl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * walkGenJrl is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Lesser Public License for more details.
 * You should have received a copy of the GNU Lesser General Public License
 * along with walkGenJrl.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Research carried out within the scope of the 
 *  Joint Japanese-French Robotics Laboratory (JRL)
 */
/*
 * OrientationsPreview.cpp
 */

#include <iostream>
#include <fstream>
#include <Debug.hh>
#include <ZMPRefTrajectoryGeneration/OrientationsPreview.hh>

using namespace PatternGeneratorJRL;
using namespace std;

const double OrientationsPreview::EPS_ = 0.00000001;

OrientationsPreview::OrientationsPreview( CjrlHumanoidDynamicRobot *aHS)
{
  CjrlJoint * waist     = aHS->waist();

  // left hip
  CjrlJoint * leftFoot  = aHS->leftFoot()->associatedAnkle();
  CjrlJoint * leftHip   = aHS->jointsBetween(*waist, *leftFoot)[1];

  lLimitLeftHipYaw_  = leftHip->lowerBound(0);//-30.0/180.0*M_PI;
  uLimitLeftHipYaw_  = leftHip->upperBound(0);//45.0/180.0*M_PI;
  if (lLimitLeftHipYaw_==  uLimitLeftHipYaw_)
    {
      lLimitLeftHipYaw_ = -30.0/180.0*M_PI;
      uLimitLeftHipYaw_ = 45.0/180.0*M_PI;
    }

  // right hip
  CjrlJoint * rightFoot = aHS->rightFoot()->associatedAnkle();
  CjrlJoint * rightHip  = aHS->jointsBetween(*waist, *rightFoot)[1];

  lLimitRightHipYaw_ = rightHip->lowerBound(0);//-45.0/180.0*M_PI;
  uLimitRightHipYaw_ = rightHip->upperBound(0);//30.0/180.0*M_PI;
  if (lLimitRightHipYaw_==  uLimitRightHipYaw_)
    {
      lLimitRightHipYaw_ = -30.0/180.0*M_PI;
      uLimitRightHipYaw_ = 45.0/180.0*M_PI;
    }

  uvLimitFoot_ = fabs(leftHip->upperVelocityBound(0));

  //Acceleration limit not given by HRP2JRLmain.wrl
  uaLimitHipYaw_ = 0.1;
  //Maximal cross angle between the feet
  uLimitFeet_ = 5.0/180.0*M_PI;
}

OrientationsPreview::~OrientationsPreview() {
}

void
OrientationsPreview::preview_orientations(double Time,
                                          const reference_t & Ref,
                                          double StepDuration,
                                          const std::deque<FootAbsolutePosition> & LeftFootPositions_deq,
                                          const std::deque<FootAbsolutePosition> & RightFootPositions_deq,
                                          solution_t & Solution)
{
  const deque<support_state_t> & PrwSupportStates_deq = Solution.SupportStates_deq;
  std::deque<double> & PreviewedSupportAngles_deq = Solution.SupportOrientations_deq;
  std::deque<double> & PreviewedTrunkOrientations_deq = Solution.TrunkOrientations_deq;

  support_state_t CurrentSupport = PrwSupportStates_deq.front();

  // Verify the acceleration of the hip joint
  verify_acceleration_hip_joint(Ref, CurrentSupport);

  // Current foot position
  const FootAbsolutePosition & LeftFoot = LeftFootPositions_deq.back();
  const FootAbsolutePosition & RightFoot = RightFootPositions_deq.back();

  bool TrunkVelOK = false;
  bool TrunkAngleOK = false;
  
  // In case of double support the next support angle is fixed
  // ds -> FirstFootPreviewed == 0
  // ss -> FirstFootPreviewed == 1
  double FirstFootPreviewed = 0.0;

  signRotVelTrunk_ = (TrunkStateT_.yaw[1] < 0.0)?-1.0:1.0;

  unsigned StepNumber = 0;

  // Parameters of the trunk polynomial (fourth order)
  double a,b,c,d,e;

  // Trunk angle at the end of the current support phase
  double PreviewedTrunkAngleEnd;

  while(!TrunkVelOK)
    {
      // Initialize support orientation:
      // -------------------------------
      double CurrentSupportAngle;
      if (CurrentSupport.Foot == LEFT)
        CurrentSupportAngle = LeftFootPositions_deq[0].theta*M_PI/180.0;
      else
        CurrentSupportAngle = RightFootPositions_deq[0].theta*M_PI/180.0;


      // (Re)Compute the trunk orientation at the end of the acceleration phase:
      // -----------------------------------------------------------------------
      if(CurrentSupport.Phase != DS)
        {
          TrunkAngleOK = false;
          while(!TrunkAngleOK)
            {
              if (fabs(TrunkStateT_.yaw[1]-TrunkState_.yaw[1]) > EPS_)
                {
                  a = TrunkState_.yaw[0];
                  b = TrunkState_.yaw[1];
                  c = 0.0;
                  d = 3.0*(TrunkStateT_.yaw[1]-TrunkState_.yaw[1]) / (T_*T_);
                  e = -2.0*d/(3.0*T_);
                  TrunkStateT_.yaw[0] = a + b*T_+1.0/2.0*c*T_*T_+1.0/3.0*d*T_*T_*T_+1.0/4.0*e*T_*T_*T_*T_;
                }
              else
                TrunkStateT_.yaw[0] = TrunkState_.yaw[0] + TrunkState_.yaw[1]*T_;
              //Compute the trunk angle at the end of the support phase
              SupportTimePassed_ = CurrentSupport.TimeLimit-Time;
              PreviewedTrunkAngleEnd = TrunkStateT_.yaw[0] + TrunkStateT_.yaw[1]*(SupportTimePassed_-T_);

              //Verify the angle between the support foot and the trunk at the end of the current support period
              TrunkAngleOK = verify_angle_hip_joint(CurrentSupport, PreviewedTrunkAngleEnd, TrunkState_, TrunkStateT_, CurrentSupportAngle, StepNumber);
            }
        }
      else//The trunk does not rotate in the DS phase
        {
          SupportTimePassed_ = CurrentSupport.TimeLimit+SSPeriod_-Time;
          FirstFootPreviewed = 1;
          PreviewedSupportAngles_deq.push_back(CurrentSupportAngle);
          TrunkStateT_.yaw[0] = PreviewedTrunkAngleEnd = TrunkState_.yaw[0];
        }

      // Initialize variables in the orientations preview loop:
      // ------------------------------------------------------
      double PreviousSupportAngle = CurrentSupportAngle;
      double PreviewedSupportFoot;
      if(CurrentSupport.Foot == LEFT)
        PreviewedSupportFoot = 1.0;
      else
        PreviewedSupportFoot = -1.0;
      double CurrentLeftFootAngle = LeftFoot.theta*M_PI/180.0;
      double CurrentRightFootAngle = RightFoot.theta*M_PI/180.0;
      double CurrentLeftFootVelocity = LeftFoot.dtheta*M_PI/180.0;
      double CurrentRightFootVelocity = RightFoot.dtheta*M_PI/180.0;

      // Preview of orientations:
      // ------------------------
      for(StepNumber = (unsigned) FirstFootPreviewed;
          StepNumber <= (unsigned)((int)ceil((N_+1)*T_/StepDuration));
          StepNumber++)
        {
          PreviewedSupportFoot = -PreviewedSupportFoot;
          //compute the optimal support orientation
          double PreviewedSupportAngle = PreviewedTrunkAngleEnd + TrunkStateT_.yaw[1]*SSPeriod_/2.0;

          verify_velocity_hip_joint(Time,
              PreviewedSupportFoot, PreviewedSupportAngle,
              StepNumber, CurrentSupport,
              CurrentRightFootAngle, CurrentLeftFootAngle,
              CurrentLeftFootVelocity, CurrentRightFootVelocity);

          //Check the feet angles to avoid self-collision:
          if ((double)PreviewedSupportFoot*(PreviousSupportAngle-PreviewedSupportAngle)-EPS_ > uLimitFeet_)
            PreviewedSupportAngle = PreviousSupportAngle+(double)signRotVelTrunk_*uLimitFeet_;
          //not being able to catch-up for a rectangular DS phase
          else if (fabs(PreviewedSupportAngle-PreviousSupportAngle) > uvLimitFoot_*SSPeriod_)
            PreviewedSupportAngle = PreviousSupportAngle+(double)PreviewedSupportFoot * uvLimitFoot_*(SSPeriod_-T_);

          // Verify orientation of the hip joint at the end of the support phase
          TrunkAngleOK = verify_angle_hip_joint( CurrentSupport, PreviewedTrunkAngleEnd,
              TrunkState_, TrunkStateT_,
              CurrentSupportAngle, StepNumber);

          if(!TrunkAngleOK)
            {
              PreviewedSupportAngles_deq.clear();
              TrunkVelOK = false;
              break;
            }
          else
            PreviewedSupportAngles_deq.push_back(PreviewedSupportAngle);

          //Prepare for the next step
          PreviewedTrunkAngleEnd = PreviewedTrunkAngleEnd + SSPeriod_*TrunkStateT_.yaw[1];
          PreviousSupportAngle = PreviewedSupportAngle;

          if(PreviewedSupportFoot == 1)
            CurrentLeftFootAngle = PreviewedSupportAngle;
          else
            CurrentRightFootAngle = PreviewedSupportAngle;

          TrunkVelOK = true;
        }

    }

  // PREVIEW TRUNK AND SUPPORT ORIENTATIONS:
  // ---------------------------------------
  PreviewedTrunkOrientations_deq.push_back(TrunkState_.yaw[0]);
  PreviewedTrunkOrientations_deq.push_back(TrunkStateT_.yaw[0]);
  unsigned j = 0;
  for(unsigned i = 1; i<N_; i++ )
    {
      PreviewedTrunkOrientations_deq.push_back(TrunkStateT_.yaw[0]+TrunkStateT_.yaw[1]*T_);
    }

  std::deque<support_state_t>::iterator prwSS_it = Solution.SupportStates_deq.begin();
  double supportAngle = prwSS_it->Yaw;
  prwSS_it++;//Point at the first previewed instant
  for(unsigned i = 0; i<N_; i++ )
    {
      if(prwSS_it->StateChanged)
        {
          supportAngle = Solution.SupportOrientations_deq[j];
          j++;
        }
      prwSS_it->Yaw = supportAngle;
      prwSS_it++;
    }

}


void
OrientationsPreview::verify_acceleration_hip_joint(const reference_t & Ref,
    const support_state_t & CurrentSupport)
{
  if(CurrentSupport.Phase != DS)
    //Verify change in velocity reference against the maximal acceleration of the hip joint
    if(fabs(Ref.Local.Yaw-TrunkState_.yaw[1]) > 2.0/3.0*T_*uaLimitHipYaw_)
      {
        double signRotAccTrunk = (Ref.Local.Yaw-TrunkState_.yaw[1] < 0.0)?-1.0:1.0;
        TrunkStateT_.yaw[1] = TrunkState_.yaw[1] + signRotAccTrunk * 2.0/3.0*T_* uaLimitHipYaw_;
      }
    else
      TrunkStateT_.yaw[1] = Ref.Local.Yaw;
  else//No rotations in a double support phase
    TrunkStateT_.yaw[1] = 0.0;
}


bool
OrientationsPreview::verify_angle_hip_joint(const support_state_t & CurrentSupport,
    double PreviewedTrunkAngleEnd,
    const COMState &TrunkState_, COMState &TrunkStateT_,
    double CurrentSupportFootAngle,
    unsigned StepNumber)
{
  //Which limitation is relevant in the current situation?
  double uJointLimit, lJointLimit, JointLimit;
  if(CurrentSupport.Foot == LEFT)
    {
      uJointLimit = uLimitLeftHipYaw_;
      lJointLimit = lLimitLeftHipYaw_;
    }
  else
    {
      uJointLimit = uLimitRightHipYaw_;
      lJointLimit = lLimitRightHipYaw_;
    }
  JointLimit = (TrunkStateT_.yaw[1] < 0.0)?lJointLimit:uJointLimit;

  // Determine a new orientation if limit violated
  if (fabs(PreviewedTrunkAngleEnd - CurrentSupportFootAngle)>fabs(JointLimit))
    {
      TrunkStateT_.yaw[1] = (CurrentSupportFootAngle+0.9*JointLimit-TrunkState_.yaw[0]-TrunkState_.yaw[1]*T_/2.0)/(SupportTimePassed_+StepNumber*SSPeriod_-T_/2.0);
      return false;
    }
  else
  {
    return true;
  }
}


void
OrientationsPreview::verify_velocity_hip_joint(double Time,
    double PreviewedSupportFoot, double PreviewedSupportAngle,
    unsigned StepNumber, const support_state_t & CurrentSupport,
    double CurrentRightFootAngle, double CurrentLeftFootAngle,
    double CurrentLeftFootVelocity, double CurrentRightFootVelocity)
{
  double CurrentAngle;
  if(PreviewedSupportFoot==1)
    CurrentAngle = CurrentLeftFootAngle;
  else
    CurrentAngle = CurrentRightFootAngle;

  // Parameters
  double a,b,c,d,T;
  //To be implemented
  //For the
  if(StepNumber>0 && CurrentSupport.Phase == SS)
    {
      //verify the necessary, maximal, relative foot velocity
      double MeanFootVelDifference = (PreviewedSupportAngle-CurrentAngle)/(SSPeriod_-T_);
      //If necessary reduce the velocity to the maximum
      if (3.0/2.0*fabs(MeanFootVelDifference) > uvLimitFoot_)
        {
          MeanFootVelDifference = 2.0/3.0*(double)signRotVelTrunk_ * uvLimitFoot_;
          //Compute the resulting angle
          PreviewedSupportAngle = CurrentAngle+MeanFootVelDifference*(SSPeriod_-T_);
        }
    }
  else if((StepNumber==0 && CurrentSupport.Phase == SS) || (StepNumber==1 && CurrentSupport.Phase == DS))
    {
      T = CurrentSupport.TimeLimit-Time-T_;

      //Previewed polynome
      a = CurrentAngle;
      if(PreviewedSupportFoot==1)
        b = CurrentLeftFootVelocity;
      else
        b = CurrentRightFootVelocity;
      c = (3.0*PreviewedSupportAngle-3.0*a-2.0*b*T)/(T*T);
      d = (-b*T+2*a-2*PreviewedSupportAngle)/(T*T*T);

      //maximal speed violated
      double temp;
      if(d==0)
        temp = 0;
      else
        temp = -1.0/3.0*c/d;

      if(df(a,b,c,d,temp)>uvLimitFoot_)
        {
          a = 0;
          c = -1.0/(2.0*T)*(2.0*b-2.0*uvLimitFoot_+2.0*sqrt(uvLimitFoot_*uvLimitFoot_-b*uvLimitFoot_));
          d = (-2.0*c-b/T)/(3.0*T);
          PreviewedSupportAngle = f(a,b,c,d,T);
        }
      
    }

}


void
OrientationsPreview::interpolate_trunk_orientation(double Time, int CurrentIndex,
    double NewSamplingPeriod,
    const deque<support_state_t> & PrwSupportStates_deq,
    deque<COMState> & FinalCOMTraj_deq)
{

  support_state_t CurrentSupport = PrwSupportStates_deq.front();

  if(CurrentSupport.Phase == SS && Time+3.0/2.0*T_ < CurrentSupport.TimeLimit)
    {
      //Fourth order polynomial parameters
      double a =  TrunkState_.yaw[1];
      double c = 3.0*(TrunkStateT_.yaw[1]-TrunkState_.yaw[1])/(T_*T_);
      double d = -2.0*c/(3.0*T_);

      double tT;
      double Theta = TrunkState_.yaw[0];

      FinalCOMTraj_deq[CurrentIndex].yaw[0] = TrunkState_.yaw[0];
      FinalCOMTraj_deq[CurrentIndex].yaw[1] = TrunkState_.yaw[1];
      //Interpolate the
      for(int k = 0; k<(int)(T_/NewSamplingPeriod);k++)
        {
          tT = (double)(k+1)*NewSamplingPeriod;
          //interpolate the orientation of the trunk
          if(fabs(TrunkStateT_.yaw[1]-TrunkState_.yaw[1])-0.000001 > 0)
            {
              TrunkState_.yaw[0] = (((1.0/4.0*d*tT+1.0/3.0*c)*
                  tT)*tT+a)*tT+Theta;
              TrunkState_.yaw[1] = ((d*tT+c)*tT)*tT+a;
              TrunkState_.yaw[2] = (3.0*d*tT+2.0*c)*tT;
            }
          else
            {
              TrunkState_.yaw[0] += NewSamplingPeriod*TrunkStateT_.yaw[1];
            }
          FinalCOMTraj_deq[CurrentIndex+k].yaw[0] = TrunkState_.yaw[0];
          FinalCOMTraj_deq[CurrentIndex+k].yaw[1] = TrunkState_.yaw[1];
        }
    }
  else if (CurrentSupport.Phase == DS || Time+3.0/2.0*T_ > CurrentSupport.TimeLimit)
    {
      for(int k = 0; k<(int)(T_/NewSamplingPeriod);k++)
        {
          FinalCOMTraj_deq[CurrentIndex+k].yaw[0] = TrunkState_.yaw[0];
          FinalCOMTraj_deq[CurrentIndex+k].yaw[1] = TrunkState_.yaw[1];
        }
    }

}


double
OrientationsPreview::f(double a,double b,double c,double d,double x)
{return a+b*x+c*x*x+d*x*x*x;}


double
OrientationsPreview::df(double ,double b,double c,double d,double x)
{return b+2*c*x+3.0*d*x*x;}



