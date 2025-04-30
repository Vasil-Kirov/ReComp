#include "Threading.h"
#include "Memory.h"

bool TryDoWork(work_queue *Queue)
{
	auto OriginalAtJob = Queue->AtJob.load();
	if(Queue->JobCount.load() > OriginalAtJob)
	{
		if(Queue->AtJob.compare_exchange_strong(OriginalAtJob, OriginalAtJob+1))
		{
			u32 JobIndex = OriginalAtJob;
			job Job = Queue->Jobs[JobIndex % MAX_JOBS];
			Job.Task(Job.Data);
			++Queue->JobsCompleted;
		}
		return true;
	}
	return false;
}

bool IsQueueDone(work_queue *Queue)
{
	return Queue->JobsCompleted == Queue->JobCount;
}

unsigned long ThreadProc(void *Queue_)
{
	work_queue *Queue = (work_queue *)Queue_;
	for(;;)
	{
		if(!TryDoWork(Queue))
		{
			PlatformSleepOnSemaphore(Queue->Semaphore);
		}
	}
	
	return 0;
}

void PostJob(work_queue *Queue, job Job)
{
	Queue->Mutex.lock();

	Queue->Jobs[Queue->JobCount % MAX_JOBS] = Job;
	++Queue->JobCount;
	if(Queue->Semaphore)
		PlatformSignalSemaphore(Queue->Semaphore);

	Queue->Mutex.unlock();
}

work_queue *CreateWorkQueue()
{
	work_queue *Queue = NewType(work_queue);
	Queue->AtJob = 0;
	Queue->JobCount = 0;
	Queue->JobsCompleted = 0;
	Queue->Semaphore = PlatformCreateSemaphore(MAX_THREADS);
	if(MAX_THREADS != 0)
		Assert(Queue->Semaphore);
	return Queue;
}

// @FIXME: Threading not working
bool NoThreads = /*false*/true;

void InitThreadsForQueue(work_queue *Queue)
{
	if(NoThreads)
		return;

	RW_BARRIER;
	for(int i = 0; i < MAX_THREADS; ++i)
		Queue->Threads[i] = PlatformCreateThread(ThreadProc, Queue);
}


