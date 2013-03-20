
/*
 * File:         robot_locking_bacteria.cpp
 * Date:         June 2008
 * Description:  Controller for the assembly of a puzzle, according to a given plane.
 * Author:       Loic Matthey
 * Modifications:
 *
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <device/robot.h>
#include <device/differential_wheels.h>
#include <device/distance_sensor.h>
#include <device/connector.h>
#include <device/emitter.h>
#include <device/receiver.h>
#include <device/gps.h>
#include <device/servo.h>
#include <vector>
#include <map>

#define KEYB_OFF 2

#define TIME_STEP 32
#define MAX_SENSOR_NUMBER 16
#define MAX_CONNECTOR_NUMBER 1


// Communication channels and ranges
#define COMMUNICATION_CHANNEL 1
#define COMMUNICATION_RANGE 0.6
#define SUPERVISOR_CHANNEL 2
#define SUPERVISOR_RANGE -1

#define EPS_X 0.04
#define EPS_Z 0.03
#define ABORT 250
#define WAIT_TIME 30

#define UNLOCKED 0
#define LOCKED 1

#define SPEED_INCR 100.0
#define INITIAL_SPEED 25000

#define DELAY_TUMBLE 22
#define DELAY_FORWARD 10

#define ROBOT 0
#define PIECE 1

using namespace std;
	
static DeviceTag commEmitter, commReceiver, gps;
//static robot_types robot_type;
static int found_robot;

// static DeviceTag arm_servo;
static DeviceTag sensors[MAX_SENSOR_NUMBER];
// static DeviceTag connectors[MAX_CONNECTOR_NUMBER];
static float matrix[MAX_SENSOR_NUMBER][2];
static int sensor_number;
static int range;
float multiple_avoid = 1.0;

int robot_num;
const char *robot_name;
const float *direction = NULL;
float turnAngle = 0;
float arm_position;
int robot1, robot2;
int piece_type;  // type of piece the robot is carrying
int piece_type_new;
int piece_num;   // number of the piece of a type that the robot is carrying
int otherPiece;
int otherRobotID;
int finalConfig = 0;
int ctr = 0;
int abortCtr = 0;  // if this goes too high then abort the state
int waitCtr = 0;   // use to wait for communication

int configuration_complete = 0;
int configuration_other_assembling = 0;
int reaction_occuring = 0;

bool separate_with_piece = false;
int new_carrying_robot = 0;
int new_configuration = 0;

int connector_status[MAX_CONNECTOR_NUMBER];
bool unlock = false;
bool braitenberg = true;

float speed_bias[2];
float speed[2];

FILE* out_file;
float elapsed_time = 0.0;
float time_encoutering = 0.0;

bool debug = false;

// Assembly Plan
vector<vector<float> > pieces_reactions_angles; // Piece i -> angle of reaction j
vector<vector<bool> > pieces_reactions_locking; // Piece i -> has to lock during reaction j
map<pair<int, int>, int> configurations_to_reaction; // Indirection mapping from configurations to reaction
map<pair<int, int>, bool> configurations_first_kept_carried; // After two configurations assembly, is the first configuration carried ? (if not, then the other one is)
map<pair<int, int>, int> reactions_configurations_change; // Reaction i and configuration j => new configuration k
vector<vector<int> > effect_reactions; // String representing the effects of the reactions on the populations

//Chemotaxis stuff
int tumbleDelay = DELAY_TUMBLE;
int forward_delay = DELAY_FORWARD;

// robot states
typedef enum { SEARCH_FOR_PIECE, ALIGN_WITH_PIECE, APPROACH_PIECE, ROTATE_PIECE, SEARCH_FOR_ROBOT, ROTATE_PIECE_ASSEMBLY,
    ALIGN_WITH_ROBOT, WAIT, APPROACH_ROBOT, UPDATE_PIECES_KNOWLEDGE, SEPARATE_ROBOTS } RobotState;
// piece states
typedef enum { FREE, CLAIMED, ROTATING, CARRIED, ROTATING_ASSEMBLY, ASSEMBLING, ASSEMBLING_LOCKED, KNOWLEDGE_UPDATED, NULL_STATE} PieceState;
RobotState state = SEARCH_FOR_PIECE;
// SEARCH_FOR_ROBOT: Robot looks for an appropriate piece
// ALIGN_WITH_ROBOT: Robot aligns with another robot with complementary piece
// WAIT: Robot waits for other robot to align

void velAlign(float *speed) {
    // Sets velocity of robot that is aligning itself with another robot
    // for the purpose of putting pieces together

    float wheelRad = 0.0212;  // wheel radius
    float axleL = 0.0893;  // axle length
    float linL = 0.005; //0.02;     // feedback linearization length
    float theta;         // orientation of robot (radians)
    float Kgain = 100;          // proportional controller gain
    const float *gps_matrix;
    float euler_angles[3];
    float vel, omega;   // translational, rotational velocities of robot
    //float theta12;  // angle made by direction to emitter in robot frame
    float s12, r12, x12, y12;  // signal strength, distance between robots 1 and 2
    float x12G, y12G, x12Gdes, y12Gdes;
    // x12G, y12G are wrt inertial coordinates
    float r12des = 0.20;  // desired distance between robots 1 and 2

    /* Get GPS data */
    gps_matrix = gps_get_matrix(gps);
    gps_euler(gps_matrix, euler_angles);
    theta = euler_angles[1];
    ////robot_console_printf("Orientation % .3f\n", euler_angles[1]);
    //gps_position_x(gps_matrix), gps_position_y(gps_matrix),
    s12 = receiver_get_signal_strength(commReceiver);
    r12 = 1/sqrt(s12);
    //robot_console_printf("Radius: %f\n", r12);
    x12 = direction[0]*r12;
    y12 = direction[2]*r12;
    x12G = x12*cos(theta) + y12*sin(theta);
    y12G = -x12*sin(theta) + y12*cos(theta);
    x12Gdes = -r12des*sin(theta);
    y12Gdes = -r12des*cos(theta);

    vel = Kgain*(x12G - x12Gdes)*cos(theta) + Kgain*(y12G - y12Gdes)*sin(theta);
    omega = Kgain*(x12G - x12Gdes)*(-sin(theta)/linL) + Kgain*(y12G - y12Gdes)*cos(theta)/linL;

    speed[0] = (2*vel + axleL*omega)/(-2*wheelRad);
    speed[1] = axleL*omega/wheelRad + speed[0];

}

