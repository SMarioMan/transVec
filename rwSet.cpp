#include "rwSet.hpp"

template <typename T>
RWSet<T>::~RWSet()
{
    operations.clear();
    return;
}

template <typename T>
std::pair<size_t, size_t> RWSet<T>::access(size_t pos)
{
    // All other cases.
    return std::make_pair(pos / Page<T, T, 8>::SEG_SIZE, pos % Page<T, T, 8>::SEG_SIZE);
}

template <typename T>
bool RWSet<T>::createSet(Desc<T> *descriptor, TransactionalVector<T> *vector)
{
    // Go through each operation.
    for (size_t i = 0; i < descriptor->size; i++)
    {
        std::pair<size_t, size_t> indexes;
        RWOperation<T> *op = NULL;
        switch (descriptor->ops[i].type)
        {
        case Operation<T>::OpType::read:
            indexes = access(descriptor->ops[i].index);
            op = &operations[indexes.first][indexes.second];
            // If this location has already been written to, read its value. This is done to handle operations that are totally internal to the transaction.
            if (op->lastWriteOp != NULL)
            {
                descriptor->ops[i].ret = op->lastWriteOp->val;
                // If the value was unset (shouldn't really be possible here), then our transaction fails.
                if (op->lastWriteOp->val == UNSET)
                {
                    descriptor->status.store(Desc<T>::TxStatus::aborted);
                    printf("Aborted!\n");
                    return false;
                }
            }
            // We haven't written here before. Request a read from the shared structure.
            else
            {
                if (op->checkBounds == RWOperation<T>::Assigned::unset)
                {
                    op->checkBounds = RWOperation<T>::Assigned::yes;
                }
                // Add ourselves to the read list.
                op->readList.push_back(&descriptor->ops[i]);
            }
            break;
        case Operation<T>::OpType::write:
            indexes = access(descriptor->ops[i].index);
            op = &operations[indexes.first][indexes.second];
            if (op->checkBounds == RWOperation<T>::Assigned::unset)
            {
                op->checkBounds = RWOperation<T>::Assigned::yes;
            }
            op->lastWriteOp = &descriptor->ops[i];
            break;
        case Operation<T>::OpType::pushBack:
            getSize(descriptor, vector->size);
            // This should never happen, but make sure we don't have an integer overflow.
            if (size == SIZE_MAX)
            {
                descriptor->status.store(Desc<T>::TxStatus::aborted);
                printf("Aborted!\n");
                return false;
            }
            indexes = access(size++);

            op = &operations[indexes.first][indexes.second];
            if (op->checkBounds == RWOperation<T>::Assigned::unset)
            {
                op->checkBounds = RWOperation<T>::Assigned::no;
            }
            op->lastWriteOp = &descriptor->ops[i];
            break;
        case Operation<T>::OpType::popBack:
            getSize(descriptor, vector->size);
            // Prevent popping past the bottom of the stack.
            if (size < 1)
            {
                descriptor->status.store(Desc<T>::TxStatus::aborted);
                printf("Aborted!\n");
                return false;
            }
            indexes = access(--size);

            op = &operations[indexes.first][indexes.second];
            // If this location has already been written to, read its value. This is done to handle operations that are totally internal to the transaction.
            if (op->lastWriteOp != NULL)
            {
                descriptor->ops[i].ret = op->lastWriteOp->val;
            }
            // We haven't written here before. Request a read from the shared structure.
            else
            {
                // Add ourselves to the read list.
                op->readList.push_back(&descriptor->ops[i]);
            }
            // We actually write an unset value here when we pop.
            // Make sure we explicitly mark as UNSET.
            // Don't leave this in the hands of the person creating the transactions.
            descriptor->ops[i].val = UNSET;
            if (op->checkBounds == RWOperation<T>::Assigned::unset)
            {
                op->checkBounds = RWOperation<T>::Assigned::no;
            }
            op->lastWriteOp = &descriptor->ops[i];
            break;
        case Operation<T>::OpType::size:
            getSize(descriptor, vector->size);

            // Note: Don't store in ret. Store in index, as a special case for size calls.
            descriptor->ops[i].index = size;
            break;
        case Operation<T>::OpType::reserve:
            // We only care about the largest reserve call.
            // All other reserve operations will consolidate into a single call at the beginning of the transaction.
            if (descriptor->ops[i].val > maxReserveAbsolute)
            {
                maxReserveAbsolute = descriptor->ops[i].val;
            }
            break;
        default:
            // Unexpected operation type found.
            break;
        }
    }

    // If we accessed size, we need to report what we changed it to.
    if (sizeDesc != NULL)
    {
        *(sizeDesc->at(0, NEW_VAL)) = size;
    }
    return true;
}

