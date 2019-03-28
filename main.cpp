#include "main.hpp"

void pushThread(int threadNum)
{
	// For each transaction.
	for (size_t i = 0; i < NUM_TRANSACTIONS; i++)
	{
		// A list of operations for the current thread.
		Operation<int> *ops = new Operation<int>[TRANSACTION_SIZE];
		// For each operation.
		for (int j = 0; j < TRANSACTION_SIZE; j++)
		{
			// All operations are pushes.
			ops[j].type = Operation<int>::OpType::pushBack;
			ops[j].val =
				threadNum * NUM_TRANSACTIONS * TRANSACTION_SIZE +
				i * TRANSACTION_SIZE +
				j;
		}
		// Create a transaction containing the these operations.
		Desc<int> *desc = new Desc<int>(TRANSACTION_SIZE, ops);
		// Execute the transaction.
		transVector->executeTransaction(desc);
	}
}

void readThread(int threadNum)
{
	// For each transaction.
	for (size_t i = 0; i < NUM_TRANSACTIONS; i++)
	{
		// A list of operations for the current thread.
		Operation<int> *ops = new Operation<int>[TRANSACTION_SIZE];
		// For each operation.
		for (int j = 0; j < TRANSACTION_SIZE; j++)
		{
			// All operations are reads.
			ops[j].type = Operation<int>::OpType::read;
			// DEBUG: This will always be an invalid read. Test invalid writes too.
			ops[j].index =
				threadNum * NUM_TRANSACTIONS * TRANSACTION_SIZE +
				i * TRANSACTION_SIZE +
				j;
		}
		// Create a transaction containing the these operations.
		Desc<int> *desc = new Desc<int>(TRANSACTION_SIZE, ops);
		// Execute the transaction.
		transVector->executeTransaction(desc);
	}
}

void writeThread(int threadNum)
{
	// For each transaction.
	for (size_t i = 0; i < NUM_TRANSACTIONS; i++)
	{
		// A list of operations for the current thread.
		Operation<int> *ops = new Operation<int>[TRANSACTION_SIZE];
		// For each operation.
		for (int j = 0; j < TRANSACTION_SIZE; j++)
		{
			// All operations are writes.
			ops[j].type = Operation<int>::OpType::write;
			ops[j].val = 0;
			ops[j].index =
				threadNum * NUM_TRANSACTIONS * TRANSACTION_SIZE +
				i * TRANSACTION_SIZE +
				j;
		}
		// Create a transaction containing the these operations.
		Desc<int> *desc = new Desc<int>(TRANSACTION_SIZE, ops);
		// Execute the transaction.
		transVector->executeTransaction(desc);
	}
}

void popThread(int threadNum)
{
	// For each transaction.
	for (size_t i = 0; i < NUM_TRANSACTIONS; i++)
	{
		// A list of operations for the current thread.
		Operation<int> *ops = new Operation<int>[TRANSACTION_SIZE];
		// For each operation.
		for (int j = 0; j < TRANSACTION_SIZE; j++)
		{
			// All operations are pushes.
			ops[j].type = Operation<int>::OpType::popBack;
		}
		// Create a transaction containing the these operations.
		Desc<int> *desc = new Desc<int>(TRANSACTION_SIZE, ops);
		// Execute the transaction.
		transVector->executeTransaction(desc);
	}
}

void randThread(int threadNum)
{
	// For each transaction.
	for (size_t i = 0; i < NUM_TRANSACTIONS; i++)
	{
		size_t transSize = (rand() % TRANSACTION_SIZE) + 1;
		// A list of operations for the current thread.
		Operation<int> *ops = new Operation<int>[transSize];
		// For each operation.
		for (int j = 0; j < transSize; j++)
		{
			// All operations are pushes.
			ops[j].type = Operation<int>::OpType(rand() % 6);
			ops[j].index = rand() % 1000;
			ops[j].val = rand() % 1000;
		}
		// Create a transaction containing the these operations.
		Desc<int> *desc = new Desc<int>(transSize, ops);
		// Execute the transaction.
		transVector->executeTransaction(desc);
	}
}

