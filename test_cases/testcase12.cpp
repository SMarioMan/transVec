// TESTCASE 12
// SLOW-FAST
// MIX: 66-33

#include "main.hpp"

void createTransactions()
{
	for (size_t j = 0; j < NUM_TRANSACTIONS; j++)
	{
		Operation *ops = new Operation[TRANSACTION_SIZE];

		for (size_t k = 0; k < TRANSACTION_SIZE; k++)
		{
			// 66 slow, 33 fast
			if (rand() % 3 != 0)
			{
				int r = rand();

				// We'll get the 33-33-33 ratio by checking for mod 3
				if (r % 3 == 0)
				{
					// All operations are pushes.
					ops[k].type = Operation::OpType::pushBack;

					// Push random values into the vector.
					ops[k].val = rand();
				}
				else if (r % 3 == 1)
				{
					ops[k].type = Operation::OpType::popBack;
				}
				else
				{
					ops[k].type = Operation::OpType::size;
				}
			}
			else
			{
				// We'll get the 50/50 ratio by checking for even or odd
				// If even, make a write operation, else make a read operation
				if (rand() % 2 == 0)
				{
					// All operations are writes.
					ops[k].type  = Operation::OpType::write;
					ops[k].val   = rand() % std::numeric_limits<VAL>::max();
					ops[k].index = rand() % NUM_TRANSACTIONS;
				}
				else
				{
					// Read all elements, split among threads.
					ops[k].type = Operation::OpType::read;
					ops[k].index = rand() % NUM_TRANSACTIONS;
				}
			}
		}

		Desc *desc = new Desc(TRANSACTION_SIZE, ops);
		transactions->push_back(desc);
	}
}

int main(void)
{
	// Seed the random number generator.
	srand(time(NULL));

	// Ensure the test process runs at maximum priority.
	// Only works if run under sudo permissions.
	setMaxPriority();

	// Pre-fill the allocators.
	allocatorInit();

	// Reserve the transaction vector, for minor performance gains.
	transactions->reserve(THREAD_COUNT);

	// Create our threads.
	std::thread threads[THREAD_COUNT];

		// Pre-insertion step.
	//threadRunner(threads, preinsert);
	// Single-threaded alternative.
	for (size_t i = 0; i < THREAD_COUNT; i++)
	{
		preinsert(i);
	}

	// Create the transactions that are to be executed and timed below
	createTransactions();

	// Get start time.
	auto start = std::chrono::high_resolution_clock::now();

	// Execute the transactions
	threadRunner(threads, executeTransactions);

	// Get end time and count abort(s)
	auto finish = std::chrono::high_resolution_clock::now();
	std::cout << SGMT_SIZE << "\t" << NUM_TRANSACTIONS << "\t";
	std::cout << TRANSACTION_SIZE << "\t" << THREAD_COUNT << "\t";
    std::cout << std::chrono::duration_cast<std::chrono::TIME_UNIT>(finish-start).count();
#ifdef METRICS
	std::cout << "Average preprocessing time" + std::chrono::duration_cast<std::chrono::TIME_UNIT>(measurePreprocessTime(transactions)).count();
	std::cout << "Average shared memory time" + std::chrono::duration_cast<std::chrono::TIME_UNIT>(measureSharedTime(transactions)).count();
	std::cout << "Average transaction time" + std::chrono::duration_cast<std::chrono::TIME_UNIT>(measureTotalTime(transactions)).count();
#endif
	std::cout << "\t" << countAborts(transactions) << "\n";

	// Report on allocator issues.
	allocatorReport();

	return 0;
}