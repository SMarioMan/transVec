/*
This file holds definitions of transactions and operations that can be executed.
The actual vector will pre-process each of these transactions into a simpler read/write set.
*/
#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <atomic>
#include <cstddef>

// Use this typedef to quickly change what type of objects we're working with.
// TODO: Does this actually belong here?
typedef int VAL;

// This reserved value indicates that a value cannot be set by a read or write here.
// Must differ depending on T.
// Consider templating the unset value relative to T?
const VAL UNSET = INT32_MIN;

template <class T>
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
        size
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
    T val;
    // The return value for this operation.
    // Only used for read, pop, and size.
    // Only safe to read if the transaction has committed.
    T ret;
};

template <class T>
// This is the descriptor generated by the programmer.
// This will be converted into an internal transaction to run on the shared datastructure.
struct Desc
{
    // The status of a transaction.
    enum TxStatus
    {
        active,
        committed,
        aborted
    };

    // The number of operations in the transaction.
    unsigned int size = 0;
    // An array of the operations themselves.
    Operation<T> *ops;
    // The status of the transaction.
    std::atomic<TxStatus> status;
    // The status of the returned values.
    // They are not safe to access until this is true.
    std::atomic<bool> returnedValues;

    // Create a descriptor object.
    // ops:     An array of operations, passed by reference.
    // size:    The number of operations in the operations array.
    Desc(unsigned int size, Operation<T> *ops);
    ~Desc();

    // Used to get our final results after a transaction commits.
    T *getResult(size_t index);
};

#endif