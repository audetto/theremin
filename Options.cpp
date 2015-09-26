#include "Options.h"
#include <iostream>

#include <boost/program_options.hpp>

namespace ASI
{
  namespace po = boost::program_options;

  bool fillOptions(int argc, char** argv, Options & options)
  {
    po::options_description desc("theremin");
    desc.add_options()
      ("help,h", "Print this help message");

    po::options_description synthDesc("Audio");
    synthDesc.add_options()
      ("octaves", po::value<size_t>()->default_value(2), "Number of octaves")
      ("decay", po::value<double>()->default_value(50.0), "Decay speed ASDR");
    desc.add(synthDesc);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help"))
    {
      std::cout << "theremin" << std::endl << std::endl << desc << std::endl;
      return false;
    }

    options.octaves = vm["octaves"].as<size_t>();
    options.decay = vm["decay"].as<double>();

    return true;
  }

}
