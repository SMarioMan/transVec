#include "segmentedVector.hpp"

template <typename T>
size_t SegmentedVector<T>::highestBit(unsigned int val)
{
	// Subtract 1 so the rightmost position is 0 instead of 1.
	return (sizeof(val) * 8) - __builtin_clz(val | 1) - 1;

	// Slower alternative approach.
	size_t onePos = 0;
	for (size_t i = 0; i <= 8 * sizeof(size_t); i++)
	{
		// Special case for zero.
		if (i == 8 * sizeof(size_t))
		{
			return 0;
		}

		size_t mask = (size_t)1 << (max - i);
		if ((val & mask) != 0)
		{
			onePos = max - i;
			break;
		}
	}
	return onePos;
}

template <typename T>
size_t SegmentedVector<T>::getBucket(size_t hiBit)
{
	// Subtract the first bucket's highest bit to offset for initial bucket size.
	return hiBit - highestBit(firstBucketSize);
}

template <typename T>
size_t SegmentedVector<T>::getIdx(size_t pos, size_t hiBit)
{
	// Removes the leftmost 1 from our position.
	// The remaining value is the index.
	return (pos) ^ (1 << (hiBit));
}

template <typename T>
bool SegmentedVector<T>::allocBucket(size_t bucket)
{
	// Cannot allocate buckets beyond the number we allocated at the beginning.
	if (bucket > buckets)
	{
		return false;
	}
	// If the bucket is already allocated, no work needs to be done here.
	if (bucketArray[bucket].load() != NULL)
	{
		return true;
	}
	// The size of the bucket we are allocating.
	// firstBucketSize^(bucket+1)
	size_t bucketSize = (size_t)1 << (highestBit(firstBucketSize) * (bucket + 1));
	// Allocate the new segment.
#ifndef BOOSTEDVEC
	std::atomic<T> *mem = new std::atomic<T>[bucketSize]();
#else
	T *mem = new T[bucketSize]();
#endif

#ifndef BOOSTEDVEC
	// We need to initialize the NULL pointer if we want to CAS.
	std::atomic<T> *null = NULL;
	// Attempt to place the segment in shared memory.
	if (!bucketArray[bucket].compare_exchange_strong(null, mem))
	{
		// Another thread already allocated this segment.
		// Deallocate the memory allocated by this thread.
		delete[] mem;
	}
#else
	bucketArray[bucket] = mem;
#endif
	return true;
}

template <typename T>
std::pair<size_t, size_t> SegmentedVector<T>::access(size_t index)
{
	// Determine the first and second dimensions that the element is contained in.
	// Add firstBucketSize to offset our results properly.
	size_t pos = index + firstBucketSize;
	// The highest bit is used to determine the bucket and index of our element.
	size_t hiBit = highestBit(pos);
	// Get the bucket asociated with this element.
	size_t bucket = getBucket(hiBit);
	// Get the index associated with this element.
	size_t idx = getIdx(pos, hiBit);
	// Ensure that the first dimension has allocated an array in the second dimension.
	// We don't call this, since we guarantee that all transactions call reserve before running the rest of their operations.
	//reserve(index);
	// Return the array indexes.
	return std::make_pair(bucket, idx);
}

template <typename T>
SegmentedVector<T>::SegmentedVector()
{
	// Initialize the first level of the array.
#ifndef BOOSTEDVEC
	bucketArray = new std::atomic<std::atomic<T> *>[buckets]();
#else
	bucketArray = new std::atomic<T *>[buckets]();
#endif
	// Initialize the first segment in the second level.
	reserve(1);
#ifndef BOOSTEDVEC
	// Ensure that the objects in this vector are actually lock-free.
	//assert(bucketArray[0].load()[0].is_lock_free());
#endif
	return;
}

template <typename T>
SegmentedVector<T>::SegmentedVector(size_t capacity)
{
	// The first bucket size should be the smallest power of 2 that can hold capacity elements.
	firstBucketSize = 1 << (highestBit(capacity) + 1);
	SegmentedVector();
	return;
}