/**
 * The Braitenberg algorithm is really simple, it simply computes the
 *  speed of each wheel by summing the value of each sensor multiplied by
 *  its corresponding weight. That is why each sensor must have a weight
 *  for each wheel.
 **/
void braitenberg_avoid(unsigned short *sensVal)
{
    int i, j;

    for (i = 0; i < 2; i++) {
	// speed[i] = speed_bias[i];

		if (braitenberg) {
	    	for (j = 0; j < sensor_number; j++) {
			// We need to recenter the value of the sensor to be able to get
			// 	negative values too. This will allow the wheels to go
			// 	backward too.

				speed[i] += 3.5 * multiple_avoid *matrix[j][i] * (1 - ((float) sensVal[j] / range));
			}
		}
    }
}

/**
 *	Perform Bacterial like movement pattern:
 *		- move forward
 *		- tumble (turn randomly) randomly
 *	A real bacteria tumbling frequency depend on a temporal gradient.
 *	We don't need that here.
 *
 **/
void movement_chemotaxis()
{

	if (tumbleDelay <= 0) {
		// Tumble now

		// Should turn randomly (no backward movement)
		do {
			for(size_t i = 0; i < 2; ++i) {
				speed[i] = speed_bias[i]*1.0*(2.0*(rand()/(RAND_MAX+1.0))-1.0);
			}
		}while(speed[0] < 0.0 && speed[1] < 0.0);

		// We keep turning for a couple of timesteps
		forward_delay = DELAY_FORWARD; // + DELAY_FORWARD*(rand()/(RAND_MAX+1.0) - 0.5);

		// Restart the counter to tumble
		tumbleDelay = DELAY_TUMBLE + DELAY_TUMBLE*(rand()/(RAND_MAX+1.0) - 0.5);
	}
	if (forward_delay <= 0) {
		// Going forward (turn finished)
		for(size_t i = 0; i < 2; ++i) {
			speed[i] = speed_bias[i];
		}
		tumbleDelay--;
	}

	forward_delay--;
}

// Send a message on the channel of the supervisor and then go back to Communication channel
void sendMessageToSupervisor(char* message) {
	emitter_set_channel(commEmitter, SUPERVISOR_CHANNEL);
	emitter_set_range(commEmitter, SUPERVISOR_RANGE);
	
	emitter_send_packet(commEmitter, message, strlen(message) + 1);
	
	emitter_set_channel(commEmitter, COMMUNICATION_CHANNEL);
	emitter_set_range(commEmitter, COMMUNICATION_RANGE);
}

