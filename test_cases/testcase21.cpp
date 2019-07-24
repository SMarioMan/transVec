// TESTCASE 21
// RANDOM HFREADS
// Mix: NA
// Creats a bunch of only read operations at random places in the vector

#include "main.hpp"

void createTransactions()
{
	for (size_t j = 0; j < NUM_TRANSACTIONS; j++)
	{
		Operation *ops = new Operation[TRANSACTION_SIZE];
		
		// Each transaction will be of this size and only made of reads
		for (size_t k = 0; k < TRANSACTION_SIZE; k++)
		{
			// Read at random indices.
			ops[k].type  = Operation::OpType::hfRead;
			ops[k].index = rand() % NUM_TRANSACTIONS;
		}

		Desc *desc = new Desc(TRANSACTION_SIZE, ops);
		transactions->push_back(desc);
	}
}

int main(void)
{
	// Seed the random number generator.
	srand(time(NULL));

	// Pre-fill the allocators.
	allocatorInit();

	// Reserve the transaction vector, for minor performance gains.
	transactions->reserve(THREAD_COUNT);

	// Create our threads.
	std::thread threads[THREAD_COUNT];

	// Pre-insertion step.
	threadRunner(threads, preinsert);

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
	std::cout << "\t" << countAborts(transactions) << "\n";

	// Report on allocator issues.
	allocatorReport();

	return 0;
}