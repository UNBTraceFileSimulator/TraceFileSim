/*
 * ObjectContainer.cpp
 *
 *  Created on: 2013-09-03
 *      Author: GarCoSim
 */

#include "ObjectContainer.hpp"

extern int gLineInTrace;

namespace traceFileSimulator {

ObjectContainer::ObjectContainer() {
	int i;
	rootset.resize(NUM_THREADS);
	for (i = 0; i < NUM_THREADS; i++) {
		rootset.at(i).resize(ROOTSET_SIZE);
	}

	objectList.resize(1);
	remSet.resize(1);
	rootCount = 0;
	remCount = 0;
}

// we forward the object in all of our lists
void ObjectContainer::forwardObject(int slot) {
	unsigned int i, j;
	int id;

	id = objectList.at(slot)->getID();

	// update object list
	objectList.at(slot) = (Object*)objectList.at(slot)->getAddress();
	objectList.at(slot)->updateAddress((size_t)0); // remove forwarding pointer

	// update rootset
	for (i = 0; i < rootset.size(); i++)
		for (j = 0; j < rootset[i].size(); j++)
			if (rootset[i][j])
				if (rootset[i][j]->getID() == id)
					rootset[i][j] = objectList.at(slot);

	// update remset
	for (i = 0; i < remSet.size(); i++)
		if (remSet[i])
			if (remSet[i]->getID() == id)
			remSet[i] = objectList.at(slot);

}

void ObjectContainer::addToGenRoot(Object* object) {
	int remSetSlot = getRemSetSlot();
	if (remSetSlot == -1) {
		fprintf(stderr, "ERROR(Line %d): no remSet slot found\n", gLineInTrace);
		exit(1);
	}
	remSet.at(remSetSlot) = object;
	remCount++;
}

int ObjectContainer::removeFromGenRoot(Object* object){
	int i;
	for(i = 0 ; (unsigned int) i<remSet.size() ; i++){
		if(remSet.at(i) == object){
			remSet.at(i) = NULL;
			remCount--;
			return 0;
		}
	}
	return -1;
}

int ObjectContainer::add(Object* newObject) {
	int listSlot = getListSlot();
	if (listSlot == -1) {
		fprintf(stderr, "ERROR(line %d): no slot found\n", gLineInTrace);
		return -1;
	}

	objectList.at(listSlot) = newObject;
	return 0;
}

int ObjectContainer::removeFromRoot(int thread, int root ){
	rootset[thread][root] = NULL;
	rootCount--;
	return -1;
}

bool ObjectContainer::doesObjectExistInList(Object *queryObject) {
	unsigned int i;
	for (i = 0; i < objectList.size(); i++) {
		if (objectList.at(i) == queryObject) {
			return true;
		}
	}
	return false;
}

bool ObjectContainer::isAlreadyRoot(int thread, int id) {
	Object *obj = getByID(id);
	int i;

	for (i = 0; i < (int)rootset[thread].size(); i++)
		if (rootset[thread].at(i) == obj)
			return true;

	return false;
}

int ObjectContainer::addToRoot(Object* newObject, int thread, int rootSlot) {
	if (!doesObjectExistInList(newObject)) {
		int listSlot = getListSlot();
		if (listSlot == -1) {
			fprintf(stderr, "ERROR(line %d): no slot found\n", gLineInTrace);
			return -1;
		}
		objectList[listSlot] = newObject;
	}
	rootset[thread][rootSlot] = newObject;
	rootCount++;

	return 0;
}

Object* ObjectContainer::getByID(int id) {
	unsigned int i;
	for (i = 0; i < objectList.size(); i++) {
		if (objectList.at(i) != NULL) {
			int currentId = objectList.at(i)->getID();
			if (id == currentId) {
				objectList.at(i)->increaseHotness();
				return objectList.at(i);
			}
		}
	}
	fprintf(stderr, "ERROR(Line %d): object with this id (%d) was not found\n",
			gLineInTrace, id);
	return NULL;
}

Object* ObjectContainer::getbySlotNr(int slot) {
	//objectList.at(slot)->increaseHotness();
	return objectList.at(slot);
}

Object* ObjectContainer::getRoot(int thread, int rootSlot) {
	//rootset[thread][rootSlot]->increaseHotness();
	return rootset[thread][rootSlot];
}

int ObjectContainer::deleteObject(Object* object, bool deleteFlag) {
	if (!object)
		fprintf(stderr, "ERROR(Line %d): trying to delete a non existing object\n", gLineInTrace);
	int id = object->getID();
	return deleteObject(id, deleteFlag);
}

int ObjectContainer::deleteObject(int objectID, bool deleteFlag) {
	unsigned int i;
	Object* temp;
	for (i = 0; i < objectList.size(); i++) {
		temp = objectList[i];
		if (temp && temp->getID() == objectID) {
			// if we forwarded our object do it now, otherwise delete it
			if (temp->isForwarded())
				forwardObject(i);
			else
				objectList[i] = NULL;
			if (deleteFlag)
				delete (temp);
			return 0;
		}
	}
	fprintf(stderr, "ERROR(Line %d): object to delete not found. id: %d\n", gLineInTrace, objectID);
	return -1;
}

int ObjectContainer::getSize() {
	return objectList.size();
}

int ObjectContainer::removeReferenceTo(Object* object) {
	int i;
	for(i = 0 ; (unsigned int)i < objectList.size() ;i++){
		if(object == objectList.at(i)){
			objectList.at(i) = NULL;
			return 0;
		}
	}
	return -1;
}

int ObjectContainer::getGenRootCount() {
	return remCount;
}
int ObjectContainer::getGenRootSize() {
	return remSet.size();
}
int ObjectContainer::getRootSize() {
	return rootCount;
}

Object* ObjectContainer::getGenRoot(int slot) {
	if(slot < 0 && (unsigned int)slot > remSet.size()){
		fprintf(stderr,"ERROR(Line%d):Request illegal gen root. slot:%d\n",
				gLineInTrace,slot);
		exit(1);
	}
	remSet.at(slot)->increaseHotness();
	return remSet.at(slot);
}



int ObjectContainer::getRemSetSlot() {
	unsigned int i;
	for (i = 0; i < remSet.size(); i++) {
		if (!remSet.at(i)) {
			return i;
		}

		if (i + 1 == remSet.size()) {
			remSet.resize(remSet.size() * 2);
		}
	}
	fprintf(stderr, "remSetSlotERror: no list slot found\n");
	return -1;
}

int ObjectContainer::getRootsetSlot(int thread){
	unsigned int i;
	for (i = 0 ; i < rootset.at(thread).size() ; i++){
		if(!rootset.at(thread).at(i)){
			return i;
		}

		//resize
		if(i + 1 == rootset.at(thread).size()){
			rootset.at(thread).resize(rootset.at(thread).size()*2);
		}
	}
	//error occured
	fprintf(stderr, "(%d)Could not find rootset index.\n",gLineInTrace);
	return -1;
}

int ObjectContainer::getListSlot() {
	unsigned int i;
	for (i = 0; i < objectList.size(); i++) {
		if (!objectList.at(i)) {
			return i;
		}
		if (i + 1 == objectList.size()) {
			objectList.resize(objectList.size() * 2);
		}
	}
	fprintf(stderr, "listSlotERror: no list slot found\n");
	return -1;
}
int ObjectContainer::getRootsetSize(int thread){
	return rootset.at(thread).size();
}

void ObjectContainer::clearRemSet(){
	remSet.clear();
	remSet.resize(1);
	remCount = 0;
}

int ObjectContainer::getRootsetIndexByID(int thread, int id){
	unsigned int i;
	for(i = 0 ; i < rootset.at(thread).size() ; i++){
		if(rootset.at(thread).at(i) && rootset.at(thread).at(i)->getID() == id){
			return i;
		}
	}
	fprintf(stderr, "(%d) No root with id %d found in thread %d.\n",gLineInTrace, id, thread);
	return -1;
}

int ObjectContainer::countElements() {
	int result = 0;
	unsigned int i;
	for (i = 0; i < objectList.size(); i++) {
		if (objectList[i]) {
			result++;
		}
	}
	return result;
}

ObjectContainer::~ObjectContainer() {
}

} 