void sendStatisticReaction(int reactionOccured, float reaction_time) {
	char message[50];
	
	// Write the message in a Matlab loadable format
	sprintf(message, "%.5f %d ", elapsed_time - reaction_time, reactionOccured);
	for (vector<int>::iterator it = effect_reactions[reactionOccured-1].begin(); it!=effect_reactions[reactionOccured-1].end(); ++it) {
		sprintf(message, "%s%d ", message, *it);
	}
	
	robot_console_printf("%s\n", message);
	
	// Send that to the supervisor to regroup everything.
	sendMessageToSupervisor(message);
}

// 
// /* Output the signals for matlab */
// void file_output() {
// 	if (out_file == NULL) {
// 		char filename[50];
// 		int nb_file = 0;
// 
// 	// We'll open a file that doesn't exist for now.
// 		sprintf(filename, "output_values_%s_%d.txt", robot_name, nb_file);
// 
// 		out_file = fopen(filename, "r");
// 		while (out_file) {
// 		// The file already exists, get another file name
// 			fclose(out_file);
// 			nb_file++;
// 			sprintf(filename, "output_values_%s_%d.txt", robot_name, nb_file);
// 			out_file = fopen(filename, "r");
// 		}
// 		out_file = fopen(filename, "w");
// 	}
// 
// 	fprintf(out_file, "%f ", elapsed_time);
// 
// 	// New puzzle completed
// 	fprintf(out_file, "%f ", 1.0);
// 
// 	fprintf(out_file, "\n");
// 
// 	fflush(out_file);
// }


/* Handle the keyboard actions */
void keyboard() {
    int key;
    key = robot_keyboard_get_key();

    // if (key != 0) {
    // 	//robot_console_printf("key: %d\n", key+KEYB_OFF);
    // }

    if (key == (317-KEYB_OFF)) { // arrow up
		speed_bias[0] += SPEED_INCR;
		speed_bias[1] += SPEED_INCR;
		// //robot_console_printf("Speed : %f %f\n", speed[0], speed[1]);
    } else if(key == (319-KEYB_OFF)) { // arrow down
		speed_bias[0] -= SPEED_INCR;
		speed_bias[1] -= SPEED_INCR;
		// speed[0] = speed[1] = 0.0;
		// //robot_console_printf("Speed : %f %f\n", speed[0], speed[1]);
    } else if(key == (316-KEYB_OFF)) { // arrow left
		speed_bias[0] -= SPEED_INCR/2;
		speed_bias[1] += SPEED_INCR/2;
		// //robot_console_printf("Speed : %f %f\n", speed[0], speed[1]);
    } else if(key == (318-KEYB_OFF)) { // arrow right
		speed_bias[0] += SPEED_INCR/2;
		speed_bias[1] -= SPEED_INCR/2;
		// //robot_console_printf("Speed : %f %f\n", speed[0], speed[1]);
    } else if(key == (34-KEYB_OFF)) { // Space
		speed_bias[0] = speed_bias[1] = 0.0;
		// //robot_console_printf("Speed : %f %f\n", speed[0], speed[1]);
    } else 	if(key == (70-KEYB_OFF)) { // D
		if (!unlock && (connector_status[0] || connector_status[1])){
	    	unlock = true;
	    	//robot_console_printf("Unlocking\n");
		}
    } else if(key == (80-KEYB_OFF)) { // N
		braitenberg = false;
		//robot_console_printf("No braitenberg\n");
    } else	if(key == (68-KEYB_OFF)) { // B
		braitenberg = true;
		//robot_console_printf("Braitenberg\n");
    } else if(key == (85-KEYB_OFF)) { // S
		debug = !debug;
	}
}

char* parseMessage(char* recMessage, int msgValues[10]) {
	// Parse the message, format:
	// robot: 0 | robot_num | state | piece_type | piece_num | configuration_complete | reaction_occuring | new_configuration | new_carrying_robot_num 
	// piece: 1 | piece_type | piece_num | state | carrying_robot_num | angle
	int i=0;
	
	char* parsedMsg = strtok(recMessage," ");
	
	msgValues[i] = atoi(parsedMsg);
	parsedMsg = strtok(NULL, " ");
	i++;
	
	if (msgValues[0] == PIECE) {
		// Watch out, last is a float...
		while(i< 5 && parsedMsg != NULL) {
			msgValues[i] = atoi(parsedMsg);
			parsedMsg = strtok(NULL, " ");
			i++;
		}
	} else {
		// Just put everything into a int array, assume the format is known
		while(i< 10 && parsedMsg != NULL) {
			msgValues[i] = atoi(parsedMsg);
			parsedMsg = strtok(NULL, " ");
			i++;
		}
	}	
	return parsedMsg;
}

