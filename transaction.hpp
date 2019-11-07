/*
This file holds definitions of transactions and operations that can be executed.
The actual vector will pre-process each of these transactions into a simpler read/write set.
*/
#ifndef TRANSACTION_HPP
#define TRANSACTION_HPP

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <map>
#include <ostream>
#include <string>

#include "define.hpp"
#include "deltaPage.hpp"
#include "memAllocator.hpp"

template <class T, size_t S>
class Page;

class RWSet;
#ifdef BOOSTEDVEC
class BoostedElement;
#endif

#ifdef CONFLICT_FREE_READS
// The global version counter.
// Used for conflict-free reads.
static std::atomic<size_t> globalVersionCounter(1);
#endif

// A standard, user-generated operation.
// Works with values of type T.
struct Operation
{
	// A high-level operation supported within a transaction.
	enum OpType
	{
		// Write to a position relative to the current size.
		// Ignores bounds checking, since size reading must be part of the transaction.
		pushBack,
		// Read from a position relative to the current size.
		// Ignores bounds checking, since size reading must be part of the transaction.
		popBack,
		// Ensure enough space has been allocated.
		reserve,
		// Read at an absolute position in the vector.
		// Can fail bounds checking.
		read,
		// Write at an absolute position in the vector.
		// Can fail bounds checking.
		write,
		// Simillar to read, but always at the size index (probably 1?).
		// Returned answer can be offset by a transaction's push and pop ops.
		size,
	};

	// The type of operation being performed.
	OpType type;
	// The index being affected by the operation.
	// Only used for read and write.
	// Also used as the value for size, mostly because it's a convienient and otherwise unused integer.
	size_t index;
	// The value being written.
	// Used for push and write.
	// Pop implicitly writes an unset value for bounds checking.
	VAL val;
	// The return value for this operation.
	// Only used for read, pop, and size.
	// Only safe to read if the transaction has committed.
	VAL ret;

	void print();
};

// This is the descriptor generated by the programmer.
// This will be converted into an internal transaction to run on the shared datastructure.
struct Desc
{
#ifndef BOOSTEDVEC
	// The status of a transaction.
	enum TxStatus
	{
		active,
		committed,
		aborted
	};

	// The status of the transaction.
	std::atomic<TxStatus> status;
	std::atomic<RWSet *> set;
#else
	RWSet *set;
	// A list of locks aquired that must be released when the transaction finishes.
	std::vector<BoostedElement *> locks;
#endif
	// The number of operations in the transaction.
	unsigned int size = 0;
	// An array of the operations themselves.
	Operation *ops;
#ifdef SEGMENTVEC
	// A list of pages for the transaction to insert.
	std::atomic<std::map<size_t, Page<VAL, SGMT_SIZE> *, std::less<size_t>, MemAllocator<std::pair<size_t, Page<VAL, SGMT_SIZE> *>>> *> pages;
#endif
#ifdef CONFLICT_FREE_READS
	// Used to determine how to reorder conflict-free reads.
	std::atomic<size_t> version;
	// Used to identify whether or not the transaction is of the conflict-free variety.
	bool isConflictFree = false;
#endif

	// Create a descriptor object.
	// ops:     An array of operations, passed by reference.
	// size:    The number of operations in the operations array.
	Desc(unsigned int size, Operation *ops);
	~Desc();

	// Used to get our final results after a transaction commits.
	VAL *getResult(size_t index);

	// Print out the contents of the vector at a given time.
	// This function is not atomic unless the transaction has committed or aborted.
	void print();
};

#endif