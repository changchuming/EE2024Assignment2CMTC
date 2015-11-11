/*****************************************************************************
 * Task header file
 *
 * Author: Chang Chu-Ming
 *         Terry Chua
 *
 * Date: 25/10/2015
 *
 ******************************************************************************/
typedef struct Task
{
	// Parameter
	void (*task)();
	int interval;
	int repeatCount; // Set to -1 for infinite repeats, 0 for zero repeats

	// To be initialized
	int runCount;
	int ticksBeforeRun;
	int tickCount;
} Task;

Task *newTask(void (*givenTask)(), int interval, int repeatCount, int tickIntervalConstant);

void runTaskOnce(Task *task);

void checkAndRunTasks(Task *taskList[10], int *taskCount);

void addTask(Task *taskList2[10], int *taskCount2, Task *task);

void removeFinishedTasks(Task *taskList[10], int *taskCount);
