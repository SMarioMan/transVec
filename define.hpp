#ifndef DEFINE_HPP
#define DEFINE_HPP

// These are used to switch between different vector implementations.
// Only uncomment one of them at a time. **Make sure they're all commented if using the test-all.sh script**
// **It's important to keep these comments as "//#define" instead of "// define"**
//#define SEGMENTVEC
//#define COMPACTVEC
//#define BOOSTEDVEC
//#define COARSEVEC
//#define STMVEC
//#define STOVEC

// Change these to test different situations.
// Makes sense to make this cache line size X associativity (perhaps at L2 level, so 8*16)
// Divide by the size of the elements in the segment an by 2, so we can hold old and new values on the same cache line.
// NOTE: This may break if division makes this resolve to 0.
#define SGMT_SIZE ((8 * 16) / (sizeof(VAL) * 2))
#define NUM_TRANSACTIONS 10000
//#define THREAD_COUNT 2
//#define TRANSACTION_SIZE 5
// Define this to enable the helping scheme.
#define HELP
// Define this to debug allocation counting.
//#define ALLOC_COUNT
// Define this to optimize traversal order.
#define HIGHTOLOW
// Define this to capture performance metrics (average transaction times)
#define METRICS

#ifdef SEGMENTVEC
// Define this to align SEGMENTVEC pages.
//#define ALIGNED
// Enable conflict-free reads and their associated (essentially non-existant) overhead.
#define CONFLICT_FREE_READS
#endif

// Compact vector requires 32-bit or smaller value types.
#ifdef COMPACTVEC
// Use this typedef to quickly change what type of objects we're working with.
// NOTE: VAL is unsigned to keep things simple between size and object elements.
typedef unsigned int VAL;
// This reserved value indicates that a value cannot be set by a read or write here.
// Must differ depending on T.
const VAL UNSET = UINT32_MAX;
#else
// Use this typedef to quickly change what type of objects we're working with.
typedef unsigned int VAL;
// This reserved value indicates that a value cannot be set by a read or write here.
// Must differ depending on T.
const VAL UNSET = UINT32_MAX;
#endif

// Define the preferred order to perform shared memory modifications.
// Greater: Low to high index.
// Less: High to low index.
#ifdef HIGHTOLOW
// Optimized order
#define ORDER std::less<size_t>
#else
// Non-optimized order
#define ORDER std::greater<size_t>
#endif

#endif
