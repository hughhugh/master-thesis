//******************************************************************************
//*  FILE:    singleSchlogl.cpp
//*
//*  AUTHOR: HongLi
//*
//*  CREATED: Mar 06, 2005
//*
//*  LAST MODIFIED:
//*             BY:
//*             TO:
//*  SUMMARY:
//*
//*
//*  NOTES:
//*
//*
//*
//*  TO DO:
//*
//*
//****************************************************************************|
//        1         2         3         4         5         6         7         8
//2345678901234567890123456789012345678901234567890123456789012345678901234567890

#include "ProblemDefinition.h"
#include "SolverOptions.h"
#include "CollectStats.h"
#include "Vector.h"
#include "Matrix.h"
#include "Random.h"
#include "Solver.h"
#include <iostream>
#include <stdlib.h>

using namespace CSE::Math;
using namespace CSE::StochRxn;

Vector Initialize();
Matrix Stoichiometry();
Matrix DependentGrapth();
Vector Propensity(const Vector& x);
Vector PartialProp(int RIndex, const Vector& x, const Matrix& dg, double& a0);

int main(int argc, const char* argv[])
{
  try {
    
    // Parse the command line:
    const char *outFile;

    if (argc != 2) {
	    std::cerr << "Usage:  singleschlogl <output file>";
	    exit(EXIT_FAILURE);
    }
    else {
         outFile = argv[1];
    }

      time_t curTime = time(0);
      CSE::Math::Seeder(static_cast<unsigned int>(curTime), curTime);

    // Set up the problem:
    Vector X0 = Initialize();
    Matrix nu = Stoichiometry();
    Matrix dg = DependentGrapth();
    ReactionSet rxns(nu, dg, Propensity, PartialProp);

    // Configure solver
    SolverOptions opt = ConfigStochRxn(0);

    opt.stepsize_selector_func =  OSSADirect_Stepsize;
    opt.single_step_func = OSSA_SingleStep;
    opt.progress_interval = 1;
    opt.initial_stepsize = 0.2;

    double TimeFinal = 4.0;                                                              
    // Make all the runs:
	SolutionHistory sln = StochRxn(X0, 0, TimeFinal, rxns, opt);
	WriteHistoryFile(sln, outFile);
	std::cerr << "Endpoints written to file <  "<< outFile << ">.\n";
  }
  catch (const std::exception& ex){ 
    std::cerr << "\nCaught " << ex.what() << '\n';
  }

  return 0;
}
