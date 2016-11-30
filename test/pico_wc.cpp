/*
 * main.cpp
 *
 *  Created on: Aug 2, 2016
 *      Author: misale
 */

/**
 * This code implements a word-count (i.e., the Big Data "hello world!")
 * on top of the PiCo API.
 *
 * We used a mix of static functions and lambdas in order to show the support
 * of various user code styles provided by PiCo operators.
 */

#include <iostream>
#include <string>
#include <sstream>

#include "Internals/Types/KeyValue.hpp"
#include "Operators/FlatMap.hpp"
#include "Operators/InOut/ReadFromFile.hpp"
#include "Operators/InOut/WriteToDisk.hpp"
#include "Operators/PReduce.hpp"
#include "Operators/Reduce.hpp"
#include "Pipe.hpp"

typedef KeyValue<std::string, int> KV;

/* static tokenizer function */
static auto tokenizer = [](std::string in) {
	std::istringstream f(in);
	std::vector<std::string> tokens;
	std::string s;

	while (std::getline(f, s, ' ')) {
		tokens.push_back(s);
	}
	return tokens;
};

int main(int argc, char** argv) {
	// parse command line
	if (argc < 2) {
		std::cerr << "Usage: ./pico_wc <input file> <output file>\n";
		return -1;
	}
	std::string filename = argv[1];
	std::string outputfilename = argv[2];

	/* define a generic word-count pipeline */
	Pipe countWords;
	countWords
	.add(FlatMap<std::string, std::string>(tokenizer)) //
	.add(Map<std::string, KV>([&](std::string in) {return KV(in,1);})) //
	.add(PReduce<KV>([&](KV v1, KV v2) {return v1+v2;}));

	// countWords can now be used to build streaming/batch pipelines */

	/* define i/o operators from/to file */
	ReadFromFile<std::string> reader(filename, [](std::string s) {return s;});
	WriteToDisk<KV> writer(outputfilename, [](KV in) {return in;});

	/* compose the pipeline */
	Pipe p2;
	p2 //the empty pipeline
	.add(reader) //
	.to(countWords) //
	.add(writer);

	/* execute the pipeline */
	p2.run();

	/* print the semantic DAG and generate dot file */
	p2.print_DAG();
	p2.to_dotfile("wordcount.dot");

	return 0;
}