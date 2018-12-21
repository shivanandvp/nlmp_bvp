// ===============================
// Includes and global definitions
// ===============================
#include <cmath>                                // C++ analog to math.h
#include <Eigen/Eigen>                          // For matrix and vector math
#include <boost/numeric/odeint.hpp>             // For the initial value problem solver
#include <nlmp_bvp.hpp>                         // For the function declarations   
using namespace std;                            //
using namespace Eigen;                          //
using namespace boost::numeric::odeint;         //
using RowVectorXi = Matrix<int, 1, Dynamic>;    // For the convenience of declaring row vectors
using RowVectorXd = Matrix<double, 1, Dynamic>; // For the convenience of declaring row vectors
const double EPSILON = 1e-8;                    // EPSILON = the state perturbation parameter to probe the differential equation system with
const double ALPHA = 1;                         // ALPHA   = the relaxation factor to scale the adjustment to the initial condition
const double SIGMA = 1e-14;                     // SIGMA   = the tolerance for error outside which the solver needs to  iterate further. 
const double BETA = 1e-3;                       // BETA    = the deflation factor
// ===============================

// ===================
// Function "nlmp_bvp"
// ===================
BVPSolution nlmp_bvp(
    int n,                                 // n              = the number of differential equations = the number of boundary conditions
    int m,                                 // m              = the number of nodes at which boundary conditions are specified
    int nGrid,                             // nGrid          = the number of points at which the state is evaluated
    RowVectorXd t_BC,                      // t_BC           = row vector of values at which the boundary conditions are specified           -- (1xm)
    VectorXd _0x_t1,                       // _0x_t1         = column vector of the guessed initial state                                    -- (nx1)    
    VectorXd dxBydt(double t, VectorXd x), // dxBydt         = a function that defines the derivative of a state vector x at t               -- (nx1)
    VectorXd BCResidue(MatrixXd x_BC)      // BCResidue      = a function that defines the boundary condition residues at state vectors x_BC -- (nx1) 
    const IVAMParameters ivamParameters    // ivamParameters = parameters for the Initial Value Adjusting Method (IVAM)
    ){  
        int m;
        int k = 0;     
        int IVPIColumnIndex = 0;
        int IVPPColumnIndex = 0;        
        int nGrid = 0;  
        double kEpsilon = 0;
        double kAlpha = ALPHA;
        double t0;
        double tm;    
        double kG, kP1G;
        m = t_BC.cols();  
        RowVectorXd IVPITSolutions, IVPPTSolutions;
        VectorXd kX0;
        VectorXd kX0Temp;
        VectorXd pX;
        VectorXd gkX0;
        MatrixXd S;         
        MatrixXd IVPIXSolutions, IVPPXSolutions; 
        runge_kutta_dopri5<VectorXd,double,VectorXd,double,vector_space_algebra> IVPIStepper;
        runge_kutta_dopri5<VectorXd,double,VectorXd,double,vector_space_algebra> IVPPStepper;

        t0 = t_BC(0);
        tm = t_BC(m-1);
        nGrid = floor((tm - t0)/h) + 1;
        IVPIXSolutions.resize(n, nGrid);
        IVPITSolutions.resize(1, nGrid);
        IVPPXSolutions.resize(n, nGrid);
        IVPPTSolutions.resize(1, nGrid);
        kX0.resize(n,1);
        kX0Temp.resize(n,1);
        gkX0.resize(n,1);
        pX.resize(n,1);
        S.resize(n,n);       
        kX0 = x0;

        // Capture function calls by the ODEInt library for differentials and convert it to a custom form 
        auto dFunctionWrapper = [dxBydt] (const VectorXd &x, VectorXd &dxdt, double t){
            dxdt = dxBydt(t, x);
        };

        // Observer to handle the solutions of the IVP Solver
        auto odeIObserver = [k, n, &IVPIColumnIndex, &IVPIXSolutions, &IVPITSolutions] (const VectorXd &x , const double t){
            IVPIXSolutions.col(IVPIColumnIndex) = x;
            IVPITSolutions(IVPIColumnIndex) = t;
            ++IVPIColumnIndex;            
        };    
        auto odePObserver = [k, n, &IVPPColumnIndex, &IVPPXSolutions, &IVPPTSolutions] (const VectorXd &x , const double t){
            IVPPXSolutions.col(IVPPColumnIndex) = x;
            IVPPTSolutions(IVPPColumnIndex) = t;
            ++IVPPColumnIndex;
        };      

        auto getBCs = [n, m, t_BC] (RowVectorXd IVPTSolutions, MatrixXd xSolutions) -> MatrixXd{
            return xSolutions(Eigen::all, ((t_BC-t_BC(0)*RowVectorXd::Ones(m))/h).array().round().cast<int>());
        };        

        // Solve the initial value problem for the first time  
        kX0Temp = kX0;
        cout<<"Starting I solver..."<<endl;
        integrate_const(IVPIStepper, dFunctionWrapper, kX0Temp, t0, tm, h, odeIObserver); 
        if(IVPIColumnIndex < nGrid){
            cout<<"OMG! I Solver is still running..."<<" k = "<<-1<<"... IVPIColumnIndex = "<<IVPIColumnIndex<<endl;
            return 0;
        }        
        IVPIColumnIndex = 0;  
        gkX0 = BCResidue(getBCs(IVPITSolutions, IVPIXSolutions));
        // cout<<"gkX0 = "<<endl<<gkX0<<endl;
        kP1G = gkX0.norm();
        kG = kP1G;
        while(kP1G > SIGMA){  
            if(kP1G < 0.1*kG) {
                cout<<"Going too fast. Changing ALPHA from "<<kAlpha<<" to "<<fmin(1.2*kAlpha, 1.0)<<"..."<<endl;
                kAlpha = fmin(1.2*kAlpha, 1.0);                
            } else if(kP1G >= kG){
                cout<<"Oops. Error increased. To make it go faster, changing ALPHA from "<<kAlpha<<" to "<<0.8*kAlpha<<"..."<<endl;
                kAlpha = 0.8*kAlpha;
            }     
            for(int j = 0; j < n; j++){   
                // Determine the perturbation parameter
                //kEpsilon = max(EPSION, abs(EPSION * kX0(j)));
                kEpsilon = EPSION;
            
                // Perturb the initial conditions            
                pX = kX0 + kEpsilon*MatrixXd::Identity(n,n).col(j);
                
                // Solve the perturbed initial value problem            
                integrate_const(IVPPStepper, dFunctionWrapper, pX, t0, tm, h, odePObserver);
                if(IVPPColumnIndex < nGrid){
                    cout<<"OMG! P Solver is still running..."<<" k = "<<k<<"... IVPPColumnIndex = "<<IVPPColumnIndex<<endl;
                    return 0;
                }    
                IVPPColumnIndex = 0;

                // Compute a column of the adjusting matrix                
                S.col(j) = (BCResidue(getBCs(IVPPTSolutions, IVPPXSolutions))- gkX0)/kEpsilon;                
                // cout<<"gkxp = "<<endl<<BCResidue(getBCs(IVPPTSolutions, IVPPXSolutions))<<endl;
            }
            // cout<<"S = "<<endl<<S<<endl;

            // Solve the linarized adjusting equation
            kX0 = S.colPivHouseholderQr().solve(-kAlpha*gkX0) + kX0;
            cout<<"Change in x0 = "<<endl<<S.completeOrthogonalDecomposition().solve(-kAlpha*gkX0)<<endl;            
            //kX0 = kX0 - kAlpha*S.inverse()*gkX0;

            // Solve the initial value problem   
            kX0Temp = kX0;
            integrate_const(IVPIStepper, dFunctionWrapper, kX0Temp, t0, tm, h, odeIObserver);  
            if(IVPIColumnIndex < nGrid){
                cout<<"OMG! I Solver is still running..."<<" k = "<<-1<<"... IVPIColumnIndex = "<<IVPIColumnIndex<<endl;
                return 0;
            }    
            IVPIColumnIndex = 0; 
            gkX0 = BCResidue(getBCs(IVPITSolutions, IVPIXSolutions));

            kG = kP1G;
            kP1G = gkX0.norm()/sqrt(n);
            if(kP1G > kG){
                cout<<"-> kP1G > kG... kP1G = "<<kP1G<<"... kG = "<<kG<<endl;
                //cout<<"S = "<<endl<<S<<endl;
                //cout<<"S^-1 = "<<endl<<S.inverse()<<endl;
            }
            cout<<"kP1G = "<<kP1G<<endl;
            ++k;
        }  
        cout<<"t(0) = "<<endl<<IVPITSolutions.col(0)<<endl;
        cout<<"t(nGrid-1) = "<<endl<<IVPITSolutions.col(nGrid-1)<<endl;
        cout<<"x(0) = "<<endl<<IVPIXSolutions.col(0)<<endl;
        cout<<"x(nGrid-1) = "<<endl<<IVPIXSolutions.col(nGrid-1)<<endl;
        //x0 = x0 + EPSION*MatrixXd::Identity(n,n).col(0);
        // pX = kX0 + kEpsilon*MatrixXd::Identity(n,n).col(0);
        // integrate_const(IVPPStepper, dFunctionWrapper, x0, t0, tm, h, odePObserver);
        // IVPPColumnIndex = 0; 

        // Perturb the initial conditions            
        // x0 = x0 + kEpsilon*MatrixXd::Identity(n,n).col(1);                
        // Solve the perturbed initial value problem            
        // integrate_const(IVPPStepper, dFunctionWrapper, x0, t0, tm, h, odePObserver);
        // IVPPColumnIndex = 0;
        // cout<<"Check BCs = "<<endl<<BCResidue(getBCs(IVPPTSolutions, IVPPXSolutions))<<endl;
        return 0;
    }
// ===================