static int run(int ms)
{
    int i;
    unsigned short sensors_value[MAX_SENSOR_NUMBER];
    // int connector_presence[MAX_CONNECTOR_NUMBER];
    // float dxEmit, dzEmit;
    // char sendMessage[30];  // message that robot sends
	// char savedMessage[30];
	// int msgValues[10];
	// char *recMessage;

    // Update the keyboard
    // keyboard();

    // Update the distance sensors
    for (i = 0; i < sensor_number; i++) {
		sensors_value[i] = distance_sensor_get_value(sensors[i]);
    }

    // Update the connector presence sensors
    // for (i = 0; i < MAX_CONNECTOR_NUMBER; i++) {
    // 		connector_presence[i] = connector_get_presence(connectors[i]);
    //    }
    //    ////robot_console_printf("Connectors : %d %d\n", connector_presence[0], connector_presence[1]);
    // 
    //    // Lock automatically when approaching a piece
    //    if (state == APPROACH_PIECE && !unlock && connector_presence[0] &&
    // 	    !connector_status[0]) {
    // 		//Should lock
    // 		// robot_console_printf("Lock !\n");
    // 		connector_lock(connectors[0]);
    // 		connector_status[0] = LOCKED;
    //    }
    // 
    //    // Unlock requested
    //    if (unlock) {
    // 		connector_unlock(connectors[0]);
    // 		connector_status[0] = UNLOCKED;
    // 		unlock = false;
    //    }

		//     // Allow locking when out of presence
		//     if (unlock && !connector_presence[0] && state == SEARCH_FOR_ROBOT) {
		// unlock= !unlock;
		//     }

    // Send heartbeat message
	// if (state != SEARCH_FOR_PIECE && state != UPDATE_PIECES_KNOWLEDGE) {
	//     sprintf(sendMessage, "0 %d %d %d %d %d %d %d %d", robot_num, state, piece_type, piece_num, configuration_complete, reaction_occuring, 0, 0);
	//     emitter_send_packet(commEmitter, sendMessage, strlen(sendMessage) + 1);
	// }

    // Bacterial style movement
    movement_chemotaxis();

    // When looking for a piece, try to avoid other robots more
	multiple_avoid = 2.5;

	// Braitenberg obstacle avoidance
    braitenberg_avoid(sensors_value);
	
	// case SEARCH_FOR_PIECE:
		
	// Set some knowledge
	piece_type = 0;
	piece_num = 0;
	configuration_complete = 0;
	configuration_other_assembling = 0;

    /* Set the motor speeds */

    //robot_console_printf("Speeds: %d %d\n", (int) speed[0], (int) speed[1]);
    differential_wheels_set_speed((int) speed[0], (int) speed[1]);

    // Output the current metrics into the file
    //file_output();

    elapsed_time += (float)TIME_STEP / 1000.0;
    return TIME_STEP;           /* run for TIME_STEP ms */
}

//================================================================================================================================================

