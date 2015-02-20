/*
 * Simulator.cpp
 *
 *  Created on: Sep 3, 2013
 *      Author: GarCoSim
 */

#include "Simulator.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;

extern int gLineInTrace;
extern int gAllocations;

namespace traceFileSimulator {

Simulator::Simulator(char* traceFilePath, int heapSize, int highWatermark) {
	myLastStepWorked = 1;
	myTraceFile.open(traceFilePath);
	if (!myTraceFile.good()) {
		fprintf(stderr, "File open failed.\n");
		return;
	}

	myMemManager = new MemoryManager(heapSize, highWatermark);
	counter = 0;
	outputCounter = 0;
}

string Simulator::getNextLine() {
	string line = "";
	//try parsing next line
	if (getline(myTraceFile, line)) {
		myLastStepWorked = 1;
	} else {
		myLastStepWorked = 0;
	}
	return line;
}

int Simulator::doNextStep() {
	string traceLine = getNextLine();
	gLineInTrace++;
	outputCounter--;
	if (outputCounter <= 0) {
		printf("\nLine:%7d\n", gLineInTrace);
		outputCounter = 10000;
	}
	if (myLastStepWorked) {
		//if content exists, advice the MM(memory manager) to execute
		char firstChar = traceLine.at(0);
		switch (firstChar) {
		case 'w':
			referenceOperation(traceLine);
			break;
		case 'a':
			allocateToRootset(traceLine);
			break;
		case '+':
			//allocateToRootset(traceLine);
			addToRoot(traceLine);
			break;
		case '-':
			deleteRoot(traceLine);
			break;
		default:
			//gLineInTrace++;
			break;
		}

	}

	/*This line calls a garbage collec after each line. Usually useful
	 * in order to analyse the actual heap use of the file.*/
	//myMemManager->evalCollect();
	/*print stats to log file after each line
	 *.. usually used for result collection and comparison*/
	//myMemManager->printStats();
	return 0;
}

int Simulator::lastStepWorked() {
	if (myLastStepWorked == 1) {
		return 1;
	}
	return 0;
}

void Simulator::allocateToRootset(string line) {
	int thread, id, size, refCount;
	int pos, length;

	pos = line.find('T') + 1;
	length = line.find(' ', pos) - pos;
	thread = atoi(line.substr(pos, length).c_str());

	pos = line.find('O') + 1;
	length = line.find(' ', pos) - pos;
	id = atoi(line.substr(pos, length).c_str());

	pos = line.find('S') + 1;
	length = line.find(' ', pos) - pos;
	size = atoi(line.substr(pos, length).c_str());

	pos = line.find('N') + 1;
	length = line.find('\n', pos) - pos;
	refCount = atoi(line.substr(pos, length).c_str());

	if (thread > NUM_THREADS) {
		fprintf(stderr,
				"ERROR (Line %d): Thread number too high. Please adjust max thread define in defines.hpp.\n",
				gLineInTrace);
		exit(1);
	}

	myMemManager->allocateObjectToRootset(thread, id, size, refCount);

	/*next line is a '+', which we skip since it adds the newly created object
	 to the rootset, which already happened in the simulator*/
	checkLineAfterAlloc(thread, id);
}

/***
 * Check if the line after an allocation adds the newly created object to
 * the expected thread. If not, a warning is printed and the line ignored.
 */
void Simulator::checkLineAfterAlloc(int thread, int id) {
	const char* currentLine = getNextLine().c_str();
	gLineInTrace++;
	char expected[80];

	sprintf(expected, "+ T%d O%d", thread, id);

	if (strcmp(expected, currentLine) != 0) {
		fprintf(stderr, "COMPARING: \n%s\n%s\n", currentLine, expected);
		fprintf(stderr,
				"WARNING (Line %d) : Allocation must be followed by the addition of the new object to the root set. Please check the trace format.\n",
				gLineInTrace);
	}
}

void Simulator::deleteRoot(string line) {
	int thread, id;
	int pos, length;

	pos = line.find('T') + 1;
	length = line.find(' ', pos) - pos;
	thread = atoi(line.substr(pos, length).c_str());

	pos = line.find('O') + 1;
	length = line.find(' ', pos) - pos;
	id = atoi(line.substr(pos, length).c_str());

	myMemManager->requestRootDelete(thread, id);
}

void Simulator::addToRoot(string line) {
	int thread, id;
	int pos, length;

	pos = line.find('T') + 1;
	length = line.find(' ', pos) - pos;
	thread = atoi(line.substr(pos, length).c_str());

	pos = line.find('O') + 1;
	length = line.find(' ', pos) - pos;
	id = atoi(line.substr(pos, length).c_str());

	if (thread > NUM_THREADS) {
		fprintf(stderr,
				"ERROR (Line %d): Thread number too high. Please adjust max thread define in defines.hpp.\n",
				gLineInTrace);
		exit(1);
	}

	myMemManager->requestRootAdd(thread, id);
}

void Simulator::allocateObject(string line) {
	int thread, parent, parentSlot, id, size, refCount;
	int pos, length;

	pos = line.find('T') + 1;
	length = line.find(' ', pos) - pos;
	thread = atoi(line.substr(pos, length).c_str());

	pos = line.find('P') + 1;
	length = line.find(' ', pos) - pos;
	parent = atoi(line.substr(pos, length).c_str());

	pos = line.find('#') + 1;
	length = line.find(' ', pos) - pos;
	parentSlot = atoi(line.substr(pos, length).c_str());

	pos = line.find('O') + 1;
	length = line.find(' ', pos) - pos;
	id = atoi(line.substr(pos, length).c_str());

	pos = line.find('S') + 1;
	length = line.find(' ', pos) - pos;
	size = atoi(line.substr(pos, length).c_str());

	pos = line.find('N') + 1;
	length = line.find('\n', pos) - pos;
	refCount = atoi(line.substr(pos, length).c_str());

	//check objectID for NULL object.
	if(id == 0){
		fprintf(stderr, "ERROR (%d): Object ID 0 is reserved for a NULL pointer.\n", gLineInTrace);
		exit(1);
	}

	myMemManager->allocateObject(thread, parent, parentSlot, id, size,
			refCount);
}

void Simulator::referenceOperation(string line) {
	int thread, parentID, parentSlot, childId;
	int pos, length;
	pos = line.find('T') + 1;
	length = line.find(' ', pos) - pos;
	thread = atoi(line.substr(pos, length).c_str());

	pos = line.find('P') + 1;
	length = line.find(' ', pos) - pos;
	parentID = atoi(line.substr(pos, length).c_str());

	pos = line.find('#') + 1;
	length = line.find(' ', pos) - pos;
	parentSlot = atoi(line.substr(pos, length).c_str());

	pos = line.find('O') + 1;
	length = line.find('\n', pos) - pos;
	childId = atoi(line.substr(pos, length).c_str());

	myMemManager->setPointer(thread, parentID, parentSlot, childId);
}

void Simulator::printStats() {
	myMemManager->printStats();
}

Simulator::~Simulator() {

}

}
