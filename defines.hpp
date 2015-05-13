/*
 * defines.hpp
 *
 *  Created on: 2013-09-04
 *      Author: GarCoSim
 */

#ifndef _DEFINES_HPP_
#define _DEFINES_HPP_

#define NUM_THREADS		50
#define ROOTSET_SIZE    50
#define VISUALIZE_GCS 	1

#define CLASS_OBJECT    -1

//DEBUGGING
#define DEBUG_MODE            0
#define WRITE_DETAILED_LOG    0
#define WRITE_HEAPMAP         0
#define WRITE_ALLOCATION_INFO 0

//GENERATIONAL GC
#define GENERATIONS        1
#define GEN_DEBUG          0
#define GENRATIO           0.3
#define PROMOTIONAGE       3
#define PROMOTIONAGEFACTOR 0
#define SHIFTING           1
#define SHIFTINGFACTOR     2

//FOR HOTNESS
#define HOTNESS_DECAY        0     // not implemented yet :)
#define HOTNESS_DECAY_FACTOR 0.2

enum traversalEnum {
					breadthFirst = 0,
					depthFirst,
					hotness
				};

enum collectorEnum {
						copyingGC = 0,
						markSweepGC,
						traversalGC
				};

enum allocatorEnum {
						realAlloc = 0,
						simulatedAlloc
				};

enum gcReason {
					reasonStatistics = 0,
					reasonFailedAlloc,
					reasonHighWatermark,
					reasonDebug,
					reasonShift,
					reasonEval,
					reasonForced
				};

#define DEBUGMSG(msg) (fprintf(stderr, "%s:%d - %s\n", __FILE__, __LINE__, msg))

#define DEBUGMSG_IFEQUAL(left, right, msg) if (left == right) DEBUGMSG(msg)

// create some fancy strings for debug output
#define TRAVERSAL_STRING (traversal == (int)breadthFirst ? "breadthFirst" : (traversal == (int)depthFirst ? "depthFirst" : "hotness"))
#define COLLECTOR_STRING (collector == (int)traversalGC ? "traversal" : (collector == (int)markSweepGC ? "markSweep" : "copying"))
#define ALLOCATOR_STRING (allocator == (int)simulatedAlloc ? "simulated" : "real")

#define CREATE_GLOBAL_FILENAME(name) (globalFilename = (name).substr(0, (name).find(".trace")))

#define CHECK_CLASS(object, string) (!strcmp(object->getClassName(), string))

#endif
