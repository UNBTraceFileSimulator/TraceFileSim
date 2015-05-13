/*
 * Simulator.cpp
 *
 *  Created on: Sep 3, 2013
 *      Author: GarCoSim
 */

#include "Simulator.hpp"

using namespace std;

extern int gLineInTrace;
extern int gAllocations;
extern int forceAGCAfterEveryStep;

namespace traceFileSimulator {

Simulator::Simulator(char* traceFilePath, int heapSize, int highWatermark, int garbageCollector, int traversal, int allocator) {
	myLastStepWorked = 1;
	myTraceFile.open(traceFilePath);
	if(!myTraceFile.good()){
		fprintf(stderr, "File open failed.\n");
		exit(1);
	}

	myMemManager = new MemoryManager(heapSize, highWatermark, garbageCollector, traversal, allocator);

	if (!myMemManager->loadClassTable((string)traceFilePath))
		fprintf(stdout, "No class table found\n");

	counter = 0;
	start = clock();
	seconds = 0;
}

string Simulator::getNextLine(){
	string line = "";
	//try parsing next line, we skip empty lines

	if (myTraceFile.eof()) {
		myLastStepWorked = 0;
		return line;
	}

	do {
		if(getline(myTraceFile, line)){
			myLastStepWorked = 1;
		} else {
			myLastStepWorked = 0;
		}
		gLineInTrace++;
	} while (line.size() == 0); 

	return line;
}

void Simulator::lastStats() {
	myMemManager->lastStats();
}

int Simulator::doNextStep(){
	string traceLine = getNextLine();
	if (ONE_SECOND_PASSED) {
		start = clock();
		seconds++;
		printf("[%3ds] Line in tracefile: %7d\n", seconds, gLineInTrace);
	}
	if(myLastStepWorked){
		//if content exists, advice the MM(memory manager) to execute
		char firstChar = traceLine.at(0);
		switch(firstChar) {
			case 'w':
				referenceOperation(traceLine);
				break;
			case 'a':
				allocateToRootset(traceLine);
				//next line is a '+', which we skip since it adds the newly created object
				//to the rootset, which already happened in the simulator
				getNextLine();
				break;
			case '+':
				//allocateToRootset(traceLine);
				addToRoot(traceLine);
				break;
			case '-':
				deleteRoot(traceLine);
				break;
			case 'c':
				allocateToClassObject(traceLine);
				break;
			case 'r':
				readObject(traceLine);
				break;
			default:
				//gLineInTrace++;
			break;
		}

	}

	if (forceAGCAfterEveryStep) 
		myMemManager->forceGC();

	/*This line calls a garbage collec after each line. Usually useful
	 * in order to analyse the actual heap use of the file.*/
	//myMemManager->evalCollect();

	/*print stats to log file after each line
	 *.. usually used for result collection and comparison*/
	//myMemManager->printStats();


	return 0;
}

void Simulator::allocateToClassObject(string line) {
	int thread, id;
	int pos, length;
	int classID;

	if (!myMemManager->hasClassTable())
		return;

	pos = line.find('T')+1;
	length = line.find(' ',pos)-pos;
	thread = atoi(line.substr(pos,length).c_str());

	pos = line.find('O')+1;
	length = line.find(' ',pos)-pos;
	id = atoi(line.substr(pos,length).c_str());

	pos = line.find('C')+1;
	length = line.find('\n',pos)-pos;
	classID = atoi(line.substr(pos,length).c_str());

	myMemManager->addObjectToClass(thread, classID, id);
}

int Simulator::lastStepWorked(){
	if(myLastStepWorked == 1){
		return 1;
	}
	return 0;
}

void Simulator::readObject(string line){
	int thread, objectID;
	int pos, length;
	pos = line.find('T')+1;
	length = line.find(' ',pos)-pos;
	thread = atoi(line.substr(pos,length).c_str());

	pos = line.find('O')+1;
	length = line.find('\n',pos)-pos;
	objectID = atoi(line.substr(pos,length).c_str());

	myMemManager->hotnessRelation(thread, objectID);
}

void Simulator::allocateToRootset(string line){
	int thread, id, size, refCount;
	int pos, length;
	int classID;

	pos = line.find('T')+1;
	length = line.find(' ',pos)-pos;
	thread = atoi(line.substr(pos,length).c_str());

	// pos = line.find('R')+1;
	// length = line.find(' ',pos)-pos;
	// rootsetIndex = atoi(line.substr(pos,length).c_str());

	pos = line.find('O')+1;
	length = line.find(' ',pos)-pos;
	id = atoi(line.substr(pos,length).c_str());

	pos = line.find('S')+1;
	length = line.find(' ',pos)-pos;
	size = atoi(line.substr(pos,length).c_str());
	size += sizeof(Object);

	pos = line.find('N')+1;
	length = line.find('\n',pos)-pos;
	refCount = atoi(line.substr(pos,length).c_str());

	if (myMemManager->hasClassTable()) {
		pos = line.find('C')+1;
		length = line.find('\n',pos)-pos;
		classID = atoi(line.substr(pos,length).c_str());
	} else
		classID = 0;

	myMemManager->allocateObjectToRootset(thread, id, size, refCount, classID);
}

void Simulator::deleteRoot(string line){
	int thread, id;
	size_t pos;
	int length;

	pos = line.find('T')+1;
	length = line.find(' ',pos)-pos;
	thread = atoi(line.substr(pos,length).c_str());

	// for now we skip the removal of classes in the tracefile, needs to be addressed in future releases
	pos = line.find('C');
	if (pos != string::npos)
		return;

	pos = line.find('O')+1;
	length = line.find(' ',pos)-pos;
	id = atoi(line.substr(pos,length).c_str());

	myMemManager->requestRootDelete(thread, id);
}

void Simulator::addToRoot(string line){
	int thread, id;
	int pos, length;

	pos = line.find('T')+1;
	length = line.find(' ',pos)-pos;
	thread = atoi(line.substr(pos,length).c_str());

	pos = line.find('O')+1;
	length = line.find(' ',pos)-pos;
	id = atoi(line.substr(pos,length).c_str());

	myMemManager->requestRootAdd(thread, id);
}

void Simulator::allocateObject(string line){
	int thread, parent, parentSlot, id, size, refCount;
	int pos, length;
	int classID;

	pos = line.find('T')+1;
	length = line.find(' ',pos)-pos;
	thread = atoi(line.substr(pos,length).c_str());

	pos = line.find('P')+1;
	length = line.find(' ',pos)-pos;
	parent = atoi(line.substr(pos,length).c_str());

	pos = line.find('#')+1;
	length = line.find(' ',pos)-pos;
	parentSlot = atoi(line.substr(pos,length).c_str());

	pos = line.find('O')+1;
	length = line.find(' ',pos)-pos;
	id = atoi(line.substr(pos,length).c_str());

	pos = line.find('S')+1;
	length = line.find(' ',pos)-pos;
	size = atoi(line.substr(pos,length).c_str());
	size += sizeof(Object);

	pos = line.find('N')+1;
	length = line.find('\n',pos)-pos;
	refCount = atoi(line.substr(pos,length).c_str());

	//check objectID for NULL object.
	if(id == 0){
		fprintf(stderr, "ERROR (%d): Object ID 0 is reserved for a NULL pointer.\n", gLineInTrace);
		exit(1);
	}

	if (myMemManager->hasClassTable()) {
		pos = line.find('C')+1;
		length = line.find('\n',pos)-pos;
		classID = atoi(line.substr(pos,length).c_str());
	} else
		classID = 0;

	myMemManager->allocateObject(thread, parent, parentSlot, id, size, refCount, classID);
}

void Simulator::referenceOperation(string line){
	int thread, parentID, parentSlot, childId;
	int pos, length;
	pos = line.find('T')+1;
	length = line.find(' ',pos)-pos;
	thread = atoi(line.substr(pos,length).c_str());

	pos = line.find('P')+1;
	length = line.find(' ',pos)-pos;
	parentID = atoi(line.substr(pos,length).c_str());

	pos = line.find('#')+1;
	length = line.find(' ',pos)-pos;
	parentSlot = atoi(line.substr(pos,length).c_str());

	pos = line.find('O')+1;
	length = line.find('\n',pos)-pos;
	childId = atoi(line.substr(pos,length).c_str());

	myMemManager->setPointer(thread, parentID, parentSlot, childId);
}

void Simulator::printStats(){
	myMemManager->printStats();
}

Simulator::~Simulator() {

}

}
