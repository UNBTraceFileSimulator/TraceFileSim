/*
 * Simulator.hpp
 *
 *  Created on: Sep 3, 2013
 *      Author: GarCoSim
 */

#ifndef SIMULATOR_HPP_
#define SIMULATOR_HPP_
 
#include <stdio.h>
#include <fstream>
#include <ctime>
#include "MemoryManager.hpp"
#include <string>
#include <stdlib.h>

#define ONE_SECOND_PASSED ((double(clock() - start) / CLOCKS_PER_SEC) >= 1.0f)

using namespace std;

namespace traceFileSimulator {

class Simulator {
public:
	Simulator(char* traceFilePath, int heapSize, int highWatermark, int garbageCollector, int traversal, int allocator);
	virtual ~Simulator();
	int lastStepWorked();
	int doNextStep();
	void printStats();
	void lastStats();

private:
	string getNextLine();
	void allocateToRootset(string line);
	void allocateObject(string line);
	void referenceOperation(string line);
	void deleteRoot(string line);
	void addToRoot(string line);
	void readObject(string line);
	void allocateToClassObject(string line);

	ifstream myTraceFile;
	
	int myLastStepWorked;
	MemoryManager* myMemManager;
	
	//debug
	int counter;
	clock_t start;
	int seconds;
};

} 
#endif /* SIMULATOR_HPP_ */
