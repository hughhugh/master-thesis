function vargout = lacOperonSSAFunction(x_values, parameters)
% Different behavior depending on calling method:
%      - if called with 1 parameter, gives the Stoichiometry matrix ('stoich'), the
%        intial concentrations ('init') or the model parameters ('params').
%      - with parameters, output the current rate propensities, according to actual variables x_values,
%        and the model parameters.
% To be used with SSA.m

if (nargin == 1), 
	% If called with 1 parameter, switch on the string provided.
	if (strcmp(x_values,'stoich'))
		% Return the Stoichiometry Matrix.
		vargout = [0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0;1 0 0 0 0 0 0 0 0 0 0 -1 0 0 0 0;0 1 -1 1 -1 1 0 0 0 0 0 0 -1 0 0 0;0 0 0 0 -1 1 -1 1 1 0 0 0 0 0 0 0;0 0 0 0 0 0 -1 1 1 0 0 0 0 0 0 0;0 0 0 0 0 0 0 0 1 0 0 0 0 0 -1 0;0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 -1;0 0 -1 1 0 0 0 0 0 0 -1 0 0 1 0 0;0 0 1 -1 0 0 0 0 0 0 0 0 0 -1 0 0;0 0 0 0 1 -1 0 0 0 0 0 0 0 0 0 0;0 0 0 0 0 0 1 -1 -1 0 0 0 0 0 0 0];
	elseif(strcmp(x_values,'init'))
		vargout = [1.000000 0.000000 50.000000 1.000000 100.000000 0.000000 0.000000 20.000000 0.000000 0.000000 0.000000 ];
	elseif(strcmp(x_values,'params'))
		vargout = [0.020000 0.100000 0.005000 0.100000 1.000000 0.010000 0.100000 0.010000 0.030000 0.100000 0.000010 0.010000 0.002000 0.002000 0.010000 0.001000 ];
	end;

else
	%--------------------------------------------------------
	% propensity functions

	vargout = [ 
			 parameters(1)*x_values(1); ... 
			 parameters(2)*x_values(2); ... 
			 parameters(3)*x_values(3)*x_values(8); ... 
			 parameters(4)*x_values(9); ... 
			 parameters(5)*x_values(3)*x_values(4); ... 
			 parameters(6)*x_values(10); ... 
			 parameters(7)*x_values(4)*x_values(5); ... 
			 parameters(8)*x_values(11); ... 
			 parameters(9)*x_values(11); ... 
			 parameters(10)*x_values(6); ... 
			 parameters(11)*x_values(8)*x_values(7); ... 
			 parameters(12)*x_values(2); ... 
			 parameters(14)*x_values(3); ... 
			 parameters(14)*x_values(9); ... 
			 parameters(15)*x_values(6); ... 
			 parameters(16)*x_values(7); ... 
	];
end;