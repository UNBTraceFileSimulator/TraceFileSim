/*
 * MemoryManager.cpp
 *
 *  Created on: 2013-09-03
 *      Author: GarCoSim
 */

#include "MemoryManager.hpp"
#include "MarkSweepCollector.hpp"
#include "../defines.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

extern FILE* gLogFile;
extern FILE* gDetLog;
extern int gLineInTrace;

namespace traceFileSimulator {

MemoryManager::MemoryManager(int heapSize, int highWatermark) {
	initAllocators(heapSize);
	initContainers();
	initGarbageCollectors(highWatermark);
}

void MemoryManager::initAllocators(int heapsize) {
	int i;
	int* genSizes = computeHeapsizes(heapsize);
	for (i = 0; i < GENERATIONS; i++) {
		myAllocators[i] = new Allocator(genSizes[i]);
	}
}

void MemoryManager::initContainers() {
	int i;
	for (i = 0; i < GENERATIONS; i++) {
		myObjectContainers[i] = new ObjectContainer();
	}
}

void MemoryManager::initGarbageCollectors(int highWatermark) {
	int i;
	for (i = 0; i < GENERATIONS; i++) {
		myGarbageCollectors[i] = new MarkSweepCollector(myAllocators[i],
				myObjectContainers[i], (MemoryManager*) this, highWatermark, i);
	}
}

void MemoryManager::statBeforeCompact(int myGeneration) {
	int i;
	for(i = 0;i<=myGeneration;i++){
		stats[i] = myAllocators[i]->getFreeSize();
	}
}

void MemoryManager::statAfterCompact(int myGeneration) {
	int i;
	for(i = 0;i<=myGeneration;i++){
		stats[i] -= myAllocators[i]->getFreeSize();
		if(stats[i] != 0){
			fprintf(stderr,"ERROR(Line %d):compact incosistency %d bytes for gen %d\n",
					gLineInTrace, stats[i],i);
		}
	}

}

int MemoryManager::shift(int size){
	//the idea: if there is still space for this object in the highest generation,
	//gc until promotes happen rather than crash the application
	int result = -1;
	int outOfMemory = 0;
	int spaceOnTop = myAllocators[GENERATIONS-1]->getFreeSize();
	while(result == -1 && spaceOnTop >= size){
		if(WRITE_DETAILED_LOG==1){
			fprintf(gDetLog,"(%d) SHIFTING for %d\n",gLineInTrace,size);
		}
		myGarbageCollectors[GENERATIONS-1]->collect(4);
		outOfMemory = myGarbageCollectors[GENERATIONS-1]->promotionPhase();
		if(outOfMemory==-1){
			fprintf(stderr,"(%d) OUT OF MEMORY: (%d)\n",gLineInTrace,size);
			exit(1);
		}
		result = myAllocators[0]->gcAllocate(size);
	}
	return result;
}

int MemoryManager::evalCollect(){
	myGarbageCollectors[GENERATIONS-1]->collect(17);
	return 0;
}

int MemoryManager::allocate(int size, int generation) {
	//check if legal generation
	if (generation < 0 && generation > GENERATIONS - 1) {
		fprintf(stderr, "ERROR (Line %d): allocate to illegal generation: %d\n",
				gLineInTrace, generation);
		exit(1);
	}
	int reason = 1;
//	if(isPromotion == 1){
//		reason = 4;
//	}
	int result = -1;
	int gen = generation;
	//try allocating in the generation
	result = myAllocators[generation]->gcAllocate(size);
	while (result == -1 && gen < GENERATIONS) {
		if (WRITE_DETAILED_LOG == 1) {
			fprintf(gDetLog,
					"(%d) Trigger Gc in generation %d.\n",
					gLineInTrace, gen);
		}
		myGarbageCollectors[gen]->collect(reason);
		result = myAllocators[generation]->gcAllocate(size);
		gen++;
	}
	if (gen > generation) {
		//gcs were made. promote if possible
		myGarbageCollectors[gen - 1]->promotionPhase();
	}

	if(result == -1 && SHIFTING == 1){
		//try shifting
		result = shift(size);
	}

	return result;
}

void MemoryManager::addRootToContainers(Object* object, int thread,
		int rootsetIndex) {

	int i;
	for (i = 0; i < GENERATIONS; i++) {
		if (i == GENERATIONS - 1) {
			myObjectContainers[i]->addToRoot(object, thread, rootsetIndex);
			//fprintf(stderr,"(%d)DEBUG: rootset %d\n",gLineInTrace, myObjectContainers[i]->getRootSize());
			//if(myObjectContainers[1]->getRootSize() != myObjectContainers[0]->getGenRootCount()){
				//exit(1);
				//fprintf(stderr,"(%d)DEBUG: EXIT\n",gLineInTrace);
			//}
		} //otherwise if there is more than one generation, add new object to remSets
		else {
			myObjectContainers[i]->add(object);
			myObjectContainers[i]->addToGenRoot(object);
			if (WRITE_DETAILED_LOG == 1) {
				fprintf(gDetLog, "(%d) Adding %d to remset %d\n", gLineInTrace,
						object->getID(), i);
			}
			//fprintf(stderr,"(%d)DEBUG: remset %d\n",gLineInTrace, myObjectContainers[i]->getGenRootCount());

		}
	}
}

int MemoryManager::allocateObjectToRootset(int thread, int id,
		int size, int refCount) {

	//find empty rootset slot. resize rootset if needed
	int rootsetIndex = myObjectContainers[GENERATIONS-1]->getRootsetSlot(thread);

	if (WRITE_DETAILED_LOG == 1) {
		fprintf(gDetLog, "(%d) Add Root %d,%d with id %d\n", gLineInTrace,
				thread, rootsetIndex, id);
	}
	//get allocation address in Generation 0
	int address = allocate(size, 0);
	if (address == -1) {
		fprintf(gLogFile, "Failed to allocate %d bytes in trace line %d.\n",
				size, gLineInTrace);
		fprintf(stderr, "ERROR(Line %d): Out of memory (%d bytes)\n",gLineInTrace,size);
		exit(1);
	}

	//create Object
	Object* object = new Object(id, size, refCount, address);
	object->setGeneration(0);
	//add to Containers
	addRootToContainers(object, thread, rootsetIndex);

	if (DEBUG_MODE == 1) {	
		myGarbageCollectors[GENERATIONS - 1]->collect(3);
		myGarbageCollectors[GENERATIONS - 1]->promotionPhase();
	}
	return 0;
}

int MemoryManager::requestRootDelete(int thread, int id){
	int rootsetIndex = myObjectContainers[GENERATIONS-1]->getRootsetIndexByID(thread,id);
	Object* oldRoot = myObjectContainers[GENERATIONS - 1]->getRoot(thread,rootsetIndex);
	myObjectContainers[GENERATIONS - 1]->removeFromRoot(thread,rootsetIndex);
	//remove the root from rem sets.
	int i;
	for(i=0;i<GENERATIONS-1;i++){
		myObjectContainers[i]->removeFromGenRoot(oldRoot);
	}
	return 0;

}

int MemoryManager::requestRootAdd(int thread, int id){
	Object* obj = myObjectContainers[GENERATIONS-1]->getByID(id);
	int rootSlot = myObjectContainers[GENERATIONS-1]->getRootsetSlot(thread);
	myObjectContainers[GENERATIONS-1]->addToRoot(obj, thread, rootSlot);
	return 0;

}

void MemoryManager::requestDelete(Object* object, int gGC) {
	if (WRITE_DETAILED_LOG == 1) {
		fprintf(gDetLog, "(%d) Delete object with id %d\n", gLineInTrace,
				object->getID());
	}
	int i;
	int objGeneration = object->getGeneration();
	//int objID = object->getID();
	//delete object from all Gen Roots it might be in
	for (i = objGeneration + 1; i < GENERATIONS; i++) {
//		if (i != GENERATIONS - 1) {
//			while (myObjectContainers[i]->removeFromGenRoot(object) != -1) {
//				if (WRITE_DETAILED_LOG == 1) {
//					fprintf(gDetLog,
//							"(%d) Removing %d from remset %d (deleteObj)\n",
//							gLineInTrace, object->getID(), i);
//				}
//			}
//		}
		int status = myObjectContainers[i]->removeReferenceTo(object);
		if (status == -1) {
			fprintf(stderr,
					"ERROR(Line %d):Object %d(g%d) could not be removed from object container %d\n",
					gLineInTrace, object->getID(), objGeneration, i);
		}
	}

	//now free in allocator and delete object
	myAllocators[objGeneration]->gcFree(object);
	myObjectContainers[objGeneration]->deleteObject(object);
}

void MemoryManager::requestFree(Object* object) {

//	if (WRITE_DETAILED_LOG == 1) {
//		fprintf(gDetLog, "(%d) Free request id %d\n", gLineInTrace,
//				object->getID());
//	}

	//if (object && object->getFreed() != 1) {
	if (object) {
		int gen = object->getGeneration();
		myAllocators[gen]->gcFree(object);
	}
}

void MemoryManager::requestReallocate(Object* object) {
//	if (WRITE_DETAILED_LOG == 1) {
//		fprintf(gDetLog, "(%d) Reallocate request for id %d\n", gLineInTrace,
//				object->getID());
//	}

	if (object) {
		int gen = object->getGeneration();
		int size = object->getPayloadSize();
		int address = myAllocators[gen]->gcAllocate(size);
		if (address == -1) {
			fprintf(stderr,
					"ERROR(Line %d):Could not reallocate Object %d to gen %d\n",
					gLineInTrace, object->getID(), gen);
			exit(1);
		}
		object->updateAddress(address);
		//object->setFreed(0);

	}
}

void MemoryManager::requestResetAllocationPointer(int generation) {
	if (WRITE_DETAILED_LOG == 1) {
		fprintf(gDetLog, "(%d) Request to reset allocation pointers\n",
				gLineInTrace);
	}
	int i;
	for (i = 0; i <= generation; i++) {
		myAllocators[i]->setAllocationSeearchStart(0);
	}
}

int MemoryManager::requestPromotion(Object* object) {
	if (object->getGeneration() == GENERATIONS - 1) {
		if (WRITE_DETAILED_LOG == 1) {
			fprintf(gDetLog,
					"(%d) Request to promote %d, but as it is in maxGen, not granted.\n",
					gLineInTrace, object->getID());
		}
		return 0;
	}

	int oldGen = object->getGeneration();
	int newGen = oldGen + 1;
	int size = object->getPayloadSize();

	if (WRITE_DETAILED_LOG == 1) {
		fprintf(gDetLog, "(%d) Request to promote %d from %d to %d\n",
				gLineInTrace, object->getID(), oldGen, newGen);
	}

	int address = myAllocators[newGen]->gcAllocate(size);
	if (address == -1) {
		//there is not enough space upstairs, stay where you are for a little longer
		if (WRITE_DETAILED_LOG == 1) {
			fprintf(gDetLog,
					"(%d) Request to promote %d from %d to %d not possible (no space)\n",
					gLineInTrace, object->getID(), oldGen, newGen);
		}
		//this line signalizes that there was an out of space error
		return 1;
	}

	//promote object
	myAllocators[oldGen]->gcFree(object);
	object->updateAddress(address);
	object->setGeneration(newGen);
	//remove from old generation
	myObjectContainers[oldGen]->removeReferenceTo(object);
	//remove all remSet entries
	while (myObjectContainers[oldGen]->removeFromGenRoot(object) != -1) {
		if (WRITE_DETAILED_LOG == 1) {
			fprintf(gDetLog,
					"(%d) Removing myself %d from remset %d (promotion))\n",
					gLineInTrace, object->getID(), oldGen);
		}
	}
	//handle children
	int i;
	for (i = 0; i < object->getPointersMax(); i++) {
		Object* child = object->getReferenceTo(i);
		if (child && child->getGeneration() == oldGen) {
			myObjectContainers[oldGen]->addToGenRoot(child);
			if (WRITE_DETAILED_LOG == 1) {
				fprintf(gDetLog,
						"(%d) Adding %d to remset %d (parent (%d) was promoted))\n",
						gLineInTrace, child->getID(), oldGen, object->getID());
			}
		}
	}
	return 0;
}

void MemoryManager::addToContainers(Object* object) {
	int i;
	for (i = 0; i < GENERATIONS; i++) {
		myObjectContainers[i]->add(object);
	}
}

int MemoryManager::allocateObject(int thread, int parentID, int parentSlot,
		int id, int size, int refCount) {
	if (WRITE_DETAILED_LOG == 1) {
		fprintf(gDetLog,
				"(%d) AllocateObject by thread %d. Parent id:%d, slot: %d with id %d\n",
				gLineInTrace, thread, parentID, parentSlot, id);
	}

	//get allocation address
	int address = allocate(size, 0);
	if (address == -1) {
		fprintf(gLogFile, "Failed to allocate %d bytes in trace line %d.\n",
				size, gLineInTrace);
		fprintf(stderr, "ERROR(Line %d): Out of memory (%d bytes)\n",gLineInTrace,size);
		exit(1);
	}
	Object* parent = myObjectContainers[GENERATIONS - 1]->getByID(parentID);
	int parentGeneration = parent->getGeneration();
	//remove old child from remSets
	Object* oldChild = parent->getReferenceTo(parentSlot);
	if (oldChild && parentGeneration > oldChild->getGeneration()) {
		int i;
		for (i = oldChild->getGeneration(); i < parentGeneration; i++) {
			if (WRITE_DETAILED_LOG == 1) {
				fprintf(gDetLog,
						"(%d) Removing %d from remset %d (i am oldchild of %d))\n",
						gLineInTrace, oldChild->getID(), i, parent->getID());
			}
			int status = myObjectContainers[i]->removeFromGenRoot(oldChild);
			if (status == -1) {
				fprintf(stderr,
						"ERROR (Line %d): could not remove oldChild %d from remset %d",
						gLineInTrace, oldChild->getID(), i);
			}
		}
	}
	//create Object
	Object* object = new Object(id, size, refCount, address);
	object->setGeneration(0);
	addToContainers(object);
	//connect to parent
	parent->setPointer(parentSlot, object);
	//add new object to remSets in needed
	if (parentGeneration > object->getGeneration()) {
		int i;
		for (i = object->getGeneration(); i < parentGeneration; i++) {
			myObjectContainers[i]->addToGenRoot(object);
			if (WRITE_DETAILED_LOG == 1) {
				fprintf(gDetLog,
						"(%d) Adding %d to remset %d (allocation))\n",
						gLineInTrace, object->getID(),i);
			}
		}
	}
	if (DEBUG_MODE == 1) {
		myGarbageCollectors[GENERATIONS - 1]->collect(3);
		myGarbageCollectors[GENERATIONS - 1]->promotionPhase();
	}
	return 0;
}

int MemoryManager::setPointer(int thread, int parentID, int parentSlot,
		int childID) {
	if (WRITE_DETAILED_LOG == 1) {
		fprintf(gDetLog, "(%d) Set pointer from %d(%d) to %d\n", gLineInTrace,
				parentID, parentSlot, childID);
	}
	Object* parent = myObjectContainers[GENERATIONS - 1]->getByID(parentID);

	//id 0 represents the NULL object.
	Object* child = NULL;
	int childGeneration = -1;
	if(childID != 0){
		child = myObjectContainers[GENERATIONS - 1]->getByID(childID);
		childGeneration = child->getGeneration();
	}
	int parentGeneration = parent->getGeneration();


	//check old child, if it created remSet entries and delete them
	Object* oldChild = parent->getReferenceTo(parentSlot);
	if (oldChild && parentGeneration > oldChild->getGeneration()) {
		int i;
		for (i = oldChild->getGeneration(); i < parentGeneration; i++) {
			if (WRITE_DETAILED_LOG == 1) {
				fprintf(gDetLog,
						"(%d) removing %d from remset %d (i am oldchild of (%d) in setpointer)\n",
						gLineInTrace, child->getID(), i, parent->getID());
			}
			int status = myObjectContainers[i]->removeFromGenRoot(oldChild);
			if (status == -1) {
				fprintf(stderr,
						"ERROR (Line %d): could not remove oldChild %d from remset %d\n",
						gLineInTrace, oldChild->getID(), i);
				//exit(1);
			}
		}
	}

	parent->setPointer(parentSlot, child);
	//childID 0 would mean a NULL object, which does not need to be remembered
	if (parentGeneration > childGeneration && childID != 0) {
		int i;
		for (i = childGeneration; i < parentGeneration; i++) {
			myObjectContainers[i]->addToGenRoot(child);
			if (WRITE_DETAILED_LOG == 1) {
				fprintf(gDetLog,
						"(%d) Adding %d to remset %d (parent (%d) got a new pointer to me))\n",
						gLineInTrace, child->getID(),i, parent->getID());
			}
		}
	}
	if (DEBUG_MODE == 1) {
		myGarbageCollectors[GENERATIONS - 1]->collect(3);
		myGarbageCollectors[GENERATIONS - 1]->promotionPhase();
	}
	return 0;
}

void MemoryManager::clearRemSets(){
	int i;
	for(i = 0;i<GENERATIONS;i++){
		myObjectContainers[i]->clearRemSet();
	}
}

void MemoryManager::requestRemSetAdd(Object* currentObj){
	int i;
	int objGen = currentObj->getGeneration();
	for(i= objGen ; i <  GENERATIONS-1; i++){
		myObjectContainers[i]->addToGenRoot(currentObj);
	}
}
int* MemoryManager::computeHeapsizes(int heapSize) {
	int heapLeft = heapSize;
	int* result = (int*) malloc(GENERATIONS * sizeof(int));
	int i;

	for (i = GENERATIONS - 1; i >= 0; i--) {
		if (i == 0) { // the youngest space gets what is left over
			result[i] = heapLeft;
		} else {
			result[i] = ceil(heapLeft * (1.0 - GENRATIO)); //no byte is left behind
			heapLeft = heapLeft * GENRATIO;
		}
		if (GEN_DEBUG == 1) {
			printf("GENDEBUG: G%d: %d\n", i, result[i]);
		}
	}

	if (GEN_DEBUG == 1) {
		int sum = 0;
		for (i = 0; i < GENERATIONS; i++) {
			sum = sum + result[i];
		}
		printf("GENDEBUG: Sum of Generations: %d\n", sum);
	}

	return result;
}

void MemoryManager::printStats() {
	for(int i=0; i < GENERATIONS;i++){
		myGarbageCollectors[i]->printStats();
	}

}

MemoryManager::~MemoryManager() {
}

}
