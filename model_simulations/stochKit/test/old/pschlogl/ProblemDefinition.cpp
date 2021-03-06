#include "ProblemDefinition.h"

Vector Initialize()
{
 	Vector x0(1, 0.0);

	x0(0) = 250;
	return x0;
} 

Matrix Stoichiometry()
{
   Matrix nu(1, 4, 0.0);

   nu(0,0) = 1; nu(0,1) = -1; 
   nu(0,2) = 1; nu(0,3) = -1;
   
   return nu;
}

Matrix DependentGrapth()
{
   Matrix dg(4,4,0.0);
   return dg;
}

Vector Propensity(const Vector& x)
{
   double c1 = 0.03, c2 = 1e-4, c3 = 200, c4 = 3.5;
   Vector a(4);
   a(0) = c1/2. * x(0)*(x(0) -1);
   a(1) = (c2/6.0)*x(0)*(x(0)-1)*(x(0)-2);
   a(2) = c3 ;
   a(3) = c4 * x(0);

   return a;
}

Vector PartialProp(int RIndex, const Vector& x, const Matrix& dg, double& a0)
{   
   double c1 = 0.03, c2 = 1e-4, c3 = 200, c4 = 3.5;
   Vector a(4);
   a(0) = c1/2. * x(0)*(x(0) -1);
   a(1) = (c2/6.0)*x(0)*(x(0)-1)*(x(0)-2);
   a(2) = c3 ;
   a(3) = c4 * x(0);

   return a;

}

Matrix PropensityJacobian(const Vector& x)
{
   double c1 = 0.03, c2 = 1e-4, c3 = 200, c4 = 3.5;
   
   Matrix j(4, 1, 0.0);
   j(0,0) = c1/2*(2*x(0) - 1);
   j(1,0) = c2/6 * ( 3*x(0)*x(0) - 6*x(0) + 2);
   j(3,0) = c4;
   
   return j;
}
void Equilibrium(Vector& x, Vector& a, int rxn)
{
}