template <typename T>
bool SegmentedVector<T>::reserve(size_t size)
{
	// Determine what bucket needs to be allocated to fit an element in this position.
	size_t targetBucket = getBucket(highestBit(size + firstBucketSize));
	// Check to see if the highest bucket is already allocated.
	if (bucketArray[targetBucket].load() != NULL)
	{
		// If so, we do nothing.
		return true;
	}
	// Otherwise, we need to allocate it, and all previous buckets if they aren't already.
	// Allocate from low to high indexes.
	// This allows us to assume all previous buckets have been allocated by checking only if the highest has been allocated.
	for (size_t i = 0; i <= targetBucket; i++)
	{
		if (!allocBucket(i))
		{
			// If bucket allocation failed.
			return false;
		}
	}

	return true;
}

template <typename T>
// Typically fails if an invalid read or write occurs.
#ifndef BOOSTEDVEC
bool SegmentedVector<T>::read(size_t index, T &value)
#else
bool SegmentedVector<T>::read(size_t index, T *&value)
#endif
{
	std::pair<size_t, size_t> indexes = access(index);
	// Requested a bucket out of bounds.
	if (indexes.first > buckets)
	{
		return false;
	}
#ifndef BOOSTEDVEC
	std::atomic<T> *bucket = bucketArray[indexes.first].load();
#else
	T *bucket = bucketArray[indexes.first].load();
#endif
	// Loaded a bucket that isn't allocated.
	if (bucket == NULL)
	{
		return false;
	}
	// Requested an index out of range of the current bucket.
	if (indexes.second > (size_t)(1 << (highestBit(firstBucketSize) * (indexes.first + 1))))
	{
		return false;
	}
#ifndef BOOSTEDVEC
	value = bucket[indexes.second].load();
#else
	value = &bucket[indexes.second];
#endif
	return true;
}

#ifndef BOOSTEDVEC
template <typename T>
bool SegmentedVector<T>::write(size_t index, T val)
{
	std::pair<size_t, size_t> indexes = access(index);
	// Requested a bucket out of bounds.
	if (indexes.first > buckets)
	{
		return false;
	}
	std::atomic<T> *bucket = bucketArray[indexes.first].load();
	// Loaded a bucket that isn't allocated.
	if (bucket == NULL)
	{
		return false;
	}
	// Requested an index out of range of the current bucket.
	if (indexes.second > (size_t)(1 << (highestBit(firstBucketSize) * (indexes.first + 1))))
	{
		return false;
	}
	bucket[indexes.second].store(val);
	return true;
}

template <typename T>
bool SegmentedVector<T>::tryWrite(size_t index, T oldVal, T newVal)
{
	std::pair<size_t, size_t> indexes = access(index);
	// Requested a bucket out of bounds.
	if (indexes.first > buckets)
	{
		return false;
	}
	std::atomic<T> *bucket = bucketArray[indexes.first].load();
	// Loaded a bucket that isn't allocated.
	if (bucket == NULL)
	{
		return false;
	}
	// Requested an index out of range of the current bucket.
	if (indexes.second > (size_t)(1 << (highestBit(firstBucketSize) * (indexes.first + 1))))
	{
		return false;
	}
	return bucket[indexes.second].compare_exchange_strong(oldVal, newVal);
}
#endif

template <typename T>
void SegmentedVector<T>::printBuckets()
{
	// Go through each bucket.
	for (size_t i = 0; i < buckets; i++)
	{
		if (bucketArray[i].load() == NULL)
		{
			break;
		}
		// Go through each element in the bucket.
		size_t bucketSize = 1 << (highestBit(firstBucketSize) * (i + 1));
		for (size_t j = 0; j < bucketSize; j++)
		{
#ifdef SEGMENTVEC
			printf("%p\n", bucketArray[i].load()[j].load());
#endif
#ifdef COMPACTVEC
			bucketArray[i].load()[j].load().print();
#endif
		}
	}
	printf("\n");
	return;
}

#ifdef SEGMENTVEC
template class SegmentedVector<Page<VAL, SGMT_SIZE> *>;
#endif
#ifdef COMPACTVEC
template class SegmentedVector<CompactElement>;
#endif
#ifdef BOOSTEDVEC
template class SegmentedVector<BoostedElement>;
#endif