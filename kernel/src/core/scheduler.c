// This file is part of the Sphynx OS
// It is released under the MIT license -- see LICENSE
// Written by: Kevin Alavik.

#include <core/scheduler.h>
#include <lib/posix/string.h>
#include <lib/posix/stdio.h>
#include <core/proc/elf.h>
#include <sys/boot.h>
#include <dev/vfs.h>
#include <mm/pmm.h>

#define MAX_TASKS 4096

u64 taskId = 0;
Task_t *currentTask;
Task_t *taskList[MAX_TASKS];
u32 taskCount = 0;
u32 currentTaskIndex = 0;

void RemoveTask(u32 index)
{
	if (index >= taskCount) {
		return;
	}

	VmmFree(taskList[index]->pm, (void *)taskList[index]->ctx.rsp);
	VmmFree(taskList[index]->pm, (void *)taskList[index]);

	for (u32 i = index; i < taskCount - 1; ++i) {
		taskList[i] = taskList[i + 1];
	}
	taskCount--;
}

void TaskExit(Task_t *task, u64 exitCode)
{
	task->hasExited = true;
	task->exitCode = exitCode;
}

void WatchdogMain()
{
	Task_t *task = currentTask;
	VmmSwitchPageMap(task->pm);
	task->taskFunction();
	while (1) {
	}
}

void WatchdogHandler()
{
	while (1) {
		if (taskCount == 1 && taskList[0] == currentTask) {
			TaskExit(currentTask, 0);
			return;
		}

		for (u32 i = 0; i < taskCount; i++) {
			Task_t *task = taskList[i];
			if (task->hasExited) {
				printf("Task %llu exited with code %llu\n", task->id,
					   task->exitCode);
				RemoveTask(i);
				i--;
			}
		}
		asm volatile("pause");
	}
}

void SchedulerInitialize()
{
	taskCount = 0;
	currentTaskIndex = 0;
	SchedulerSpawn(WatchdogHandler);
}

void SchedulerSpawn(TaskFunction_t function)
{
	if (taskCount >= MAX_TASKS) {
		return;
	}

	Task_t *task = (Task_t *)PHYS_TO_VIRT(PmmRequestPages(1));
	task->id = taskId++;
	task->pm = VmmNewPageMap();
	task->ctx.rip = (u64)WatchdogMain;
	task->ctx.rsp = (u64)PHYS_TO_VIRT(PmmRequestPages(1)) + 4095;
	task->ctx.cs = 0x08;
	task->ctx.ss = 0x10;
	task->ctx.rflags = 0x202;
	task->taskFunction = function;

	taskList[taskCount++] = task;
}

void SchedulerTick(Context_t *ctx)
{
	if (taskCount == 0) {
		return;
	}

	Task_t *task = taskList[currentTaskIndex];
	if (task == NULL) {
		return;
	}

	if (currentTask != NULL) {
		memcpy(&currentTask->ctx, ctx, sizeof(Context_t));
	}

	currentTask = task;

	memcpy(ctx, &currentTask->ctx, sizeof(Context_t));
	VmmSwitchPageMap(currentTask->pm);

	currentTaskIndex = (currentTaskIndex + 1) % taskCount;
}

Task_t *SchedulerGetCurrentTask()
{
	return currentTask;
}

void SchedulerSpawnElf(const char *path)
{
	if (taskCount >= MAX_TASKS) {
		printf("ERROR: Maximum task limit reached!\n");
		return;
	}

	u8 *bin = (u8 *)VmmAlloc(VmmGetKernelPageMap(), 4,
							 1 | 2); // ~16KB should be enough for now
	if (bin == NULL) {
		printf("ERROR: Failed to allocate memory for ELF data!\n");
		return;
	}

	u64 bytesRead = VfsRead(path, bin);
	if (bytesRead > 0) {
		Task_t *task = (Task_t *)PHYS_TO_VIRT(PmmRequestPages(1));
		if (task == NULL) {
			printf("ERROR: Failed to allocate memory for task!\n");
			VmmFree(VmmGetKernelPageMap(), bin);
			return;
		}

		task->id = taskId++;
		task->pm = VmmNewPageMap();
		if (task->pm == NULL) {
			printf("ERROR: Failed to create seperate pagemap for task %d!\n",
				   task->id);
			VmmFree(VmmGetKernelPageMap(), bin);
			PmmFreePages((void *)VIRT_TO_PHYS((u64)task), 1);
			return;
		}

		for (u32 i = 0; i < taskCount; i++) {
			if (taskList[i]->pm == task->pm) {
				printf("ERROR: Task %d's pagemap collides with tasks %d\n",
					   task->id, i);

				VmmFree(VmmGetKernelPageMap(), bin);
				PmmFreePages((void *)VIRT_TO_PHYS((u64)task), 1);
				return;
			}
		}

		task->ctx.rip = (u64)WatchdogMain;
		task->ctx.rsp = (u64)PHYS_TO_VIRT(PmmRequestPages(1)) + 4095;
		task->ctx.cs = 0x08;
		task->ctx.ss = 0x10;
		task->ctx.rflags = 0x202;

		task->taskFunction = (TaskFunction_t)SpawnElf(bin, task->pm);

		taskList[taskCount++] = task;
	} else {
		printf("ERROR: Failed to read ELF at %s\n", path);
	}

	VmmFree(VmmGetKernelPageMap(), bin);
}