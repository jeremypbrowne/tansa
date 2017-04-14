//
// Created by kyle on 2/28/17.
//

#include "tansa/csv.h"

#include <boost/algorithm/string.hpp>
namespace tansa {

template<typename Out>
void split(const std::string &s, char delim, Out result) {
	std::stringstream ss;
	ss.str(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		*(result++) = item;
	}
}

std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, std::back_inserter(elems));
	return elems;
}

ActionTypes parse_action_type(const std::string& data){
	if(data == "transition")
		return ActionTypes::Transition;
	else if (data == "line")
		return ActionTypes::Line;
	else if (data == "circle")
		return ActionTypes::Circle;
	else if (data == "hover")
		return ActionTypes::Hover;
	else if (data == "light")
		return ActionTypes::Light;
	else if (data == "strobe")
		return ActionTypes::Strobe;
	else if (data == "ellipse")
		return ActionTypes::Ellipse;
	else if (data == "spiral")
		return ActionTypes::Spiral;
	else if (data == "helix")
		return ActionTypes::Helix;
	else if (data == "arc")
		return ActionTypes::Arc;
	else if (data == "trajectory")
		return ActionTypes::TransformedTraj;
	else{
		std::cout << "Unknown action type: " << data << std::endl;
		return ActionTypes::None;
	}
}

bool is_light_action(ActionTypes type){
	return type == ActionTypes::Strobe || type == ActionTypes::Light;
}

std::vector<string> read_csv_line(std::string& line){
	auto ret = split(line, ',');
	return std::move(ret);

}
Choreography* parse_csv(const char* filepath, double scale){
	Choreography* ret = nullptr;
	int currentLine = 1; // Lines indexed at 1
	try {
		auto csv = ifstream(filepath);
		ret = new Choreography();
		auto drone_map = std::map<std::string, unsigned long>();
		std::string line;
		getline(csv, line); //Contains dronekey and meters/feet
		std::vector<std::string> split_line;
		split_line = split(line, ',');
		unsigned break_num = 0;
		double conversion_factor = scale;
		double angle_conversion = 1.0;
		std::string length_units = split_line[csv_positions::LengthUnitPos];
		std::string angle_units = split_line[csv_positions::AngleMeasurePos];
		ret->needConvertToMeters = length_units != "meters" && length_units != "Meters";
		ret->needConvertToRadians = angle_units != "radians" && angle_units != "Radians";
		if(ret->needConvertToMeters) {
			conversion_factor *= FEET_TO_METERS;
		}
		if(ret->needConvertToRadians) {
			angle_conversion *= DEGREES_TO_RADIANS;
		}
		size_t start_index = csv_positions::DroneKeyPos;
		unsigned long num_drones = 0;
		for (size_t i = start_index; i < split_line.size(); i++) {
			if (split_line[i].empty())
				break;
			drone_map[split_line[i]] = num_drones;
			num_drones++;
		}
		ret->actions.resize(num_drones);
		ret->lightActions.resize(num_drones);
		currentLine++;

		getline(csv, line); //contains homes
		auto home_split = split(line, ',');
		for (size_t i = csv_positions::ParamStartPos; i < home_split.size(); i++) {
			ret->homes.push_back(parse_point(home_split[i])*conversion_factor);
			if (ret->homes.size() == num_drones)
				break;
		}
		currentLine++;

		while (getline(csv, line)) { //Iterate line by line
			split_line = std::move(read_csv_line(line)); //TODO: Probably inefficient.

			//Parse breakpoints
			if (!split_line[csv_positions::BreakpointPos].empty()) {
				ret->breakpoints.push_back(
						Breakpoint(split_line[csv_positions::BreakpointPos],
								   break_num++,
								   parse_time(split_line[csv_positions::StartTimePos])));
			}

			double start_time = parse_time(split_line[csv_positions::StartTimePos]);
			double end_time = parse_time(split_line[csv_positions::EndTimePos]);
			ActionTypes action_type = parse_action_type(split_line[csv_positions::ActionTypePos]);
			std::vector<unsigned long> drones = parse_drones(split_line[csv_positions::DronesPos], drone_map);
			//currently only supporting one drone per line so just parse that one.
			if (is_light_action(action_type)) {
				for (int j = 0; j < 1; j++) {
					auto light_action = parse_light_action(
							action_type,
							start_time,
							end_time,
							drones[j],
							std::vector<std::string>(split_line.begin() + csv_positions::ParamStartPos,
													 split_line.end()));
					auto light_index = light_action->GetLightIndex();
					ret->lightActions[drones[j]][light_index].push_back(light_action);
				}
			} else {
				for (int j = 0; j < 1; j++) {
					auto motion_action = parse_motion_action(
							action_type,
							start_time,
							end_time,
							drones[j],
							std::vector<std::string>(split_line.begin() + csv_positions::ParamStartPos,
													 split_line.end()),
							conversion_factor,
							angle_conversion
					);
					if(motion_action != nullptr)
						motion_action->line = currentLine;
					ret->actions[drones[j]].push_back(motion_action);
				}
			}

			currentLine++;
		}
		insert_transitions(ret->actions, ret->homes);

		return ret;

	} catch (const std::exception& e) {
		delete ret;
		std::cout << "Line " << currentLine << ": " << e.what() << std::endl;
		return nullptr;
	}
}