void populateAssemblyPlan() {
	const int nb_reactions = 8;
	const int nb_species = 12;
	
	// float angles[][nb_reactions] = {{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 3.14}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}};
	// bool locks[][nb_reactions] = {{true, false, false, false}, {true, false, true, true}, {false, true, true, false}, {false, true, false, false}};
																
	/*
		TODO Should depend on some external file...
	*/
	// for(int i = 0; i < nb_pieces; ++i) {
		// pieces_reactions_angles.push_back(vector<float> (angles[i], angles[i] + sizeof(angles[i]) / sizeof(float)));
		// pieces_reactions_locking.push_back(vector<bool> (locks[i], locks[i] + sizeof(locks[i]) / sizeof(bool)));
	// }
	
	// // First reaction set (Spring)
	// configurations_to_reaction[pair<int, int> (1,2)] = 1;
	// configurations_to_reaction[pair<int, int> (2,1)] = 1;
	// configurations_to_reaction[pair<int, int> (3,4)] = 2;
	// configurations_to_reaction[pair<int, int> (4,3)] = 2;
	// configurations_to_reaction[pair<int, int> (6,7)] = 3;
	// configurations_to_reaction[pair<int, int> (7,6)] = 3;
	// configurations_to_reaction[pair<int, int> (2,8)] = 4;
	// configurations_to_reaction[pair<int, int> (8,2)] = 4;
	// 
	// configurations_first_kept_carried[pair<int, int> (1,2)] = true; // First piece carried on
	// configurations_first_kept_carried[pair<int, int> (2,1)] = false; // First piece carried on
	// configurations_first_kept_carried[pair<int, int> (3,4)] = false; // Second piece carried on
	// configurations_first_kept_carried[pair<int, int> (4,3)] = true; // Second piece carried on
	// configurations_first_kept_carried[pair<int, int> (6,7)] = false; // Second piece carried on
	// configurations_first_kept_carried[pair<int, int> (7,6)] = true; // Second piece carried on
	// configurations_first_kept_carried[pair<int, int> (8,2)] = false; // No piece carried on (stay on floor)
	// configurations_first_kept_carried[pair<int, int> (2,8)] = false; // No piece carried on (stay on floor)
	// 
	// // Rules of modifications of configurations
	// reactions_configurations_change[pair<int, int> (1, 1)] = 6;
	// reactions_configurations_change[pair<int, int> (1, 2)] = 6;
	// 
	// reactions_configurations_change[pair<int, int> (2, 3)] = 7;
	// reactions_configurations_change[pair<int, int> (2, 4)] = 7;
	// 
	// reactions_configurations_change[pair<int, int> (3, 6)] = 8;
	// reactions_configurations_change[pair<int, int> (3, 7)] = 8;
	// 
	// reactions_configurations_change[pair<int, int> (4, 8)] = 9;
	// reactions_configurations_change[pair<int, int> (4, 2)] = 9;
	
	
	// Modified last connection reaction set (Loic)
	
	// Effects of reactions.
	//  - Taken of pieces (4 reactions)
	//	- Assembly (4 reactions)
	// format: {free piece x4} {carried piece x4} {product x4}
	int eff_reac[][nb_species] = {{-1, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0 },
								  {0, -1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0 },
								  {0, 0, -1, 0,  0, 0, 1, 0,  0, 0, 0, 0 },
								  {0, 0, 0, -1,  0, 0, 0, 1,  0, 0, 0, 0 },
								
								  {0, 0, 0, 0,  -1, -1, 0, 0,  1, 0, 0, 0 },
								  {0, 0, 0, 0,  0, 0, -1, -1,  0, 1, 0, 0 },
								  {0, 0, 0, 0,  0, 0, 0, 0,  -1, -1, 1, 0 },
								  {0, 0, 0, 0,  0, -1, 0, 0,  0, 0, -1, 1 }
	};
	
	configurations_to_reaction[pair<int, int> (1,2)] = 1;
	configurations_to_reaction[pair<int, int> (2,1)] = 1;
	configurations_to_reaction[pair<int, int> (3,4)] = 2;
	configurations_to_reaction[pair<int, int> (4,3)] = 2;
	configurations_to_reaction[pair<int, int> (5,6)] = 3;
	configurations_to_reaction[pair<int, int> (6,5)] = 3;
	configurations_to_reaction[pair<int, int> (2,7)] = 4;
	configurations_to_reaction[pair<int, int> (7,2)] = 4;
	
	configurations_first_kept_carried[pair<int, int> (1,2)] = true; // First piece carried on
	configurations_first_kept_carried[pair<int, int> (2,1)] = false; // First piece carried on
	configurations_first_kept_carried[pair<int, int> (3,4)] = true; // Second piece carried on
	configurations_first_kept_carried[pair<int, int> (4,3)] = false; // Second piece carried on
	configurations_first_kept_carried[pair<int, int> (5,6)] = false; // Second piece carried on
	configurations_first_kept_carried[pair<int, int> (6,5)] = true; // Second piece carried on
	configurations_first_kept_carried[pair<int, int> (7,2)] = false; // No piece carried on (stay on floor)
	configurations_first_kept_carried[pair<int, int> (2,7)] = false; // No piece carried on (stay on floor)
	
	// Rules of modifications of configurations
	reactions_configurations_change[pair<int, int> (1, 1)] = 5;
	reactions_configurations_change[pair<int, int> (1, 2)] = 5;
	
	reactions_configurations_change[pair<int, int> (2, 3)] = 6;
	reactions_configurations_change[pair<int, int> (2, 4)] = 6;
	
	reactions_configurations_change[pair<int, int> (3, 5)] = 7;
	reactions_configurations_change[pair<int, int> (3, 6)] = 7;
	
	reactions_configurations_change[pair<int, int> (4, 7)] = 8;
	reactions_configurations_change[pair<int, int> (4, 2)] = 8;
	
	for(int i = 0; i < nb_reactions; ++i) {
		effect_reactions.push_back(vector<int> (eff_reac[i], eff_reac[i] + sizeof(eff_reac[i]) / sizeof(int)));
	}
	
}

