#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0500

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <psapi.h>
#include <time.h>

#define ITERATIONS 2000000 

// Shared Resources
volatile int patient_record_updates = 0;      // Tracks concurrent patient data updates
volatile int total_context_switches = 0;      // Simulates CPU scheduling/context switches
double swap_overhead = 0;                     // Simulated hardware delay

// SYNCHRONIZATION OBJECT (The Lock)
HANDLE hMutex;

// --- CPU UTILIZATION HELPERS ---

// Helper to convert FILETIME to unsigned 64-bit integer
unsigned __int64 FileTimeToInt64(FILETIME ft) {
    return (((unsigned __int64)(ft.dwHighDateTime)) << 32) | ((unsigned __int64)ft.dwLowDateTime);
}

typedef struct {
    unsigned __int64 idle;
    unsigned __int64 kernel;
    unsigned __int64 user;
} CpuStats;

void GetCpuStats(CpuStats *stats) {
    FILETIME ftIdle, ftKernel, ftUser;
    GetSystemTimes(&ftIdle, &ftKernel, &ftUser);
    stats->idle = FileTimeToInt64(ftIdle);
    stats->kernel = FileTimeToInt64(ftKernel);
    stats->user = FileTimeToInt64(ftUser);
}

double CalculateCpuUsage(CpuStats *start, CpuStats *end) {
    unsigned __int64 idleTicks   = end->idle   - start->idle;
    unsigned __int64 kernelTicks = end->kernel - start->kernel;
    unsigned __int64 userTicks   = end->user   - start->user;

    unsigned __int64 totalTicks = kernelTicks + userTicks;
    if (totalTicks == 0) return 0.0;

    double idlePercent = (double)idleTicks / (double)totalTicks * 100.0;
    return 100.0 - idlePercent;
}

// --- REAL HARDWARE SWAP HEURISTIC ---
double measure_hardware_swap_windows() {
    LARGE_INTEGER freq, s, e;
    QueryPerformanceFrequency(&freq);

    FILE *fp = fopen("disk_test.bin", "wb");
    if (!fp) return 0.000001;

    char buffer[1024 * 1024];
    memset(buffer, 0, sizeof(buffer));

    QueryPerformanceCounter(&s);
    for (int i = 0; i < 5; i++) fwrite(buffer, 1, sizeof(buffer), fp);
    fflush(fp);
    fclose(fp);
    DeleteFileA("disk_test.bin");

    double disk_time = (double)(e.QuadPart - s.QuadPart) / freq.QuadPart;
    double disk_speed = 5.0 / disk_time;

    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    double mem_usage = pmc.WorkingSetSize / (1024.0 * 1024.0);

    QueryPerformanceCounter(&s);
    fp = fopen("lat.txt", "w");
    if (fp) { fputc('A', fp); fclose(fp); }
    QueryPerformanceCounter(&e);
    DeleteFileA("lat.txt");

    double latency = (double)(e.QuadPart - s.QuadPart) / freq.QuadPart;
    double result = 2 * (latency + (mem_usage / disk_speed));

    return (result * 1000) > 1.0 ? (result * 1000) : 1.5;
}

// --- WORKER THREAD (The Solution) ---
DWORD WINAPI UpdatePatientRecords(LPVOID lpParam) {
    int delay_iters = (int)(swap_overhead * 1000);

    for (int i = 0; i < ITERATIONS; i++) {

        // 1. ACQUIRE LOCK (Wait if another monitoring module is here)
        WaitForSingleObject(hMutex, INFINITE);

        // 2. CRITICAL SECTION (Protected patient data update)
        int temp = patient_record_updates;
        patient_record_updates = temp + 1;

        // 3. RELEASE LOCK
        ReleaseMutex(hMutex);

        // Simulate Context Switch / CPU work (Outside lock to allow interleaving)
        if (i % 50000 == 0) {
            InterlockedIncrement((LONG*)&total_context_switches);
            volatile int dummy = 0;
            for(int k=0; k < delay_iters; k++) { dummy++; }
        }
    }
    return 0;
}

int main() {
    HANDLE hThreads[2];
    CpuStats cpuStart, cpuEnd;
    LARGE_INTEGER freq, start_time, end_time;

    printf("ICU Patient Monitoring System - Synchronization Solution (Windows)\n");
    printf("Simulating Concurrent Patient Data Updates with Mutex Locking...\n");

    // 1. Hardware Calibration
    swap_overhead = measure_hardware_swap_windows();

    // 2. Create Mutex
    hMutex = CreateMutex(NULL, FALSE, NULL);
    if (hMutex == NULL) {
        printf("Mutex creation failed.\n");
        return 1;
    }

    // 3. Start Timers & CPU Snapshot
    QueryPerformanceFrequency(&freq);
    GetCpuStats(&cpuStart);
    QueryPerformanceCounter(&start_time);

    // 4. Launch Threads
    hThreads[0] = CreateThread(NULL, 0, UpdatePatientRecords, NULL, 0, NULL);
    hThreads[1] = CreateThread(NULL, 0, UpdatePatientRecords, NULL, 0, NULL);

    WaitForMultipleObjects(2, hThreads, TRUE, INFINITE);

    // 5. Stop Timers & CPU Snapshot
    QueryPerformanceCounter(&end_time);
    GetCpuStats(&cpuEnd);

    // 6. Metrics Calculation
    double exec_time = (double)(end_time.QuadPart - start_time.QuadPart) / freq.QuadPart;
    double cpu_utilization = CalculateCpuUsage(&cpuStart, &cpuEnd);

    int expected = ITERATIONS * 2;
    int actual = patient_record_updates;
    int lost = expected - actual;

    // --- REPORT GENERATION ---
    printf("\n===========================================================\n");
    printf("           ICU PATIENT MONITORING - SYNCHRONIZATION REPORT\n");
    printf("===========================================================\n");

    printf("\nPatient Data Integrity:\n");
    printf("---------------------------------------\n");
    printf("Expected Record Updates : %d\n", expected);
    printf("Actual Record Updates   : %d\n", actual);

    if (actual != expected) {
        printf("System Status           : [CRITICAL FAILURE]\n");
        printf("Fault Type              : Race Condition Detected\n");
        printf("Lost Updates            : %d\n", lost);
    } else {
        printf("System Status           : [SAFE]\n");
        printf("Fault Type              : None\n");
        printf("Mechanism               : Windows Mutex (Correctly Locked)\n");
    }

    printf("\nContext Switching & Memory Effects:\n");
    printf("---------------------------------------\n");
    printf("Swap Penalty            : %.6f units\n", swap_overhead);
    printf("Context Switch Events   : %d\n", total_context_switches);
    printf("Total Swap Overhead     : %.6f units\n", total_context_switches * swap_overhead);

    printf("\nReal-Time Performance Metrics:\n");
    printf("---------------------------------------\n");
    printf("Execution Time          : %.6f seconds\n", exec_time);
    printf("CPU Utilization         : %.2f%% (System-Wide Load)\n", cpu_utilization);
    printf("Updates Per Second      : %.2f updates/sec\n", expected / exec_time);
    printf("===========================================================\n");

    // Cleanup
    CloseHandle(hThreads[0]);
    CloseHandle(hThreads[1]);
    CloseHandle(hMutex);

    return 0;
}

