/*
 * TraversalCollector.hpp
 *
 *  Created on: 2013-09-04
 *      Author: GarCoSim
 */

#ifndef TRAVERSALCOLLECTOR_HPP_
#define TRAVERSALCOLLECTOR_HPP_

#include "Collector.hpp"
#include "../Allocators/Allocator.hpp"
#include "../Main/ObjectContainer.hpp"
#include <queue>
#include <stack>
#include "../defines.hpp"
#include "../Main/MemoryManager.hpp"
#include <stdio.h>
#include <ctime>

using namespace std;

namespace traceFileSimulator {

class MemoryManager;

// This collector immplements a split-heap copying collection policy
class TraversalCollector : public Collector {
public:
	TraversalCollector();
	virtual ~TraversalCollector();
	void collect(int reason);
	int promotionPhase();
	void initializeHeap();

private:
	void copy();
	void compact();
	void initializeMarkPhase();
	void preCollect();
	void freeAllLiveObjects();
	void reallocateAllLiveObjects();
	void breadthFirstCopying();
	void depthFirstCopying();
	void getAllRoots();
	void emptyHelpers();
	void swap();
	void copyAndForwardObject(Object *o);
};

} 
#endif /* TRAVERSALCOLLECTOR_HPP_ */
