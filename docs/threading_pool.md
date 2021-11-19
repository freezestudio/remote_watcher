# 线程池

线程池 是工作线程的集合, 可用于代表应用程序有效地执行异步回调. 线程池主要用于减少应用程序线程数, 并提供工作线程的管理. 应用程序可以对工作项进行排队、将工作与可等待句柄关联、自动根据计时器进行排队以及与 i/o 绑定.

线程池体系结构由以下内容组成:

* 执行回调函数的工作线程
* 等待多个等待句柄的等待线程数
* 工作队列
* 每个进程的默认线程池
* 管理工作线程的工作线程工厂

线程池最佳做法：

* 进程的线程共享线程池. 单个工作线程可以一次执行多个回调函数. 这些工作线程由线程池管理.
因此, 请不要通过调用线程上的 `TerminateThread` 或从回调函数调用 `ExitThread` 来终止线程池中的线程.

* `I/O` 请求可以在线程池中的任何线程上运行.
取消线程池线程上的 `I/O` 需要同步, 因为 `cancel` 函数可能在处理 `I/O` 请求的线程之外的其他线程上运行, 这可能会导致取消未知操作.
为避免出现这种情况, 请始终提供在调用异步 `I/O` 的 `CancelIoEx` 时启动 `I/O` 请求的重叠结构, 或使用自己的同步, 
以确保在调用 `CancelSynchronousIo` 或 `CancelIoEx` 函数之前, 不能在目标线程上启动任何其他 `I/O`.

* 在从函数返回之前, 清理在回调函数中创建的所有资源. 其中包括 `TLS`、安全上下文、线程优先级和 `COM` 注册. 回调函数还必须还原线程状态, 然后再返回.

* 使等待句柄及其关联的对象保持活动状态, 直到线程池终止了句柄的处理.

* 标记等待冗长操作的所有线程 (例如, `I/O` 刷新或资源清理) 以便线程池可以分配新线程而不是等待此线程.

* 在卸载使用线程池的 `DLL` 之前, 请取消所有工作项、`I/O`、等待操作和计时器, 并等待执行回调完成.

* 通过确保回调不等待自身完成, 并保留线程优先级来避免死锁, 从而消除工作项之间和回调之间的依赖关系.

* 在使用默认线程池的其他组件的进程中, 不要将太多项目排队太多. 每个进程都有一个默认的线程池, 其中包括 `Svchost.exe`. 
默认情况下, 每个线程池最多具有 `500` 个工作线程. 当就绪/运行状态中的工作线程数必须小于处理器数时, 线程池会尝试创建更多工作线程.

* 避免 `COM` 单线程单元模型, 因为它与线程池不兼容. `STA` 会创建线程状态, 这可能会影响线程的下一个工作项. `STA` 通常是长时间生存期, 并且具有线程关联, 这与线程池相反.

* 创建新的线程池来控制线程优先级和隔离、创建自定义特征, 并可能提高响应能力. 但是, 其他线程池需要更多的系统资源 (线程、内核内存) . 池过多会增加 `CPU` 争用的可能性.

* 如果可能, 请使用可等待对象而不是基于 `APC` 的机制来向线程池线程发出信号. `APC` 并不像其他信号机制一样使用线程池线程, 因为系统控制线程池线程的生存期, 因此在传递通知之前, 线程可能会终止.

## 新旧线程池 `API`

功能|旧 API|新 API
-|-|-
等待|等待重置是自动的|每次都必须显式重置等待
安全上下文|自动处理模拟, 将调用进程的安全上下文传输到该线程|应用程序必须显式设置安全上下文

