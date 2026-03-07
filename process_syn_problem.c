#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0500

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <psapi.h>
#include <time.h>

#define ITERATIONS 2000000   // High workload to expose race condition

/* ================= SHARED ICU RESOURCE ================= */
// Represents shared patient data updates
volatile int patient_record_updates = 0;
volatile int total_context_switches = 0;
double swap_overhead = 0;

/* ================= CPU UTILIZATION HELPERS ================= */

unsigned __int64 FileTimeToInt64(FILETIME ft) {
    return (((unsigned __int64)ft.dwHighDateTime) << 32) |
           ((unsigned __int64)ft.dwLowDateTime);
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

/* ================= REAL HARDWARE SWAP HEURISTIC ================= */
double measure_hardware_swap_windows() {
    LARGE_INTEGER freq, s, e;
    QueryPerformanceFrequency(&freq);

    FILE *fp = fopen("disk_test.bin", "wb");
    if (!fp) return 0.000001;

    char buffer[1024 * 1024];
    memset(buffer, 0, sizeof(buffer));

    QueryPerformanceCounter(&s);
    for (int i = 0; i < 5; i++)
        fwrite(buffer, 1, sizeof(buffer), fp);
    fflush(fp);
    fclose(fp);
    QueryPerformanceCounter(&e);
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

/* ================= ICU MONITORING THREAD (RACE CONDITION) ================= */
DWORD WINAPI UpdatePatientRecords(LPVOID lpParam) {

    int delay_iters = (int)(swap_overhead * 1000);

    for (int i = 0; i < ITERATIONS; i++) {

        /* ---- CRITICAL SECTION (UNPROTECTED) ---- */
        int temp = patient_record_updates;

        // Simulate OS scheduling / cache miss / context switch
        if (i % 50000 == 0) {
            total_context_switches++;
            volatile int dummy = 0;
            for (int k = 0; k < delay_iters; k++) dummy++;
        }

        patient_record_updates = temp + 1;
    }
    return 0;
}

int main() {

    HANDLE hThreads[2];
    CpuStats cpuStart, cpuEnd;
    LARGE_INTEGER freq, start_time, end_time;

    printf("HOSPITAL ICU PATIENT MONITORING SYSTEM\n");
    printf("PROCESS SYNCHRONIZATION FAILURE DEMONSTRATION\n");
    printf("Scenario: Concurrent Patient Data Updates\n\n");

    /* -------- Hardware Calibration -------- */
    swap_overhead = measure_hardware_swap_windows();

    QueryPerformanceFrequency(&freq);
    GetCpuStats(&cpuStart);
    QueryPerformanceCounter(&start_time);

    /* -------- Launch Concurrent Monitoring Modules -------- */
    hThreads[0] = CreateThread(NULL, 0, UpdatePatientRecords, NULL, 0, NULL);
    hThreads[1] = CreateThread(NULL, 0, UpdatePatientRecords, NULL, 0, NULL);

    WaitForMultipleObjects(2, hThreads, TRUE, INFINITE);

    QueryPerformanceCounter(&end_time);
    GetCpuStats(&cpuEnd);

    double exec_time =
        (double)(end_time.QuadPart - start_time.QuadPart) / freq.QuadPart;
    double cpu_utilization = CalculateCpuUsage(&cpuStart, &cpuEnd);

    int expected = ITERATIONS * 2;
    int actual = patient_record_updates;
    int lost = expected - actual;

    /* ================= REPORT ================= */
    printf("\n===========================================================\n");
    printf("        ICU PROCESS SYNCHRONIZATION REPORT                \n");
    printf("===========================================================\n");

    printf("\nPatient Data Integrity:\n");
    printf("---------------------------------------\n");
    printf("Expected Record Updates : %d\n", expected);
    printf("Actual Record Updates   : %d\n", actual);

    if (actual != expected) {
        printf("System Status           : [CRITICAL FAILURE]\n");
        printf("Fault Type              : Race Condition\n");
        printf("Lost Clinical Updates   : %d\n", lost);
    } else {
        printf("System Status           : [SAFE]\n");
        printf("Fault Type              : None\n");
    }

    printf("\nContext Switching & Memory Effects:\n");
    printf("---------------------------------------\n");
    printf("Swap Penalty            : %.6f units\n", swap_overhead);
    printf("Context Switch Events   : %d\n", total_context_switches);
    printf("Total Swap Overhead     : %.6f units\n",
           total_context_switches * swap_overhead);

    printf("\nReal-Time Performance Metrics:\n");
    printf("---------------------------------------\n");
    printf("Execution Time          : %.6f seconds\n", exec_time);
    printf("CPU Utilization         : %.2f%% (System-Wide)\n", cpu_utilization);
    printf("Update Rate             : %.2f updates/sec\n",
           expected / exec_time);

    printf("===========================================================\n");

    CloseHandle(hThreads[0]);
    CloseHandle(hThreads[1]);

    return 0;
}