double parse_time(std::string& time){
	return std::atof(time.c_str());
}

std::vector<unsigned long> parse_drones(std::string drone_field, const std::map<std::string, unsigned long>& drone_map) {
	boost::erase_all(drone_field, "(");
	boost::erase_all(drone_field, ")");
	boost::erase_all(drone_field, "\"");
	boost::erase_all(drone_field, " ");
	auto split_line = split(drone_field, ';');
	std::vector<unsigned long> ret;
	for(std::string s : split_line){
		unsigned long i = drone_map.at(s);
		ret.push_back(i);
	}
	return std::move(ret);
}

Action* parse_motion_action(ActionTypes type, double start, double end, unsigned long droneid, std::vector<std::string> split_line, double length_conversion, double angle_conversion) {
	Action* ret = nullptr;
	switch (type){
		case ActionTypes::Hover:
			ret = parse_hover_action(start, end, droneid, split_line, length_conversion);
			break;
		case ActionTypes::Line:
			ret = parse_line_action(start, end, droneid, split_line, length_conversion);
			break;
		case ActionTypes::Circle:
			ret = parse_circle_action(start, end, droneid, split_line, length_conversion, angle_conversion);
			break;
		case ActionTypes::Transition:
			ret = new EmptyAction((unsigned)droneid, start, end); //Transitions replaced later.
			break;
		case ActionTypes::Arc:
			ret = parse_arc_action(start, end, droneid, split_line, length_conversion);
			break;
		case ActionTypes::Ellipse:
			ret = parse_ellipse_action(start, end, droneid, split_line, length_conversion, angle_conversion);
			break;
		case ActionTypes::Spiral:
			ret = parse_spiral_action(start, end, droneid, split_line, length_conversion, angle_conversion);
			break;
		case ActionTypes::TransformedTraj:
			ret = parse_trajectory_action(start, end, droneid, split_line, length_conversion, angle_conversion);
			break;
		case ActionTypes::Helix:
			ret = parse_helix_action(start, end, droneid, split_line, length_conversion, angle_conversion);
			break;
		case ActionTypes::None:
			ret = nullptr;
		default:
			ret = nullptr;
	}
	return ret;
}

LightAction* parse_light_action(ActionTypes type, double start, double end, unsigned long droneid, std::vector<std::string> split_line){
	LightAction* ret = nullptr;
	switch (type){
		case ActionTypes::Light:
			ret = parse_simple_light_action(start, end, droneid, split_line);
			break;
		case ActionTypes::Strobe:
			ret = parse_strobe_action(start, end, droneid, split_line);
			break;
		default:
			ret = nullptr;
			break;
	}
	return ret;
}

Action* parse_hover_action(double start, double end, unsigned long droneid, const std::vector<std::string>& split_line, double length_conversion) {
	enum indices : unsigned {
		start_x_loc 	= 1,
		start_y_loc 	= 3,
		start_z_loc 	= 5,
	};

	if(split_line.size() < start_z_loc + 1) {
		throw std::runtime_error("Not enough fields in hover action");
	}

	Point hover;
	hover.x() 			= std::atof(split_line[start_x_loc].c_str());
	hover.y() 			= std::atof(split_line[start_y_loc].c_str());
	hover.z() 			= std::atof(split_line[start_z_loc].c_str());
	hover				= hover * length_conversion;
	return new MotionAction(
					(unsigned) droneid,
					make_shared<LinearTrajectory>(hover, start, hover, end),
					ActionTypes::Hover);
}