功能|旧 API|新 API
-|-|-
Synch|`RegisterWaitForSingleObject`<br>`UnretisterWaitEx`|`CreateThreadpoolWait`<br>`CloseThreadpoolWait`<br>`SetThreadpoolWait`<br>`WaitForThreadpoolWaitCallbacks`
Work|`QueueUserWorkItem`|`CreateThreadpoolWork`<br>`CloseThreadpoolWork`<br>`SubmitThreadpoolWork`<br>`TrySubmitThreadpoolCallback`<br>`WaitForThreadpoolIWorkCallbacks`<br>
Timer|`CreateTimerQueue`<br>`CreateTimerQueueTimer`<br>`ChangeTimerQueueTimer`<br>`DeleteTimerQueueTime`<br>`DeleteTimerQueueEx`<br>|`CreateThreadpoolTimer`<br>`CloseThreadpoolTimer`<br>`IsThreadpoolTimerSet`<br>`SetTheadpoolTimer`<br>`WaitForThreadpoolTimerCallbacks`<br>
I/O|`BindIoCompletionCallback`|`CreateThreadpoolIo`<br>`CloseThreadpoolIo`<br>`StartThreadpoolIo`<br>`CancelTreadpoolIo`<br>`WaitForThreadpoolIoCallbacks`<br>
Clean-up group||`CreateThreadpoolCleanupGroup`<br>`CloseThreadpoolCleanupGroup`<br>`CloseThreadpoolCleanupGroupMembers`<br>
Pool||`CreateThreadpool`<br>`CloseThreadpool`<br>`SetThreadpoolThreadMaximum`<br>`SetThreadpoolThreadMinimum`<br>
Callback environment||`InitializeThreadpoolEnvironment`<br>`DestroyThreadpoolEnvironment`<br>`SetThreadpoolCallbackCleanupGroup`<br>`SetThreadpoolCallbackLibrary`<br>`SetThreadpoolCallbackPool`<br>`SetThreadpoolCallbackPriority`<br>`SetThreadpoolCallbackRunsLong`<br>
Callback||`CallbackMayRunLong`<br>
Callback clean up||`SetEventWhenCallbackReturns`<br>`DisassociateCurrentThreadFromCallback`<br>`FreeLibraryWhenCallbackReturns`<br>`LeaveCriticalSectionWhenCallbackReturns`<br>`ReleaseMutexWhenCallbackReturns`<br>`ReleaseSemaphoreWhenCallbackReturns`<br>


## 示例

此示例将创建一个自定义线程池、创建工作项和线程池计时器, 并将它们与清理组相关联. 池由一个持久线程组成.
它演示了如何使用以下线程池函数:

* `InitializeThreadpoolEnvironment`
* `SetThreadpoolThreadMaximum`
* `SetThreadpoolThreadMinimum`
* `SetThreadpoolCallbackPool`

* `CreateThreadpool`
* `CloseThreadpool`

* `CreateThreadpoolCleanupGroup`
* `CloseThreadpoolCleanupGroupMembers`
* `SetThreadpoolCallbackCleanupGroup`
* `CloseThreadpoolCleanupGroup`

* `CreateThreadpoolWait`
* `SetThreadpoolWait`
* `CloseThreadpoolWait`

* `CreateThreadpoolTimer`
* `SetThreadpoolTimer`

* `CreateThreadpoolWork`
* `WaitForThreadpoolWaitCallbacks`
* `SubmitThreadpoolWork`

