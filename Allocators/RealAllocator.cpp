/*
 * RealAllocator.cpp
 *
 *  Created on: 2015-03-04
 *      Author: GarCoSim
 */

#include "RealAllocator.hpp"

extern int gLineInTrace;

using namespace std;

namespace traceFileSimulator {

RealAllocator::RealAllocator() {
}

bool RealAllocator::isRealAllocator() {
	return true;
}

bool RealAllocator::isInNewSpace(Object *object) {
	int address = getLogicalAddress(object);
	
	if (address >= newSpaceOffset && address < myHeapSizeNewSpace)
		return true;

	return false;
}


void RealAllocator::moveObject(Object *object) {
	if (isInNewSpace(object))
		return;

	Object *temp;
	int size = object->getPayloadSize();

	size_t address = (size_t)allocateInNewSpace(size);

	if (address == (size_t)-1) {
		fprintf(stderr, "error moving object (size %d), old space %d, new space %d\n", size, getUsedSpace(false), getUsedSpace(true));
		exit(1);
	}
	object->updateAddress(address);
	temp = (Object*)address;

	// now we move the object
	//memcpy(&temp, &object, sizeof(*object));
	*temp = *object;
	// we do a hack for now
	//temp->setArgs(object->getID(), object->getPayloadSize(), object->getPointersMax(), (char*)object->getClassName());
	temp->setVisited(1);
	object->setForwarded(true);

	object = temp;
}

void RealAllocator::initializeHeap(int heapSize) {
	myHeapBitMap = new char[heapSize / 8 + 1];

	myHeapSizeOldSpace = heapSize;
	myLastSuccessAddressOldSpace = 0;
	myLastSuccessAddressNewSpace = heapSize / 2;
	myHeapSizeNewSpace = heapSize;

	statBytesAllocated = 0;
	statLiveObjects = 0;
	if (DEBUG_MODE && WRITE_ALLOCATION_INFO) {
		allocLog = fopen("alloc.log", "w+");
	}
	if (DEBUG_MODE && WRITE_HEAPMAP) {
		heapMap = fopen("heapmap.log", "w+");
	}
	newSpaceOffset = heapSize / 2;
	oldSpaceOffset = 0;
	overallHeapSize = heapSize;
	fprintf(stderr, "heap size %d\n", overallHeapSize);

	heap = (unsigned char*)malloc(heapSize * 8);
	myLastSuccessAddressOldSpace = (size_t)&heap[0];
	myLastSuccessAddressNewSpace = (size_t)&heap[0];
}

void RealAllocator::freeAllSectors() {
	int i;

	for (i = 0; i < overallHeapSize; i++) {
		setBitUnused(i);
	}

	statBytesAllocated = 0;
}

size_t RealAllocator::allocate(int size, int lower, int upper, size_t lastAddress) {
	if (size <= 0) {
		return -1;
	}

	//hope for the best, assume the worst
	int address = lower;
	int contiguous = 0;
	//nextFit search
	int i, bit;
	int passedBoundOnce = 0;
	//int end = (lastAddress - (size_t)(&heap) - 1);

	for (i = lower; i != upper; i++) {
	//for (i = lastAddress - (size_t)(&heap); i != end; i++) {
		bit = isBitSet(i);

		if (i == upper) {
			if (passedBoundOnce == 1) {
				return -1;
			}
			i = lower;
			contiguous = 0;
			address = i + 1;
			passedBoundOnce = 1;
		}

		if (bit == 1) {
			address = i + 1;
			contiguous = 0;
		} else {
			contiguous++;
			if (contiguous == size) {
				setAllocated(address, size);
				statBytesAllocated += size;
				statLiveObjects++;
				return (size_t)&heap[address];
			}
		}
	}

	return -1;
}

size_t RealAllocator::getLogicalAddress(Object *object) {
	size_t address = (size_t)object;
	size_t i;

	// find the address we're looking for
	for (i = 0; i < (size_t)overallHeapSize; i++)
		if (address == (size_t)&heap[i])
			return i;

	return -1;
}

void RealAllocator::gcFree(Object* object) {
	int size = object->getPayloadSize();

	setFree(getLogicalAddress(object), size);
	//object->setFreed(1);

	statLiveObjects--;
	statBytesAllocated -= size;
}

RealAllocator::~RealAllocator() {
}

}
