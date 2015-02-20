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
#include "MemoryManager.hpp"

using namespace std;

namespace traceFileSimulator {

class Simulator {
public:
	Simulator(char* traceFilePath, int heapSize, int highWatermark);
	virtual ~Simulator();
	int lastStepWorked();
	int doNextStep();
	void printStats();

private:
	string getNextLine();
	void allocateToRootset(string line);
	void allocateObject(string line);
	void referenceOperation(string line);
	void deleteRoot(string line);
	void addToRoot(string line);
	void checkLineAfterAlloc(int thread, int id);

	ifstream myTraceFile;
	int myLastStepWorked;
	int outputCounter;
	MemoryManager* myMemManager;
	
	//debug
	int counter;
};

} 
#endif /* SIMULATOR_HPP_ */
