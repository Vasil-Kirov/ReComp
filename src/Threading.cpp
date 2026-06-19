#include "Threading.h"
#include "Memory.h"

bool TryDoWork(work_queue *Queue)
{
	for(;;)
	{
		auto OriginalAtJob = Queue->AtJob.load();
		if(Queue->JobCount <= OriginalAtJob)
			return false;

		if(Queue->AtJob.compare_exchange_strong(OriginalAtJob, OriginalAtJob+1))
		{
			u32 JobIndex = OriginalAtJob;
			job Job = Queue->Jobs[JobIndex % MAX_JOBS];
			Job.Task(Job.Data);
			++Queue->JobsCompleted;
			if(Queue->JobsCompleted == Queue->JobCount)
				Queue->WakeMain.notify_all();
		}
		else
			continue;

		return true;
	}
}

void MainThreadWorkUntilDone(work_queue *Queue)
{
	while(!IsQueueDone(Queue))
	{
		if (TryDoWork(Queue))
			continue;

		std::unique_lock<std::mutex> Lock {Queue->MainMutex};
		Queue->WakeMain.wait(Lock, [&] {
			if(Queue->AtJob < Queue->JobCount)
				return true;

			if(IsQueueDone(Queue))
				return true;

			return false;
		});
	}
}

bool IsQueueDone(work_queue *Queue)
{
	return Queue->JobsCompleted == Queue->JobCount;
}

struct thread_proc_data
{
	work_queue *Queue;
	size_t ID;
};

unsigned long ThreadProc(void *Data)
{
	auto tpd = (thread_proc_data *)Data;
	work_queue *Queue = tpd->Queue;
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
	Queue->WakeMain.notify_all();
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
bool NoThreads = false;

void InitThreadsForQueue(work_queue *Queue)
{
	if(NoThreads)
		return;

	RW_BARRIER;
	for(int i = 0; i < MAX_THREADS; ++i)
	{
		thread_proc_data *Data = NewType(thread_proc_data);
		Data->Queue = Queue;
		Data->ID = i+1;
		Queue->Threads[i] = PlatformCreateThread(ThreadProc, Data);
	}
}