```c++
#include <windows.h>
#include <tchar.h>
#include <stdio.h>

//
// 等待回调
//
VOID __stdcall MyWaitCallback(
    PTP_CALLBACK_INSTANCE Instance,
    PVOID                 Parameter,
    PTP_WAIT              Wait,
    TP_WAIT_RESULT        WaitResult
)
{
    // Instance, Parameter, Wait, and WaitResult not used in this example.
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(Parameter);
    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(WaitResult);

    //
    // 执行等待结束时的操作
    //
    _tprintf(_T("MyWaitCallback: wait is over.\n"));
}


//
// 计时器回调
//
VOID __stdcall MyTimerCallback(
    PTP_CALLBACK_INSTANCE Instance,
    PVOID                 Parameter,
    PTP_TIMER             Timer
)
{
    // Instance, Parameter, and Timer not used in this example.
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(Parameter);
    UNREFERENCED_PARAMETER(Timer);

    //
    // 发出计时时的操作
    //
    _tprintf(_T("MyTimerCallback: timer has fired.\n"));

}


//
// Work回调
//
VOID __stdcall MyWorkCallback(
    PTP_CALLBACK_INSTANCE Instance,
    PVOID                 Parameter,
    PTP_WORK              Work
)
{
    // Instance, Parameter, and Work not used in this example.
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(Parameter);
    UNREFERENCED_PARAMETER(Work);

    BOOL bRet = FALSE;
 
    //
    // 执行任务
    //
    {
        _tprintf(_T("MyWorkCallback: Task performed.\n"));
    }

    return;
}

// 演示清理组, 工作对象, 计时器对象
VOID DemoCleanupPersistentWorkTimer()
{
    BOOL bRet = FALSE;
    PTP_WORK work = NULL;
    PTP_TIMER timer = NULL;
    PTP_POOL pool = NULL;
    PTP_WORK_CALLBACK workcallback = MyWorkCallback;
    PTP_TIMER_CALLBACK timercallback = MyTimerCallback;
    TP_CALLBACK_ENVIRON CallBackEnviron;
    PTP_CLEANUP_GROUP cleanupgroup = NULL;
    FILETIME FileDueTime;
    ULARGE_INTEGER ulDueTime;
    UINT rollback = 0;

    // 初始化线程池回调环境
    // 回调在线程池的默认回调环境中执行, 没有关联的 Cleanup 组
    // 
    InitializeThreadpoolEnvironment(&CallBackEnviron);

    //
    // 创建线程池
    // 创建之后: 
    //   1. 设置(min,max)数量
    //   2. 关联回调环境
    //   3. 关联Cleanup组
    //   4. 确保指定的 DLL 保持加载状态 (如果回调需要加载锁时)
    //   5. 指定回调线程的优先级
    //   6. 设置回调执行是否需要较长时间
    //
    pool = CreateThreadpool(NULL);

    if (NULL == pool) {
        _tprintf(_T("CreateThreadpool failed. LastError: %u\n"),
                     GetLastError());
        goto main_cleanup;
    }

    rollback = 1; // pool creation succeeded

    //
    // The thread pool is made persistent simply by setting
    // both the minimum and maximum threads to 1.
    //
    SetThreadpoolThreadMaximum(pool, 1);

    bRet = SetThreadpoolThreadMinimum(pool, 1);

    if (FALSE == bRet) {
        _tprintf(_T("SetThreadpoolThreadMinimum failed. LastError: %u\n"),
                     GetLastError());
        goto main_cleanup;
    }

    //
    // 创建 Cleanup 组
    //
    cleanupgroup = CreateThreadpoolCleanupGroup();

    if (NULL == cleanupgroup) {
        _tprintf(_T("CreateThreadpoolCleanupGroup failed. LastError: %u\n"), 
                     GetLastError());
        goto main_cleanup; 
    }

    rollback = 2;  // Cleanup group creation succeeded

    //
    // 关联回调环境
    //
    SetThreadpoolCallbackPool(&CallBackEnviron, pool);

    //
    // Associate the cleanup group with our thread pool.
    // Objects created with the same callback environment
    // as the cleanup group become members of the cleanup group.
    //
    SetThreadpoolCallbackCleanupGroup(&CallBackEnviron,
                                      cleanupgroup,
                                      NULL);

    //
    // 创建工作对象
    //
    work = CreateThreadpoolWork(workcallback,
                                NULL, 
                                &CallBackEnviron);

    if (NULL == work) {
        _tprintf(_T("CreateThreadpoolWork failed. LastError: %u\n"),
                     GetLastError());
        goto main_cleanup;
    }

    rollback = 3;  // Creation of work succeeded

    //
    // 此工作对象提交到线程池
    // 最多可提交 MAXULONG 次(提交会导致工作线程并行执行)
    // Because this was a pre-allocated work item (using CreateThreadpoolWork), it is guaranteed to execute.
    //
    SubmitThreadpoolWork(work);


    //
    // 创建计时器对象
    //
    timer = CreateThreadpoolTimer(timercallback,
                                  NULL,
                                  &CallBackEnviron);


    if (NULL == timer) {
        _tprintf(_T("CreateThreadpoolTimer failed. LastError: %u\n"),
                     GetLastError());
        goto main_cleanup;
    }

    rollback = 4;  // Timer creation succeeded

    //
    // Set the timer to fire in one second.
    // 以 100 纳秒为单位, 负数表示从当前时间开始计时
    //
    ulDueTime.QuadPart = (ULONGLONG) -(1 * 10 * 1000 * 1000);
    FileDueTime.dwHighDateTime = ulDueTime.HighPart;
    FileDueTime.dwLowDateTime  = ulDueTime.LowPart;

    // 会立即替换旧的计时器对象
    SetThreadpoolTimer(timer,
                       &FileDueTime,
                       0,
                       0);

    //
    // Delay for the timer to be fired
    //
    Sleep(1500);

    //
    // Wait for all callbacks to finish.
    // CloseThreadpoolCleanupGroupMembers also releases objects
    // that are members of the cleanup group, so it is not necessary 
    // to call close functions on individual objects 
    // after calling CloseThreadpoolCleanupGroupMembers.
    //
    CloseThreadpoolCleanupGroupMembers(cleanupgroup,
                                       FALSE,
                                       NULL);

    //
    // Already cleaned up the work item with the
    // CloseThreadpoolCleanupGroupMembers, so set rollback to 2.
    //
    rollback = 2;
    goto main_cleanup;

main_cleanup:
    //
    // Clean up any individual pieces manually
    // Notice the fall-through structure of the switch.
    // Clean up in reverse order.
    //

    switch (rollback) {
        case 4:
        case 3:
            // Clean up the cleanup group members.
            CloseThreadpoolCleanupGroupMembers(cleanupgroup,
                FALSE, NULL);
        case 2:
            // Clean up the cleanup group.
            CloseThreadpoolCleanupGroup(cleanupgroup);

        case 1:
            // Clean up the pool.
            CloseThreadpool(pool);

        default:
            break;
    }

    return;
}

VOID DemoNewRegisterWait()
{
    PTP_WAIT Wait = NULL;
    PTP_WAIT_CALLBACK waitcallback = MyWaitCallback;
    HANDLE hEvent = NULL;
    UINT i = 0;
    UINT rollback = 0;

    //
    // Create an auto-reset event.
    //
    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (NULL == hEvent) {
        // Error Handling
        return;
    }

    rollback = 1; // CreateEvent succeeded

    //
    // 等待对象只等待一个 HANDLE
    // 注意: 它关联了默认回调环境
    // 
    Wait = CreateThreadpoolWait(waitcallback,
                                NULL,
                                NULL);

    if(NULL == Wait) {
        _tprintf(_T("CreateThreadpoolWait failed. LastError: %u\n"),
                     GetLastError());
        goto new_wait_cleanup;
    }

    rollback = 2; // CreateThreadpoolWait succeeded

    //
    // Need to re-register the event with the wait object
    // each time before signaling the event to trigger the wait callback.
    // 替换等待对象
    for (i = 0; i < 5; i ++) {
        SetThreadpoolWait(Wait,
                          hEvent,
                          NULL);

        SetEvent(hEvent);

        //
        // Delay for the waiter thread to act if necessary.
        //
        Sleep(500);

        //
        // Block here until the callback function is done executing.
        //

        WaitForThreadpoolWaitCallbacks(Wait, FALSE);
    }

new_wait_cleanup:
    switch (rollback) {
        case 2:
            // Unregister the wait by setting the event to NULL.
            SetThreadpoolWait(Wait, NULL, NULL);

            // Close the wait.
            CloseThreadpoolWait(Wait);

        case 1:
            // Close the event.
            CloseHandle(hEvent);

        default:
            break;
    }
    return;
}

int main( void)
{
    DemoNewRegisterWait();
    DemoCleanupPersistentWorkTimer();
    return 0;
}
```
