function dy = puzzlecase_ode(t, y, params)
%PUZZLECASE_ODE Definition of the derivatives of the Puzzle Test Case
%   system
%

p_encounter = params(1);
p_encounter_robot = params(2);
p_encounter_robot_11 = params(3);


% Take care of null/negative populations
y(y<0) = 0;

% Species
% robot piece{1..4}free piece{1..4}carried p{5..8}carried
robot = y(1);

piecefree(1) = y(2);
piecefree(2) = y(3);
piecefree(3) = y(4);
piecefree(4) = y(5);

piececarried(1) = y(6);
piececarried(2) = y(7);
piececarried(3) = y(8);
piececarried(4) = y(9);
piececarried(5) = y(10);
piececarried(6) = y(11);
piececarried(7) = y(12);


% Model
% \dot{robot} = -p_encouter*piece{1..4}free*robot +
%                    p_encounter_robot*piece5carried*piece6carried +
%                    p_encounter_robot*piece7carried*piece8carried +
%                    p_encounter_robot*piece9carried*piece10carried +
%                    2*p_encounter_robot*piece10carried*piece2carried
% \dot{piece_1_free} = -p_encounter*piece1free*robot
                
                
                
dy = [  -p_encounter*robot*sum(piecefree) + p_encounter_robot*piececarried(1)*piececarried(2) ...
                + p_encounter_robot*piececarried(3)*piececarried(4) + p_encounter_robot*piececarried(5)*piececarried(6) ...
                + 2*p_encounter_robot_11*piececarried(7)*piececarried(2);
        -p_encounter*piecefree(1)*robot;
        -p_encounter*piecefree(2)*robot;
        -p_encounter*piecefree(3)*robot;
        -p_encounter*piecefree(4)*robot;
        p_encounter*piecefree(1)*robot - p_encounter_robot*piececarried(1)*piececarried(2);
        p_encounter*piecefree(2)*robot - p_encounter_robot*piececarried(1)*piececarried(2) - p_encounter_robot_11*piececarried(7)*piececarried(2);
        p_encounter*piecefree(3)*robot - p_encounter_robot*piececarried(3)*piececarried(4);
        p_encounter*piecefree(4)*robot - p_encounter_robot*piececarried(3)*piececarried(4);
        p_encounter_robot*piececarried(1)*piececarried(2) - p_encounter_robot*piececarried(5)*piececarried(6);
        p_encounter_robot*piececarried(3)*piececarried(4) - p_encounter_robot*piececarried(5)*piececarried(6);
        p_encounter_robot*piececarried(5)*piececarried(6) - p_encounter_robot_11*piececarried(7)*piececarried(2);
        p_encounter_robot_11*piececarried(7)*piececarried(2);
    ];