Action* parse_line_action(double start, double end, unsigned long droneid, const std::vector<std::string>& split_line, double length_conversion) {
	enum indices : unsigned {
		start_x_loc 	= 1,
		start_y_loc 	= 3,
		start_z_loc 	= 5,
		end_x_loc 		= 7,
		end_y_loc 		= 9,
		end_z_loc 		= 11
	};

	if(split_line.size() < end_z_loc + 1) {
		throw std::runtime_error("Not enough fields in line action");
	}


	Point start_point, end_point;
	start_point.x() 	= std::atof(split_line[start_x_loc].c_str());
	start_point.y() 	= std::atof(split_line[start_y_loc].c_str());
	start_point.z() 	= std::atof(split_line[start_z_loc].c_str());
	end_point.x() 		= std::atof(split_line[end_x_loc].c_str());
	end_point.y() 		= std::atof(split_line[end_y_loc].c_str());
	end_point.z() 		= std::atof(split_line[end_z_loc].c_str());
	start_point 		= start_point * length_conversion;
	end_point 			= end_point * length_conversion;
	return new MotionAction(
			(unsigned) droneid,
			make_shared<LinearTrajectory>(start_point, start, end_point, end),
			ActionTypes::Line);
}

Action* parse_circle_action(double start, double end, unsigned long droneid, const std::vector<std::string>& split_line,double length_conversion,double angle_conversion) {
	enum indices : unsigned {
		radius_loc 		= 1,
		theta1_loc 		= 3,
		theta2_loc 		= 5,
		x_loc 			= 7,
		y_loc 			= 9,
		z_loc 			= 11
	};

	if(split_line.size() < z_loc + 1) {
		throw std::runtime_error("Not enough fields in circle action");
	}

	Point origin;
	double radius 		= std::atof(split_line[radius_loc].c_str()) * length_conversion;
	double theta1 		= std::atof(split_line[theta1_loc].c_str()) * angle_conversion;
	double theta2 		= std::atof(split_line[theta2_loc].c_str()) * angle_conversion;
	origin.x() 			= std::atof(split_line[x_loc].c_str());
	origin.y() 			= std::atof(split_line[y_loc].c_str());
	origin.z() 			= std::atof(split_line[z_loc].c_str());
	origin 				= origin * length_conversion;
	return new MotionAction(
			(unsigned) droneid,
			make_shared<CircleTrajectory>(origin, radius,theta1, start, theta2, end),
			ActionTypes::Circle);
}

Action* parse_ellipse_action(double start, double end, unsigned long droneid, const std::vector<std::string>& split_line, double length_conversion, double angle_conversion) {
	enum indices : unsigned {
		radius_x_loc 	= 1,
		radius_y_loc 	= 3,
		theta1_loc 	 	= 5,
		theta2_loc	 	= 7,
		x_loc 		 	= 9,
		y_loc			= 11,
		z_loc 			= 13
	};
	Point origin;
	double radius_x 	= std::atof(split_line[radius_x_loc].c_str()) * length_conversion;
	double radius_y 	= std::atof(split_line[radius_y_loc].c_str()) * length_conversion;
	double theta1 		= std::atof(split_line[theta1_loc].c_str()) * angle_conversion;
	double theta2 		= std::atof(split_line[theta2_loc].c_str()) * angle_conversion;
	origin.x() 			= std::atof(split_line[x_loc].c_str());
	origin.y() 			= std::atof(split_line[y_loc].c_str());
	origin.z() 			= std::atof(split_line[z_loc].c_str());
	origin 				= origin * length_conversion;
	return new MotionAction(
			(unsigned) droneid,
			make_shared<EllipseTrajectory>(origin, radius_x, radius_y, theta1, start, theta2, end),
			ActionTypes::Ellipse);
}

Action* parse_helix_action(double start, double end, unsigned long droneid, const std::vector<std::string>& split_line, double length_conversion, double angle_conversion) {
	enum indices : unsigned {
		radius_loc 	= 1,
		theta1_loc 	 	= 3,
		theta2_loc	 	= 5,
		x_loc 		 	= 7,
		y_loc			= 9,
		z_loc 			= 11,
		x2_loc 		 	= 13,
		y2_loc			= 15,
		z2_loc 			= 17,
	};

	if(split_line.size() < z2_loc + 1) {
		throw std::runtime_error("Not enough fields in hover action");
	}

	Point origin, origin2;
	double radius 	= std::atof(split_line[radius_loc].c_str()) * length_conversion;
	double theta1 		= std::atof(split_line[theta1_loc].c_str()) * angle_conversion;
	double theta2 		= std::atof(split_line[theta2_loc].c_str()) * angle_conversion;
	origin.x() 			= std::atof(split_line[x_loc].c_str());
	origin.y() 			= std::atof(split_line[y_loc].c_str());
	origin.z() 			= std::atof(split_line[z_loc].c_str());
	origin 				= origin * length_conversion;

	origin2.x() 			= std::atof(split_line[x2_loc].c_str());
	origin2.y() 			= std::atof(split_line[y2_loc].c_str());
	origin2.z() 			= std::atof(split_line[z2_loc].c_str());
	origin2 				= origin2 * length_conversion;
	return new MotionAction(
			(unsigned) droneid,
			HelixHelper(radius, origin, theta1, start, origin2, theta2, end),
			ActionTypes::Helix);
}