static void reset(void) {
	int i,j;
	// int channel;
	robot_name = robot_get_name();
		
	robot_num = atoi(&robot_name[1]);

	// char connector_name[15];

	float khepera3_matrix[9][2] =
		{ {0, 0}, {-20000, 20000}, {-50000, 50000}, {-70001, 70000}, {70001, -70000},
	    	{50000, -50000}, {20000, -20000}, {0, 0}, {0, 0} };
	// { {-5000, -5000}, {-20000, 40000}, {-30000, 50000}, {-70000, 70000}, {70000, -60000},
	// {50000, -40000}, {40000, -20000}, {-5000, -5000}, {-10000, -10000} };
	char sensors_name[12];
	// float (*temp_matrix)[2];

	sensor_number = 9;
	sprintf(sensors_name, "ds0");
	range = 2000;

	// Enable the Distance sensors and Braitenberg weights
	for (i = 0; i < sensor_number; i++) {
	    sensors[i] = robot_get_device(sensors_name);
	    distance_sensor_enable(sensors[i], TIME_STEP);

	    if ((i + 1) >= 10) {
			sensors_name[2] = '1';
			sensors_name[3]++;

			if ((i + 1) == 10) {
		    	sensors_name[3] = '0';
		    	sensors_name[4] = (char) '\0';
			}
	    } else {
			sensors_name[2]++;
	    }

	    for (j = 0; j < 2; j++) {
			matrix[i][j] = khepera3_matrix[i][j];
	    }
	}

	// Enable the Connectors
	// for (i = 0; i< MAX_CONNECTOR_NUMBER; i++) {
	// 	    // Construct the names of connectors
	// 	    sprintf(connector_name, "con%d", i+1);
	// 	    //robot_console_printf("connector %d: %s \n", i, connector_name);
	// 
	// 	    // Activate the connectors
	// 	    connectors[i] = robot_get_device(connector_name);
	// 	    connector_enable_presence(connectors[i], TIME_STEP);
	// 
	// 	    // Internal state
	// 	    connector_status[i] = UNLOCKED;
	// 	}
	//
	// connectors[0] = robot_get_device("con1");
	// connectors[1] = robot_get_device("con2");
	// connector_enable_presence(connectors[0], TIME_STEP);
	// connector_enable_presence(connectors[1], TIME_STEP);
	//
	// for (i = 0; i< MAX_CONNECTOR_NUMBER; i++) {
	// 	connector_status[i] = UNLOCKED;
	// }

	// Basic speed at 0
	speed_bias[0] = INITIAL_SPEED;
	speed_bias[1] = INITIAL_SPEED;

	speed[0] = 0.0;
	speed[1] = 0.0;

	// Enable keyboard
	// robot_keyboard_enable(TIME_STEP);

	// Enable the Arm servo
	// arm_servo = robot_get_device("arm_servo");
	// 	servo_enable_position(arm_servo, TIME_STEP);
	
	//robot_console_printf("The %s robot is reset, it uses %d sensors\n",robot_name, sensor_number);

	// Enable the emitter/receiver, gps ------------%
	// commEmitter = robot_get_device("rs232_out");
	// 	commReceiver = robot_get_device("rs232_in");
	// 	gps = robot_get_device("gps");

	/* If the channel is not the good one, we change it. */
	// channel = emitter_get_channel(commEmitter);
	// 	if (channel != COMMUNICATION_CHANNEL) {
	// 	    emitter_set_channel(commEmitter, COMMUNICATION_CHANNEL);
	// 	}
	// 	
	// 	// Communication range
	// 	float comm_range = emitter_get_range(commEmitter);
	// 	if (comm_range != COMMUNICATION_RANGE) {
	// 		emitter_set_range(commEmitter, COMMUNICATION_RANGE);
	// 	}

	// receiver_enable(commReceiver, TIME_STEP);
	// gps_enable(gps, TIME_STEP);

	found_robot = 0;

	srand(robot_num*time(NULL));
	// robot_console_printf("RR: %f\n", (rand()/(RAND_MAX+1.0)));

	// populateAssemblyPlan();
	
	return;
}

int main() {
	robot_live(reset);
	robot_run(run);             /* this function never returns */

	return 0;
}