template <typename T>
std::map<size_t, Page<T, T, 8> *> RWSet<T>::setToPages(Desc<T> *descriptor)
{
    // All of the pages we want to insert (except size), ordered from low to high.
    std::map<size_t, Page<T, T, 8> *> pages;

    // For each page to generate.
    // These are all independent of shared memory.
    for (auto i = operations.begin(); i != operations.end(); ++i)
    {
        // Determine how many elements are in this page.
        size_t elementCount = i->second.size();
        // Create the initial page.
        Page<T, T, 8> *page = new Page<T, T, 8>(elementCount);
        // Link the page to the transaction descriptor.
        page->transaction = descriptor;

        // For each element to place in the given page.
        for (auto j = i->second.begin(); j != i->second.end(); ++j)
        {
            size_t index = j->first;
            page->bitset.read[index] = (j->second.readList.size() > 0);
            page->bitset.write[index] = (j->second.lastWriteOp != NULL);
            page->bitset.checkBounds[index] = (j->second.checkBounds == RWOperation<T>::Assigned::yes) ? true : false;
            *(page->at(index, NEW_VAL)) = j->second.lastWriteOp->val;
        }
        pages[i->first] = page;
    }

    return pages;
}

template <typename T>
void RWSet<T>::setOperationVals(Desc<T> *descriptor, std::map<size_t, Page<T, T, 8> *> *pages)
{
    // If the returned values have already been copied over, do no more.
    if (descriptor->returnedValues.load() == true)
    {
        return;
    }

    // For each page.
    for (auto i = pages->begin(); i != pages->end(); ++i)
    {
        // Get the page.
        Page<T, T, 8> *page = i->second;
        // For each element.
        for (auto j = 0; j < pages->size(); ++j)
        {
            // Get the value.
            T value = *(page->at(j, OLD_VAL));

            // Get the read list for the current element.
            std::vector<Operation<T> *> readList = operations.at(i->first).at(j).readList;
            // For each operation attempting to read the element.
            for (size_t k = 0; k < readList.size(); k++)
            {
                // Assign the return value.
                readList[k]->ret = value;
            }
        }
    }

    // Subsequent attempts to load values will not have to repeat this work.
    descriptor->returnedValues.store(true);

    return;
}

// TODO: Implement size functions here instead of transVector.

template <typename T>
size_t RWSet<T>::getSize(Desc<T> *descriptor, std::atomic<Page<size_t, T, 1> *> *sizeHead)
{
    if (sizeDesc != NULL)
    {
        return size;
    }

    // Prepend a read page to size.
    // The size page is always of size 1.
    // Set all unchanging page values here.
    sizeDesc = new Page<size_t, T, 1>(1);
    sizeDesc->bitset.read.set();
    sizeDesc->bitset.write.set();
    sizeDesc->bitset.checkBounds.reset();
    sizeDesc->transaction = descriptor;
    sizeDesc->next = NULL;

    Page<size_t, T, 1> *rootPage;
    do
    {
        // Quit if the transaction is no longer active.
        if (descriptor->status.load() != Desc<T>::TxStatus::active)
        {
            return 0;
        }

        // Get the current head.
        rootPage = sizeHead->load();
        // TODO: Something is wrong here. Find out what.
        // If the root page does not exist, or does not contain a value in the expected position.
        if (rootPage == NULL || rootPage->at(0, NEW_VAL) == NULL)
        {
            // Assume an initial size of 0.
            *(sizeDesc->at(0, OLD_VAL)) = 0;
        }
        else
        {
            // Store the root page's value as an old value in case we abort.
            *sizeDesc->at(0, OLD_VAL) = *rootPage->at(0, NEW_VAL);

            if (rootPage->transaction->status.load() == Desc<T>::TxStatus::active)
            {
                // TODO: Use help scheme here.
                // Just busy wait for now.
                while (rootPage->transaction->status.load() == Desc<T>::TxStatus::active)
                {
                    continue;
                }
            }
        }
    }
    // Replace the page.
    // Finish on success.
    // Retry on failure.
    while (!sizeHead->compare_exchange_strong(rootPage, sizeDesc));

    // No need to use a linked-list for size. Just deallocate the old page.
    delete rootPage;

    // Store the actual size.
    size = *(sizeDesc->at(0, OLD_VAL));

    return size;
}

template class RWSet<int>;