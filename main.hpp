#ifndef TRANSVEC_H
#define TRANSVEC_H

#include <chrono>
#include <iostream>
#include <thread>
#include "deltaPage.hpp"
#include "transaction.hpp"
#include "transVector.hpp"

using namespace std;

TransactionalVector<int> *transVector;

void runThread(int threadNum);

int main(int argc, char *argv[]);

// Change these to test different situations.
// TUNE
const size_t THREAD_COUNT = 1;
// TUNE
const size_t TRANSACTION_SIZE = 5;
// TUNE
const size_t NUM_TRANSACTIONS = 5;

#endif