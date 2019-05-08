#include "transaction.hpp"

Desc::Desc(unsigned int size, Operation *ops)
{
	this->size = size;
	this->ops = ops;
	// Transactions are always active at start.
	status.store(active);
	// Returned values are never safe to access until they have been explicitly set.
	returnedValues.store(false);
	// The page map always starts out empty.
	pages.store(NULL);
	// The RWSet always starts out empty.
	set.store(NULL);
	return;
}

Desc::~Desc()
{
	//delete ops;
	return;
}

VAL *Desc::getResult(size_t index)
{
	// If we request a result at an invalid operation index.
	if (index >= size)
	{
		return NULL;
	}
	// If the transaction has not yet committed.
	if (status.load() != committed)
	{
		return NULL;
	}
	// Return the value returned by the operation.
	return &(ops[index].ret);
}

void Desc::print()
{
	const char *statusStrList[] = {"active", "committed", "aborted"};
	size_t statusStrIndex = 0;
	switch (status.load())
	{
	case active:
		statusStrIndex = 0;
		break;
	case committed:
		statusStrIndex = 1;
		break;
	case aborted:
		statusStrIndex = 2;
		break;
	}
	printf("Transaction status:\t%s\n", statusStrList[statusStrIndex]);

	printf("Ops: (size=%u)\n", size);
	for (size_t i = 0; i < size; i++)
	{
		ops[i].print();
		printf("\n");
	}
}

void Operation::print()
{
	const char *typeStrList[] = {"pushBack", "popBack", "reserve", "read", "write", "size"};
	size_t typeStrIndex = 0;
	switch (type)
	{
	case pushBack:
		typeStrIndex = 0;
		break;
	case popBack:
		typeStrIndex = 1;
		break;
	case reserve:
		typeStrIndex = 2;
		break;
	case read:
		typeStrIndex = 3;
		break;
	case write:
		typeStrIndex = 4;
		break;
	case size:
		typeStrIndex = 5;
		break;
	}
	printf("Type:\t%s\n", typeStrList[typeStrIndex]);

	printf("index:\t%lu\n", index);
	printf("value:\t%lu\n", val);
	printf("return:\t%lu\n", ret);
}