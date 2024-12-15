#pragma once
#include "Platform.h"
#include <atomic>
#include <mutex>

#define READ_BARRIER atomic_thread_fence(std::memory_order_acquire)
#define WRITE_BARRIER atomic_thread_fence(std::memory_order_release)
#define RW_BARRIER atomic_thread_fence(std::memory_order_seq_cst)
const int MAX_THREADS = 12;
const int MAX_JOBS = 128;

struct job
{
	void (*Task)(void *);
	void *Data;
};

struct work_queue
{
	job Jobs[MAX_JOBS];
	t_handle Threads[MAX_THREADS];
	t_semaphore Semaphore;
	std::atomic<u32> JobCount;
	std::atomic<u32> AtJob;
	std::atomic<u32> JobsCompleted;
	std::mutex Mutex;
};

// Global mutex
void LockMutex();
void UnlockMutex();

work_queue *CreateWorkQueue();
void InitWorkQueue(work_queue *Queue);
void PostJob(work_queue *Queue, job Job);
unsigned long ThreadProc(void *_Queue);
bool IsQueueDone(work_queue *Queue);
bool TryDoWork(work_queue *Queue);

