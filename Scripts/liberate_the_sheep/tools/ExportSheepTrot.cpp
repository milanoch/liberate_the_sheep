#include "Game/SheepTrotMotion.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>

int main(int argc, char **argv) {
    namespace Motion = LiberateTheSheep::Game::SheepTrotMotion;
    namespace Animation = LiberateTheSheep::Game::SheepRunAnimation;
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [output.csv]\n";
        return 1;
    }
    std::ofstream file;
    if (argc == 2) {
        file.open(argv[1]);
        if (!file) {
            std::cerr << "Cannot write trot tracks: " << argv[1] << '\n';
            return 1;
        }
    }
    std::ostream &output = argc == 2 ? static_cast<std::ostream &>(file) : std::cout;
    output << "frame,leg,phase,stance,x,y,lift,lane_x,neutral_y\n" << std::setprecision(17);
    for (int frame = 0; frame < Animation::kRunPoseCount; ++frame) {
        for (const auto leg : Motion::kLegs) {
            const auto pose = Motion::sampleFrame(frame, leg);
            const auto index = static_cast<std::size_t>(leg);
            output << frame << ',' << Motion::kLegNames[index] << ','
                   << pose.phase << ',' << (pose.stance ? 1 : 0) << ',' << pose.x << ','
                   << pose.y << ',' << pose.lift << ',' << Motion::kLanePixels[index] << ','
                   << Motion::kNeutralYPixels[index] << '\n';
        }
    }
    output.flush();
    return output ? 0 : 1;
}
