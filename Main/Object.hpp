/*
 * Object.hpp
 *
 *  Created on: Jul 30, 2013
 *      Author: GarCoSim
 */

#ifndef OBJECT_HPP_
#define OBJECT_HPP_

#include "../defines.hpp"
#include <vector>
#include <stdio.h>
#include <stdlib.h>

using std::vector;
namespace traceFileSimulator {

class Object {
public:
	Object(int id, int payloadSize, int maxPointers, int address, char *className);
	void setArgs(int id, int payloadSize, int maxPointers, char *className);
	virtual ~Object();
	size_t 	getAddress();
	void 	updateAddress(size_t newAddress);
	int 	getID();
	int 	getPayloadSize();
	int	 	getPointerCount();
	int 	getPointersMax();
	Object* getReferenceTo(int pointerNumber);
	int 	setPointer(int pointerNumber, Object* target);

	int 	getIsAlive();
	void 	setIsAlive(int value);
	int 	getVisited();
	void	setVisited(int value);

	int getAge() const {
		return myAge;
	}

	void setAge(int age) {
		myAge = age;
	}
	void setGeneration(int generation);
	int  getGeneration();

	int getFreed() const {
		return freed;
	}

	const char *getClassName() {
		return myName;
	}

	void setFreed(int freed) {
		this->freed = freed;
	}

	bool isForwarded() {
		return forwarded;
	}

	void setForwarded(bool value) {
		forwarded = value;
	}

	void increaseHotness() {
		hotness++;
	}

	double getHotness() {
		return hotness;
	}

	void decayHotness() {
		if (HOTNESS_DECAY)
			hotness *= (1 - HOTNESS_DECAY_FACTOR);
	}

	void addPointer(Object* object);

	//marcel: needed for debugging my traversals, can be deleted if not needed
	int komaID;

private:
	int 	myId;
	int freed;

	/*The actual object I am storing information about
	 * (not interesting for our purpose, it only has a size) */
	int  	myPayloadSize;

	/*How many objects am I pointing at? How many am I allowed to point at?*/
	int 	myPointersMax;
	int 	myPointersCurrent;

	//the starting address of the object on the virtual heap OR the forwarding address if split heaps
	size_t myAddress;

	/*the list of objects I am pointing at*/
	vector<Object*> pointers;

	//garbage collector stuff
	//TODO those two are basically the same. one could be removed
	int isVisited;
	int isAlive;

	//genCon
	int myAge;
    int	myGeneration;

    char *myName;
    bool forwarded;

    double hotness;

};

} 
#endif /* OBJECT_HPP_ */
