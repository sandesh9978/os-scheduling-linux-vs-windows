
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <windows.h>
#include <psapi.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

// Link PSAPI library for GetProcessMemoryInfo
#pragma comment(lib, "psapi.lib")

#define MAX_PATIENTS 10

/* ================= ICU PATIENT STRUCT ================= */
struct Patient {
    int id;
    double at, bt;
    int prio;   // Clinical Priority
    double st, ft, wt, tat, rt;
    int is_completed;
};

/* ================= REAL WINDOWS SWAP HEURISTIC ================= */
double measure_hardware_swap_windows() {
    LARGE_INTEGER freq, s, e;
    QueryPerformanceFrequency(&freq);

    FILE *fp = fopen("disk_test.bin", "wb");
    if (!fp) return 2.0;

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

int main() {

    struct Patient p[MAX_PATIENTS];
    int n = MAX_PATIENTS;
    int total_context_switches = 0;

    printf("HOSPITAL ICU PATIENT MONITORING SYSTEM\n");
    printf("CPU SCHEDULING & REAL-TIME PERFORMANCE REPORT\n");
    printf("Scheduling Policy: Priority (Non-Preemptive)\n");
    printf("Configuration: HIGHER NUMBER = HIGHER CLINICAL PRIORITY\n\n");

    double swap_time = measure_hardware_swap_windows();
    printf("Hardware Benchmark Complete. Swap Penalty: %.6f units\n", swap_time);

    srand((unsigned)time(NULL));

    /* -------- Manual Patient Arrival, Burst Time, Priority -------- */
    double AT[10] = {2,3,4,3,2,2,5,3,5,5};
    double BT[10] = {9,6,3,9,9,9,3,9,9,5};
    int PRIO[10]  = {9,1,5,4,1,9,9,7,2,9};  // optional

    for (int i = 0; i < n; i++) {
        p[i].id = i + 1;
        p[i].at = AT[i];
        p[i].bt = BT[i];
        p[i].prio = PRIO[i];
        p[i].is_completed = 0;
    }


    LARGE_INTEGER freq, exec_start, exec_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&exec_start);

    LARGE_INTEGER sched_s, sched_e;
    double total_sched_latency_sec = 0;

    double current_time = 0;
    int completed = 0;

    struct Patient *execution_sequence[MAX_PATIENTS];
    int exec_idx = 0;

    /* ================= ICU PRIORITY SCHEDULER ================= */
    while (completed < n) {
        int idx = -1;
        int highest_prio = -1;

        QueryPerformanceCounter(&sched_s);

        for (int i = 0; i < n; i++) {
            if (!p[i].is_completed && p[i].at <= current_time) {
                if (p[i].prio > highest_prio) {
                    highest_prio = p[i].prio;
                    idx = i;
                } else if (p[i].prio == highest_prio) {
                    if (p[i].at < p[idx].at)
                        idx = i;
                }
            }
        }

        QueryPerformanceCounter(&sched_e);
        total_sched_latency_sec +=
            (double)(sched_e.QuadPart - sched_s.QuadPart) / freq.QuadPart;

        if (idx != -1) {

            // Simulate paging / cache miss penalty
            if ((current_time - p[idx].at) > 5) {
                current_time += swap_time;
                total_context_switches++;
            }

            p[idx].st = current_time;
            p[idx].rt = p[idx].st - p[idx].at;

            current_time += p[idx].bt;
            p[idx].ft = current_time;

            p[idx].tat = p[idx].ft - p[idx].at;
            p[idx].wt = p[idx].tat - p[idx].bt;

            p[idx].is_completed = 1;
            completed++;
            execution_sequence[exec_idx++] = &p[idx];
        } else {
            current_time++; // ICU system idle
        }
    }

    QueryPerformanceCounter(&exec_end);
    double exec_time_sec =
        (double)(exec_end.QuadPart - exec_start.QuadPart) / freq.QuadPart;

    double total_wt = 0, total_tat = 0, total_rt = 0, total_bt = 0;
    double max_wt = 0, min_wt = 1e9, max_tat = 0, min_tat = 1e9;

    for (int i = 0; i < n; i++) {
        total_wt += p[i].wt;
        total_tat += p[i].tat;
        total_rt += p[i].rt;
        total_bt += p[i].bt;
        if (p[i].wt > max_wt) max_wt = p[i].wt;
        if (p[i].wt < min_wt) min_wt = p[i].wt;
        if (p[i].tat > max_tat) max_tat = p[i].tat;
        if (p[i].tat < min_tat) min_tat = p[i].tat;
    }

    /* ================= PATIENT EXECUTION TABLE ================= */
    printf("\nPatient Scheduling Table (Execution Sequence)\n");
    printf("+------+-------+-------+------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| P_ID |   AT  |   BT  | Prio |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+------+-------+-------+------+-----------+-----------+-----------+-----------+-----------+\n");

    for (int i = 0; i < n; i++) {
        struct Patient *pt = execution_sequence[i];
        printf("| P%02d  | %-5.1f | %-5.1f | %-4d | %-9.2f | %-9.2f | %-9.2f | %-9.2f | %-9.2f |\n",
               pt->id, pt->at, pt->bt, pt->prio,
               pt->st, pt->ft, pt->wt, pt->tat, pt->rt);
    }

    printf("+------+-------+-------+------+-----------+-----------+-----------+-----------+-----------+\n");

    printf("\nICU Real-Time Performance Metrics\n");
    printf("=================================\n");
    printf("Average Waiting Time       : %.2f units\n", total_wt / n);
    printf("Average Turnaround Time    : %.2f units\n", total_tat / n);
    printf("Average Response Time      : %.2f units\n", total_rt / n);
    printf("Worst-Case Patient Latency : %.0f units\n", max_wt);
    printf("System Throughput          : %.4f patients/unit\n", (double)n / current_time);
    printf("CPU Utilization            : %.0f%%\n", (total_bt / current_time) * 100);

    printf("\nContext Switching & Memory Metrics\n");
    printf("=================================\n");
    printf("Swap Time (Hardware Calc)  : %.6f units\n", swap_time);
    printf("Total Context Switches     : %d\n", total_context_switches);
    printf("Total Swap Overhead        : %.6f units\n", total_context_switches * swap_time);

    printf("\nExecution Timing\n");
    printf("=================================\n");
    printf("Program Execution Time     : %.9f seconds\n", exec_time_sec);
    printf("Scheduling Latency         : %.9f seconds\n", total_sched_latency_sec);

    return 0;
}
