#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <windows.h>
#include <psapi.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Link PSAPI library for GetProcessMemoryInfo
#pragma comment(lib, "psapi.lib")

#define TIME_QUANTUM 4.0
#define MAX_PATIENTS 10

/* ================= ICU PATIENT STRUCT ================= */
struct Patient {
    int id;
    double at, bt, rem_bt;
    double st, ft, wt, tat, rt;
    int first_run;
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
    printf("Scheduling Policy: Round Robin (Time Quantum = %.1f units)\n\n", TIME_QUANTUM);

    double swap_time = measure_hardware_swap_windows();
    printf("Hardware Benchmark Complete. Swap Penalty: %.6f units\n", swap_time);

    srand((unsigned)time(NULL));

   /* -------- Manual Patient Arrival & Burst Time -------- */
    double AT[10] = {2, 5, 5, 2, 2, 5, 2, 2, 4, 1};
    double BT[10] = {5, 6, 5, 6, 6, 2, 3, 6, 2, 9};

    for (int i = 0; i < n; i++) {
        p[i].id = i + 1;
        p[i].at = AT[i];
        p[i].bt = BT[i];
        p[i].rem_bt = p[i].bt;
        p[i].first_run = 0;
        p[i].st = -1;
    }


    /* --- SORT BY ARRIVAL TIME --- */
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (p[j].at > p[j + 1].at) {
                struct Patient temp = p[j];
                p[j] = p[j + 1];
                p[j + 1] = temp;
            }
        }
    }

    LARGE_INTEGER freq, exec_start, exec_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&exec_start);

    LARGE_INTEGER sched_s, sched_e;
    double total_sched_latency_sec = 0;

    int queue[1000];
    int front = 0, rear = 0;
    int visited[MAX_PATIENTS] = {0};

    double current_time = p[0].at;
    queue[rear++] = 0;
    visited[0] = 1;

    int completed = 0;
    int last_executed_idx = -1;

    /* ================= ROUND ROBIN SCHEDULER ================= */
    while (completed < n) {

        QueryPerformanceCounter(&sched_s);

        if (front == rear) {
            for (int i = 0; i < n; i++) {
                if (!visited[i]) {
                    current_time = p[i].at;
                    queue[rear++] = i;
                    visited[i] = 1;
                    break;
                }
            }
        }

        int idx = queue[front++];

        QueryPerformanceCounter(&sched_e);
        total_sched_latency_sec +=
            (double)(sched_e.QuadPart - sched_s.QuadPart) / freq.QuadPart;

        if (last_executed_idx != -1 && last_executed_idx != idx) {
            current_time += swap_time;
            total_context_switches++;
        }
        last_executed_idx = idx;

        if (!p[idx].first_run) {
            p[idx].st = current_time;
            p[idx].rt = p[idx].st - p[idx].at;
            p[idx].first_run = 1;
        }

        double slice = (p[idx].rem_bt < TIME_QUANTUM) ? p[idx].rem_bt : TIME_QUANTUM;
        current_time += slice;
        p[idx].rem_bt -= slice;

        for (int i = 0; i < n; i++) {
            if (!visited[i] && p[i].at <= current_time) {
                queue[rear++] = i;
                visited[i] = 1;
            }
        }

        if (p[idx].rem_bt <= 0) {
            p[idx].ft = current_time;
            p[idx].tat = p[idx].ft - p[idx].at;
            p[idx].wt = p[idx].tat - p[idx].bt;
            completed++;
        } else {
            queue[rear++] = idx;
        }
    }

    QueryPerformanceCounter(&exec_end);
    double exec_time_sec =
        (double)(exec_end.QuadPart - exec_start.QuadPart) / freq.QuadPart;

    /* --- SORT BACK BY PATIENT ID --- */
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (p[j].id > p[j + 1].id) {
                struct Patient temp = p[j];
                p[j] = p[j + 1];
                p[j + 1] = temp;
            }
        }
    }

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

    printf("\nPatient Scheduling Table (Round Robin)\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| P_ID |   AT  |   BT  |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    for (int i = 0; i < n; i++) {
        printf("| P%02d  | %-5.1f | %-5.1f | %-9.2f | %-9.2f | %-9.2f | %-9.2f | %-9.2f |\n",
               p[i].id, p[i].at, p[i].bt,
               p[i].st, p[i].ft, p[i].wt, p[i].tat, p[i].rt);
    }

    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    printf("\nICU Performance Metrics\n");
    printf("=================================\n");
    printf("Average Waiting Time       : %.2f units\n", total_wt / n);
    printf("Average Turnaround Time    : %.2f units\n", total_tat / n);
    printf("Average Response Time      : %.2f units\n", total_rt / n);
    printf("Worst-Case Patient Latency : %.0f units\n", max_wt);
    printf("System Throughput          : %.4f patients/unit\n", (double)n / current_time);
    printf("CPU Utilization            : %.0f%%\n", (total_bt / current_time) * 100);

    printf("\nContext Switching Metrics\n");
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

