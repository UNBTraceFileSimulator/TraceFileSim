/*
 * TraversalCollector.cpp
 *
 *  Created on: 2015-02-16
 *      Author: GarCoSim
 */

#include "TraversalCollector.hpp"

extern int gLineInTrace;
extern FILE* gLogFile;
extern FILE* gDetLog;

extern FILE* gcFile;

extern clock_t start, stop;

namespace traceFileSimulator {

TraversalCollector::TraversalCollector() {
	fprintf(stderr, "This collector is WIP, please use a different collector!\n");
}

/**
 * Argument indicates the reason for collection: 0 - unknown, 1 - failed alloc, 2 - high watermark
 */
void TraversalCollector::collect(int reason) {
	statCollectionReason = reason;
	stop = clock();
	double elapsed_secs = double(stop - start)/CLOCKS_PER_SEC;
	fprintf(stderr, "GC #%d at %0.3fs", statGcNumber + 1, elapsed_secs);

	preCollect();

	copy();

	swap();

	postCollect();

	stop = clock();
	elapsed_secs = double(stop - start)/CLOCKS_PER_SEC;
	fprintf(stderr, " took %0.3fs\n", elapsed_secs);
}

void TraversalCollector::swap() {
	/*
	 * we need to do this step because we are simulating everything by maintaining an object list
	 * if we use a real allocator we do not delete the object, we just remove it from the object list
	 * it will then by overwritten afterwards. the real allocator just removes it from the object list
	 */
	int i;
	Object *currentObj;

	for (i = 0; i < myObjectContainer->getSize(); i++) {
		currentObj = myObjectContainer->getbySlotNr(i);
		if (currentObj) {
			if (!myAllocator->isInNewSpace(currentObj)) {
				if (!currentObj->isForwarded()) {
					statFreedObjects++;
					statFreedDuringThisGC++;
				}
				myMemManager->requestDelete(currentObj, myGeneration == GENERATIONS - 1 ? 1 : 0);
				//myObjectContainer->deleteObject(currentObj, !myAllocator->isRealAllocator());
			}
		}
	}

	myAllocator->swapHeaps();
}

void TraversalCollector::checkWatermark() {
	int size = myAllocator->getHeapSize();
	int free = myAllocator->getFreeSize();
	int ratio = 100 - (100 * free / size);
	if (ratio > myWatermark) {
		collect((int)reasonHighWatermark);
	}
}

void TraversalCollector::emptyHelpers() {
	while (!myQueue.empty())
		myQueue.pop();
	while (!myStack.empty())
		myStack.pop();
}

void TraversalCollector::copy() {
	//set all objects to dead and not visible firs
	initializeMarkPhase();

	emptyHelpers();

	getAllRoots();

	switch(order) {
		case breadthFirst:
			breadthFirstCopying();
			break;
		case depthFirst:
			depthFirstCopying();
			break;
		case hotness:
			hotnessCopying();
			break;
		default:
			break;
	}
}

void TraversalCollector::getAllRoots() {
	Object* currentObj;
	int i, j;

	// enqueue all roots
	if (myGeneration == GENERATIONS - 1) {
		//we are performing a glolab GC and can use it to fix possible rem set problems
		//we clear all rem sets and fill them again while performing the marking
		myMemManager->clearRemSets();

		for (i = 0; i < NUM_THREADS; i++) {
			for (j = 0; j < myObjectContainer->getRootsetSize(i) ; j++) {
				currentObj = myObjectContainer->getRoot(i, j);
				if (currentObj && currentObj->getVisited() == 0) {
					currentObj->setVisited(1);
					//currentObj->setAge(currentObj->getAge() + 1);
					//add to rem set if the root is in a younger generation.
					if (currentObj->getGeneration() < myGeneration) {
						myMemManager->requestRemSetAdd(currentObj);
					}
					myQueue.push(currentObj);
					myStack.push(currentObj);
				}
			}
		}
	} else {
		for (j = 0; j < myObjectContainer->getGenRootSize(); j++) {
			currentObj = myObjectContainer->getGenRoot(j);
			if (currentObj && currentObj->getVisited() == 0) {
				currentObj->setVisited(1);
				//currentObj->setAge(currentObj->getAge() + 1);
				myQueue.push(currentObj);
				myStack.push(currentObj);
			}
		}
	}
}

void TraversalCollector::breadthFirstCopying() {
	int i;
	Object* currentObj;
	Object* child;

	//breadth first through the tree using a queue
	while (!myQueue.empty()) {
		currentObj = myQueue.front();
		myQueue.pop();

		int kids = currentObj->getPointersMax();
		myAllocator->moveObject(currentObj);
		if (CHECK_CLASS(currentObj, "KoMaClass"))
			fprintf(stderr, "komaclass %d\n", currentObj->komaID);
		currentObj->setAge(currentObj->getAge() + 1);
		for (i = 0; i < kids; i++) {
			child = currentObj->getReferenceTo(i);
			//no matter if the child was processed before or not, add it to the rem set.
			if(child && child->getGeneration() < currentObj->getGeneration()){
				myMemManager->requestRemSetAdd(child);
			}
			if (child && child->getVisited() == 0
					&& child->getGeneration() <= myGeneration) {
				child->setVisited(1);

				myQueue.push(child);
			}
		}
	}
}

void TraversalCollector::depthFirstCopying() {
	int i;
	Object* currentObj;
	Object* child;

	//depth first through the tree using a stack
	while (!myStack.empty()) {
		currentObj = myStack.top();
		myStack.pop();
		int kids = currentObj->getPointersMax();
		myAllocator->moveObject(currentObj);
		currentObj->setAge(currentObj->getAge() + 1);
		for (i = 0; i < kids; i++) {
			child = currentObj->getReferenceTo(i);
			//no matter if the child was processed before or not, add it to the rem set.
			if(child && child->getGeneration() < currentObj->getGeneration()){
				myMemManager->requestRemSetAdd(child);
			}
			if (child && child->getVisited() == 0
					&& child->getGeneration() <= myGeneration) {
				child->setVisited(1);

				myStack.push(child);
			}
		}
	}
}

void TraversalCollector::hotnessCopying() {
	//TODO
}

void TraversalCollector::initializeMarkPhase() {
	Object* currentObj;
	int i;
	for (i = 0; i < myObjectContainer->getSize(); i++) {
		currentObj = myObjectContainer->getbySlotNr(i);
		if (currentObj) {
			if (WRITE_DETAILED_LOG == 1) {
				//fprintf(gDetLog, "(%d) MARK: %ld\n", gLineInTrace,
						//(long) currentObj);
			}
			currentObj->setVisited(0);
			currentObj->setIsAlive(0);
		}
	}
}

void TraversalCollector::preCollect() {
	start = clock();
	statFreedDuringThisGC = 0;
	statGcNumber++;
//	if(myGeneration == GENERATIONS -1 ){
//		promotionPhase();
//	}
}

void TraversalCollector::compact() {
	/*only alive objects are left in the container. If I traverse
	 through the live list, I get all elements*/
	//free everything.
	myMemManager->statBeforeCompact(myGeneration);
	freeAllLiveObjects();
	//allocate everything back.
	myMemManager->requestResetAllocationPointer(myGeneration);
	reallocateAllLiveObjects();
	myMemManager->statAfterCompact(myGeneration);
}

void TraversalCollector::freeAllLiveObjects() {
	int i;
	
	int end = myObjectContainer->getSize();
	for (i = 0; i < end; i++) {
		Object* currentObj = myObjectContainer->getbySlotNr(i);
		if (currentObj) {
			myMemManager->requestFree(currentObj);
		}
	}
	//myAllocator->freeAllSectors();
}

int TraversalCollector::promotionPhase() {
	if (gcsSinceLastPromotionPhase == 0) {
		//nothing to do here
		return 0;
	}
	int g, oldi;
	//in case the next generation is too full, a flag to wait
	int noSpaceUpstairs = 0;
	oldi = -1;
	int end = myObjectContainer->getSize();
	for (g = 0; g < end; g++) {
		Object* currentObj = myObjectContainer->getbySlotNr(g);
		if (g < oldi) {
			printf("oO\n");
		}
		//if object exists and is not in the oldest generation already
		if (currentObj && currentObj->getGeneration() < (GENERATIONS - 1)) {
			int age = currentObj->getAge();
			if (age
					>= PROMOTIONAGE + PROMOTIONAGE * myGeneration
							/*+ currentObj->getGeneration() * PROMOTIONAGEFACTOR*/) {
				//fprintf(stderr, "(%d) promoting object of age %d\n",gLineInTrace, age);
				if (currentObj->getGeneration() < GENERATIONS - 1) {
					noSpaceUpstairs = myMemManager->requestPromotion(
							currentObj);
					// end the promotion phase because of an "out of memory"
					// exception in the higher generation. It is not the best
					// solution
					if (noSpaceUpstairs == 1) {
						return -1;
					}
				}
			}
		}
		oldi = g;
	}
	return 0;
}

void TraversalCollector::reallocateAllLiveObjects() {
	int i;
	int end = myObjectContainer->getSize();
	for (i = 0; i < end; i++) {
		Object* currentObj = myObjectContainer->getbySlotNr(i);
		if (currentObj) {
			myMemManager->requestReallocate(currentObj);
		}
	}

}

TraversalCollector::~TraversalCollector() {
}

} 