Action* parse_spiral_action(double start, double end, unsigned long droneid, const std::vector<std::string>& split_line, double length_conversion, double angle_conversion){
	enum indices : unsigned {
		radius_loc 		= 1,
		x_loc 		 	= 3,
		y_loc			= 5,
		z_loc 			= 7,
		theta1_loc 	 	= 9,
		theta2_loc	 	= 11
	};

	if(split_line.size() < theta2_loc + 1) {
		throw std::runtime_error("Not enough fields in hover action");
	}

	Point origin;
	double radius	 	= std::atof(split_line[radius_loc].c_str()) * length_conversion;
	double theta1 		= std::atof(split_line[theta1_loc].c_str()) * angle_conversion;
	double theta2 		= std::atof(split_line[theta2_loc].c_str()) * angle_conversion;
	origin.x() 			= std::atof(split_line[x_loc].c_str());
	origin.y() 			= std::atof(split_line[y_loc].c_str());
	origin.z() 			= std::atof(split_line[z_loc].c_str());
	origin 				= origin * length_conversion;
	return new MotionAction(
			(unsigned) droneid,
			make_shared<SpiralTrajectory>(origin, radius, theta1, start, theta2, end),
			ActionTypes::Spiral);
}

Action* parse_arc_action(double start, double end, unsigned long droneid, const std::vector<std::string>& split_line, double length_conversion){
	constexpr unsigned NUM_PER_SEGMENT = 8;
	std::vector<ConstrainedPoint> points;
	std::vector<double> times;
	std::vector<string> line = split_line;
	//Remove all empty cells
	line.erase(std::remove_if(line.begin(), line.end(), [](const std::string& i) { return i.empty() || i == "\r";}), line.end());
	assert((line.size() % NUM_PER_SEGMENT) == 0);

	unsigned to_process = (unsigned)line.size() / NUM_PER_SEGMENT;
	unsigned processed 	= 0;
	auto process 		= [&](const std::vector<std::string> &sub_line) {
		enum indices : unsigned {
			time_loc 	= 1,
			x_loc 		= 3,
			y_loc		= 5,
			z_loc 		= 7
		};

		Point p;
		unsigned time 	= (unsigned)std::atof(sub_line[time_loc].c_str()); //TODO: warn about non integer times
		p.x() 			= std::atof(sub_line[x_loc].c_str());
		p.y()			= std::atof(sub_line[y_loc].c_str());
		p.z()			= std::atof(sub_line[z_loc].c_str());
		p 				= p * length_conversion;
		times.push_back(time);
		if(to_process == 1 || processed == 0) { //If it is the first or last point it must be stationary
			points.push_back(ConstrainedPoint::Stationary(p));
		} else {
			points.push_back({p});
		}
	};
	while (to_process > 0) {
		process(std::vector<std::string>(line.begin() + processed, line.begin() + processed + NUM_PER_SEGMENT));
		processed += NUM_PER_SEGMENT;
		to_process--;
	}
	Trajectory::Ptr out;
	if(compute_minsnap_mellinger11(points, times, {}, &out, NULL)) {
		return new MotionAction(
				(unsigned) droneid,
				out,
				ActionTypes::Arc
		);
	} else {
		//TODO: Return null here and handle above instead of just throwing an exception.
		throw std::runtime_error("Failed to compute Arc trajectory for drone id"
								 + std::to_string(droneid)
								 + "at start time: "
								 + std::to_string(start));
	}
}

