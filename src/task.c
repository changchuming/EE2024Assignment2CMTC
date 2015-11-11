/*****************************************************************************
 * Task functions
 *
 * Author: Chang Chu-Ming
 *         Terry Chua
 *
 * Date: 25/10/2015
 *
 ******************************************************************************/
#include "task.h"
#include <math.h>
#include <stdio.h>

// ########################################################################################
// Returns a new task as a pointer
// ########################################################################################
Task *newTask(void (*givenTask)(), int interval, int repeatCount, int tickIntervalConstant) {
	Task *task;
	if((task = malloc(sizeof *task)) != NULL)
	  {
		task->task = givenTask;
		task->interval = interval;
		task->repeatCount = repeatCount;
		task->runCount = 0;
		task->ticksBeforeRun = ceil(task->interval/tickIntervalConstant);
		task->tickCount = 0;
	  }
	  return task;
}

// ########################################################################################
// Runs a given task once
// ########################################################################################
void runTaskOnce(Task *task) {
	if (task->runCount<task->repeatCount || task->repeatCount==-1) {
//		printf("run: %i %i \n", task->runCount, task->repeatCount);
		task->runCount++;
		task->task();
	}
}

// ########################################################################################
// Check all tasks in task list and run if necessary
// ########################################################################################
void checkAndRunTasks(Task *taskList[10], int *taskCount) {
	int taskNum;
	for (taskNum=0;taskNum<*taskCount;taskNum++) {
		taskList[taskNum]->tickCount++;
		if (taskList[taskNum]->tickCount >= taskList[taskNum]->ticksBeforeRun) {
			taskList[taskNum]->tickCount = 0;
			runTaskOnce(taskList[taskNum]);
		}
	}
}

// ########################################################################################
// Add task to task list
// ########################################################################################
void addTask(Task *taskList[10], int *taskCount, Task *task) {
	taskList[*taskCount] = task;
//	printf("t1: %d\n", *taskCount);
	*taskCount = *taskCount+1;
//	printf("t2: %d\n", *taskCount);
}

// ########################################################################################
// Remove task from task list
// ########################################################################################
void removeFinishedTasks(Task *taskList[10], int *taskCount) {
	int taskNum = 0;
////	printf("At start %d %d\n", taskNum, *taskCount);
////	printf("%d %d\n", taskNum, *taskCount);
//	while (taskNum<*taskCount) {
		for (taskNum=0;taskNum<*taskCount;taskNum++) {
			if (taskList[taskNum]->runCount >= taskList[taskNum]->repeatCount && taskList[taskNum]->repeatCount!=-1) {
//				printf("remove: %d %i %i \n", taskNum, taskList[taskNum]->runCount, taskList[taskNum]->repeatCount);
				int oldTaskNum;
				for (oldTaskNum=taskNum;oldTaskNum<*taskCount-1;oldTaskNum++) {
					taskList[oldTaskNum] = taskList[oldTaskNum+1];
				}
//				taskList[oldTaskNum] = '\0';
				*taskCount = *taskCount-1;
//				printf("After remove %d%d %d %d\n", taskList[0]->runCount, taskList[0]->repeatCount, taskNum, *taskCount);
				taskNum = 0;
				break;
			}
		}
//	}
}
