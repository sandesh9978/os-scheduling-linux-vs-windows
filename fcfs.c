#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <windows.h>
#include <psapi.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Link PSAPI library for GetProcessMemoryInfo
#pragma comment(lib, "psapi.lib")

struct Patient {
    int id;
    double at, bt, st, ft, wt, tat, rt;
};

/* ================= REAL WINDOWS SWAP HEURISTIC ================= */
double measure_hardware_swap_windows() {
    LARGE_INTEGER freq, s, e;
    QueryPerformanceFrequency(&freq);

    /* 1. Disk latency test */
    FILE *fp = fopen("disk_test.bin", "wb");
    if (!fp) return 2.0;

    char buffer[1024 * 1024]; // 1MB
    memset(buffer, 0, sizeof(buffer));

    QueryPerformanceCounter(&s);
    for (int i = 0; i < 5; i++)
        fwrite(buffer, 1, sizeof(buffer), fp); // Write 5MB
    fflush(fp);
    fclose(fp);
    QueryPerformanceCounter(&e);
    DeleteFileA("disk_test.bin");

    double disk_time = (double)(e.QuadPart - s.QuadPart) / freq.QuadPart;
    double disk_speed = 5.0 / disk_time; // MB/sec

    /* 2. Process memory usage */
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    double mem_usage = pmc.WorkingSetSize / (1024.0 * 1024.0); // MB

    /* 3. File open latency */
    QueryPerformanceCounter(&s);
    fp = fopen("lat.txt", "w");
    if (fp) {
        fputc('A', fp);
        fclose(fp);
    }
    QueryPerformanceCounter(&e);
    DeleteFileA("lat.txt");

    double latency = (double)(e.QuadPart - s.QuadPart) / freq.QuadPart;

    /* Heuristic Formula */
    double result = 2 * (latency + (mem_usage / disk_speed));
    return (result * 1000) > 1.0 ? (result * 1000) : 1.5;
}

int main() {
    int n = 10;
    struct Patient p[10];
    int total_swaps = 0;

    printf("HOSPITAL ICU PATIENT MONITORING SYSTEM (Windows FCFS)\n");
    printf("Calculating System Swap Overhead...\n");

    double swap_time = measure_hardware_swap_windows();
    printf("Hardware Swap Penalty: %.6f units\n", swap_time);

    /* ================= FIXED PATIENTS ================= */
    p[0] = (struct Patient){1, 1.0, 6.0};
    p[1] = (struct Patient){2, 1.0, 2.0};
    p[2] = (struct Patient){3, 3.0, 2.0};
    p[3] = (struct Patient){4, 5.0, 9.0};
    p[4] = (struct Patient){5, 4.0, 3.0};
    p[5] = (struct Patient){6, 1.0, 5.0};
    p[6] = (struct Patient){7, 2.0, 3.0};
    p[7] = (struct Patient){8, 5.0, 7.0};
    p[8] = (struct Patient){9, 4.0, 8.0};
    p[9] = (struct Patient){10, 3.0, 6.0};

    /* --- MEASURE SCHEDULING LATENCY --- */
    LARGE_INTEGER freq, sched_start, sched_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&sched_start);

    /* Sort by Arrival Time */
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - i - 1; j++)
            if (p[j].at > p[j + 1].at) {
                struct Patient t = p[j];
                p[j] = p[j + 1];
                p[j + 1] = t;
            }

    QueryPerformanceCounter(&sched_end);
    double scheduling_latency_sec = (double)(sched_end.QuadPart - sched_start.QuadPart) / freq.QuadPart;

    /* --- START EXECUTION TIMER --- */
    LARGE_INTEGER exec_start, exec_end;
    QueryPerformanceCounter(&exec_start);

    double current_time = 0;
    double total_wt = 0, total_tat = 0, total_rt = 0, total_bt = 0;
    double max_wt = 0, min_wt = 1e9, max_tat = 0, min_tat = 1e9;

    printf("\nICU Patient Monitoring Table (FCFS)\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| P_ID |   AT  |   BT  |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    for (int i = 0; i < n; i++) {
        if (current_time < p[i].at) current_time = p[i].at;

        if ((current_time - p[i].at) > 5) {
            current_time += swap_time;
            total_swaps++;
        }

        p[i].st = current_time;
        p[i].rt = p[i].st - p[i].at;

        current_time += p[i].bt;
        p[i].ft = current_time;

        p[i].tat = p[i].ft - p[i].at;
        p[i].wt = p[i].tat - p[i].bt;

        total_wt += p[i].wt;
        total_tat += p[i].tat;
        total_rt += p[i].rt;
        total_bt += p[i].bt;

        if (p[i].wt > max_wt) max_wt = p[i].wt;
        if (p[i].wt < min_wt) min_wt = p[i].wt;
        if (p[i].tat > max_tat) max_tat = p[i].tat;
        if (p[i].tat < min_tat) min_tat = p[i].tat;

        printf("| P%02d  | %-5.1f | %-5.1f | %-9.2f | %-9.2f | %-9.2f | %-9.2f | %-9.2f |\n",
               p[i].id, p[i].at, p[i].bt, p[i].st, p[i].ft, p[i].wt, p[i].tat, p[i].rt);
    }

    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    QueryPerformanceCounter(&exec_end);
    double exec_time_sec = (double)(exec_end.QuadPart - exec_start.QuadPart) / freq.QuadPart;

    /* -------- METRICS -------- */
    printf("\nPerformance Metrics:\n");
    printf("=================================\n");
    printf("Average Waiting Time       : %.2f units\n", total_wt / n);
    printf("Average Turnaround Time    : %.2f units\n", total_tat / n);
    printf("Average Response Time      : %.2f units\n", total_rt / n);
    printf("Maximum Waiting Time       : %.0f units\n", max_wt);
    printf("Minimum Waiting Time       : %.0f units\n", min_wt);
    printf("Maximum Turnaround Time    : %.0f units\n", max_tat);
    printf("Minimum Turnaround Time    : %.0f units\n", min_tat);
    printf("Throughput                 : %.4f patients/unit\n", (double)n / p[n - 1].ft);
    printf("CPU Utilization            : %.0f%%\n", (total_bt / p[n - 1].ft) * 100);

    printf("\nSwapping Metrics:\n");
    printf("=================================\n");
    printf("Swap Time (Hardware Calc)  : %.6f units\n", swap_time);
    printf("Total Swapped Patients     : %d\n", total_swaps);
    printf("Total Swapping Overhead    : %.6f units\n", total_swaps * swap_time);

    printf("\nReal-Time Execution Metrics:\n");
    printf("=================================\n");
    printf("Program Execution Time     : %.9f seconds\n", exec_time_sec);
    printf("Scheduling Latency         : %.9f seconds\n", scheduling_latency_sec);
    printf("Average Patient Latency    : %.2f units\n", total_wt / n);
    printf("Total Latency              : %.2f units\n", total_wt);
    printf("Worst-Case Latency         : %.0f units\n", max_wt);

    return 0;
}