Action* parse_trajectory_action(double start, double end, unsigned long droneid, const std::vector<std::string>& split_line, double length_conversion, double angle_conversion){
	enum indices : unsigned {
		angle_x_loc 	= 1,
		angle_y_loc 	= 3,
		angle_z_loc 	= 5,
		offset_x_loc 	= 7,
		offset_y_loc 	= 9,
		offset_z_loc 	= 11,
		traj_loc		= 12,
	};

	if(split_line.size() < traj_loc + 1) {
		throw std::runtime_error("Not enough fields in transform action");
	}

	Trajectory::Ptr ptr = dynamic_cast<MotionAction*>(parse_motion_action(
			parse_action_type(split_line[traj_loc]),
			start,
			end,
			droneid,
			std::vector<std::string>(split_line.begin()+ traj_loc + 1, split_line.end()),
			length_conversion,
			angle_conversion))->GetPath();

	double angle_x 			= std::atof(split_line[angle_x_loc].c_str()) * angle_conversion;
	double angle_y 			= std::atof(split_line[angle_y_loc].c_str()) * angle_conversion;
	double angle_z 			= std::atof(split_line[angle_z_loc].c_str()) * angle_conversion;
	double trans_x 			= std::atof(split_line[offset_x_loc].c_str()) * length_conversion;
	double trans_y 			= std::atof(split_line[offset_y_loc].c_str()) * length_conversion;
	double trans_z 			= std::atof(split_line[offset_z_loc].c_str()) * length_conversion;
	Vector3d translation 	= {trans_x, trans_y, trans_z};
	AngleAxisd x_rotation 	= AngleAxisd(angle_x, Vector3d::UnitX());
	AngleAxisd y_rotation 	= AngleAxisd(angle_y, Vector3d::UnitY());
	AngleAxisd z_rotation 	= AngleAxisd(angle_z, Vector3d::UnitZ());
	Quaterniond quat 		= x_rotation * y_rotation * z_rotation;

	return new MotionAction(
			(unsigned) droneid,
			make_shared<TransformedTrajectory>( ptr, quat.matrix(), translation, start, end),
			ActionTypes::TransformedTraj
	);
}

LightAction* parse_simple_light_action(double start, double end, unsigned long droneid, const std::vector<std::string>& split_line){
	return nullptr;
}

LightAction* parse_strobe_action(double start, double end, unsigned long droneid, const std::vector<std::string>& split_line){
	return nullptr;
}

void insert_transitions(std::vector<std::vector<Action*>>& actions, const std::vector<Point>& homes ){
	// Go back and review actions such that transitions can be created with polynomial trajectories
	for (unsigned i = 0; i < actions.size(); i++) {
		// Each entry in "actions" has a vector full of actions for that drone
		for (unsigned j = 0; j < actions[i].size(); j++) {
			if (!actions[i][j]->IsCalculated()) {
				double thisStart = actions[i][j]->GetStartTime();
				double thisEnd = actions[i][j]->GetEndTime();
				if (j == 0) {
					MotionAction *next = static_cast<MotionAction *>(actions[i][j + 1]);
					auto endState = next->GetPathState(next->GetStartTime());
					delete actions[i][j];
					actions[i][j] = new MotionAction(i,
													 PolynomialTrajectory::compute(
															 {homes[i]},
															 thisStart,
															 {endState.position, endState.velocity,
															  endState.acceleration},
															 thisEnd
													 ),
													 ActionTypes::Transition
					);
				} else if (j == (actions[i].size() - 1)) {
					//TODO: Handle the case of the last action being a transition, where our ending velocity and acceleration are 0 and destination is home.
				} else {
					// Calculate previous and next motion to generate what's needed for the transition
					// - can safely assume all are motions since made separate array for lights
					MotionAction* prev = dynamic_cast<MotionAction*>(actions[i][j - 1]);
					MotionAction* next = dynamic_cast<MotionAction*>(actions[i][j + 1]);
					// Get states from the previous and next state that were found
					auto startState = prev->GetPathState(prev->GetEndTime());
					auto endState = next->GetPathState(next->GetStartTime());
					// Cleanup object and replace with a new MotionAction
					delete actions[i][j];
					actions[i][j] = new MotionAction(
							i,
							PolynomialTrajectory::compute(
								{startState.position, startState.velocity, startState.acceleration},
								thisStart,
								{endState.position, endState.velocity, endState.acceleration},
								thisEnd),
							ActionTypes::Transition);
				}
			}
		}
	}
}

Point parse_point(std::string point) {
	std::string point_clean = point;
	boost::erase_all(point_clean, "(");
	boost::erase_all(point_clean, ")");
	boost::erase_all(point_clean, "\"");
	boost::erase_all(point_clean, " ");
	Point ret;
	auto split_point 	= split(point_clean, ';');

	if(split_point.size() != 3) {
		throw std::runtime_error("Invalid Point: " + point);
	}

	ret.x() 			= std::atof(split_point[0].c_str());
	ret.y() 			= std::atof(split_point[1].c_str());
	ret.z() 			= std::atof(split_point[2].c_str());
	return ret;
}

}
