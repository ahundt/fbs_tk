#include <flatbuffers/flatbuffers.h>
#include <flatbuffers/idl.h>
#include <flatbuffers/util.h>
#include <fbs-to-jsonl/fbs-to-jsonl.hpp>

int main(int argc, const char **argv) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <SCHEMA>" << std::endl;
		std::cerr << "\tConverts JSONL into a stream of binary Flatbuffers." << std::endl;
		std::cerr << "\tSCHEMA\tThe flatbuffers schema used to parse the data." << std::endl;
		return -1;
	}
	flatbuffers::Parser parser;
	std::string schemafile;
  	if (!flatbuffers::LoadFile(argv[1], false, &schemafile)) {
  		std::cerr << "Could not open schema: " << argv[1] << std::endl;
  		return -2;
  	}
  	if (!fbs2jsonl::jsonl_to_fbs_stream(schemafile.c_str(), std::cin, std::cout)) {
		std::cerr << "Error parsing data!" << std::endl;
		return -3;
  	}
	return 0;
}