void threadRunner(thread *threads, void function(int threadNum))
{
	// Start our threads.
	for (size_t i = 0; i < THREAD_COUNT; i++)
	{
		threads[i] = thread(function, i);
	}

	// Wait for all threads to complete.
	for (size_t i = 0; i < THREAD_COUNT; i++)
	{
		threads[i].join();
	}
	return;
}

void predicatePreinsert(int threadNum)
{
	// A list of operations for the current thread.
	Operation<int> *insertOps = new Operation<int>[NUM_TRANSACTIONS];
	// For each operation.
	for (int j = 0; j < NUM_TRANSACTIONS; j++)
	{
		// All operations are pushes.
		insertOps[j].type = Operation<int>::OpType::pushBack;
		// Push random values into the vector.
		insertOps[j].val = rand() % INT32_MAX;
	}
	// Create a transaction containing the these operations.
	Desc<int> *insertDesc = new Desc<int>(NUM_TRANSACTIONS, insertOps);
	// Execute the transaction.
	transVector->executeTransaction(insertDesc);
}

void predicateFind(int threadNum)
{
	// Get the transaction associated with the thread.
	Desc<int> *desc = transactions[threadNum];
	// Execute the transaction.
	transVector->executeTransaction(desc);
	// Get the results.
	// Busy wait until they are ready. Should never happen, but we need to be safe.
	while (desc->returnedValues.load() == false)
	{
		printf("Thread %d had to wait on returned values.\n", threadNum);
		continue;
	}
	if (desc->status.load() != Desc<int>::TxStatus::committed)
	{
		printf("Error on thread %d. Transaction failed.\n", threadNum);
	}
	// Check for predicate matches.
	size_t matchCount = 0;
	for (size_t i = 0; i < desc->size; i++)
	{
		// Out simple predicate: the value is even.
		if (desc->ops[i].ret % 2 == 0)
		{
			matchCount++;
		}
	}
	// Add to the global total.
	totalMatches.fetch_add(matchCount);
	return;
}

// Insert random elements into the vector and count the number of elements that satisfy the predicate.
void predicateSearch()
{
	// Ensure we start with no matches.
	totalMatches.store(0);

	// Create our threads.
	thread threads[THREAD_COUNT];

	// Pre-insertion step.
	threadRunner(threads, predicatePreinsert);

	// Prepare read transactions for each thread.
	for (size_t i = 0; i < THREAD_COUNT; i++)
	{
		Operation<int> *ops = new Operation<int>[NUM_TRANSACTIONS];
		// Prepare to read the entire vector.
		for (int j = 0; j < NUM_TRANSACTIONS; j++)
		{
			// Read all elements, split among threads.
			ops[j].type = Operation<int>::OpType::read;
			ops[j].index = i * NUM_TRANSACTIONS + j;
		}
		Desc<int> *desc = new Desc<int>(NUM_TRANSACTIONS, ops);
		transactions.push_back(desc);
	}

	printf("Completed preinsertion!\n\n\n");

	// Get the current time.
	auto start = chrono::system_clock::now();

	// Run the threads.
	threadRunner(threads, predicateFind);

	// Get total execution time.
	auto total = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - start);

	//transVector->printContents();

	cout << "" << THREAD_COUNT << " threads and " << NUM_TRANSACTIONS << " locations per thread" << endl;
	cout << total.count() << " milliseconds" << endl;

	printf("Total: %lu matched out of %lu\n", totalMatches.load(), THREAD_COUNT * NUM_TRANSACTIONS);
}

int main(int argc, char *argv[])
{
	//THREAD_COUNT = atol(argv[1]);
	//NUM_TRANSACTIONS = atol(argv[2]);

	// Seed the random number generator.
	srand(time(NULL));

	transVector = new TransactionalVector<int>();

	predicateSearch();
	return 0;

	// Create our threads.
	thread threads[THREAD_COUNT];

	// Get the current time.
	auto start = chrono::system_clock::now();

	threadRunner(threads, randThread);

	// Get total execution time.
	auto total = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - start);

	transVector->printContents();

	cout << "" << THREAD_COUNT << " threads and " << TRANSACTION_SIZE << " operations per transaction" << endl;
	cout << total.count() << " milliseconds" << endl;

	return 0;
}