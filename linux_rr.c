#define _POSIX_C_SOURCE 199309L // Required for clock_gettime
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/resource.h>

#define TIME_QUANTUM 4.0
#define MAX_PATIENTS 10

struct Patient {
    int id;
    double at, bt, rem_bt; // at = Arrival Time, bt = Treatment Time, rem_bt = Remaining Treatment
    double st, ft, wt, tat, rt; // Start, Finish, Waiting, Turnaround, Response
    int first_run; // Flag to capture Start/Response Time
};

/* ================= REAL LINUX SWAP HEURISTIC ================= */
// Measures Disk I/O and RAM usage to calculate a realistic penalty
double measure_hardware_swap_linux() {
    struct timespec s, e;

    /* 1. Disk latency test */
    FILE *fp = fopen("disk_test.bin", "wb");
    if (!fp) return 2.0;

    char buffer[1024 * 1024]; // 1MB Buffer
    memset(buffer, 0, sizeof(buffer));

    clock_gettime(CLOCK_MONOTONIC, &s);
    for (int i = 0; i < 5; i++)
        fwrite(buffer, 1, sizeof(buffer), fp); // Write 5MB
    fflush(fp);
    fclose(fp);
    clock_gettime(CLOCK_MONOTONIC, &e);
    remove("disk_test.bin"); // Cleanup

    double disk_time = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9;
    double disk_speed = 5.0 / disk_time;   // MB/sec

    /* 2. Process memory usage */
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    double mem_usage = usage.ru_maxrss / 1024.0; // MB (Linux reports KB)

    /* 3. File open latency (Simulate OS overhead) */
    clock_gettime(CLOCK_MONOTONIC, &s);
    fp = fopen("lat.txt", "w");
    if (fp) {
        fputc('A', fp);
        fclose(fp);
    }
    clock_gettime(CLOCK_MONOTONIC, &e);
    remove("lat.txt");

    double latency = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9;

    /* Heuristic Formula */
    double result = 2 * (latency + (mem_usage / disk_speed));
    return (result * 1000) > 1.0 ? (result * 1000) : 1.5;
}

int main() {
    struct Patient p[MAX_PATIENTS];
    int n = MAX_PATIENTS;
    int total_swaps = 0;

    printf("Hospital ICU - Round Robin Patient Scheduler (Linux Real-Time)\n");
    printf("Initializing ICU System (Time Quantum = %.1f units)...\n", TIME_QUANTUM);

    // 1. Calculate Real Hardware Swap Cost
    double swap_time = measure_hardware_swap_linux();
    printf("Hardware Benchmark Complete. Swap Penalty: %.6f units\n", swap_time);

    // 2. FIXED INPUT GENERATION (Example Patient Data)
    p[0].id = 1; p[0].at = 2.0; p[0].bt = 5.0; p[0].rem_bt = 5.0; p[0].first_run = 0; p[0].st = -1;
    p[1].id = 2; p[1].at = 5.0; p[1].bt = 6.0; p[1].rem_bt = 6.0; p[1].first_run = 0; p[1].st = -1;
    p[2].id = 3; p[2].at = 5.0; p[2].bt = 5.0; p[2].rem_bt = 5.0; p[2].first_run = 0; p[2].st = -1;
    p[3].id = 4; p[3].at = 2.0; p[3].bt = 6.0; p[3].rem_bt = 6.0; p[3].first_run = 0; p[3].st = -1;
    p[4].id = 5; p[4].at = 2.0; p[4].bt = 6.0; p[4].rem_bt = 6.0; p[4].first_run = 0; p[4].st = -1;
    p[5].id = 6; p[5].at = 5.0; p[5].bt = 2.0; p[5].rem_bt = 2.0; p[5].first_run = 0; p[5].st = -1;
    p[6].id = 7; p[6].at = 2.0; p[6].bt = 3.0; p[6].rem_bt = 3.0; p[6].first_run = 0; p[6].st = -1;
    p[7].id = 8; p[7].at = 2.0; p[7].bt = 6.0; p[7].rem_bt = 6.0; p[7].first_run = 0; p[7].st = -1;
    p[8].id = 9; p[8].at = 4.0; p[8].bt = 2.0; p[8].rem_bt = 2.0; p[8].first_run = 0; p[8].st = -1;
    p[9].id = 10; p[9].at = 1.0; p[9].bt = 9.0; p[9].rem_bt = 9.0; p[9].first_run = 0; p[9].st = -1;

    /* --- SORT BY ARRIVAL TIME (Preprocessing) --- */
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (p[j].at > p[j+1].at) {
                struct Patient temp = p[j];
                p[j] = p[j+1];
                p[j+1] = temp;
            }
        }
    }

    /* --- START EXECUTION TIMER --- */
    struct timespec exec_start, exec_end;
    clock_gettime(CLOCK_MONOTONIC, &exec_start);

    // Scheduling Latency Timers
    struct timespec sched_s, sched_e;
    double total_sched_latency_sec = 0;

    // ICU Queue Simulation
    int queue[1000];
    int front = 0, rear = 0;
    int visited[MAX_PATIENTS] = {0};

    // Push first patient(s)
    double current_time = p[0].at;
    queue[rear++] = 0;
    visited[0] = 1;

    int completed = 0;
    int last_executed_idx = -1;

    /* ================= ROUND ROBIN LOGIC ================= */
    while (completed < n) {

        clock_gettime(CLOCK_MONOTONIC, &sched_s);

        // If queue empty, jump to next patient arrival
        if (front == rear) {
            for(int i=0; i<n; i++) {
                if (!visited[i]) {
                    current_time = p[i].at;
                    queue[rear++] = i;
                    visited[i] = 1;
                    break;
                }
            }
        }

        int idx = queue[front++]; // Dequeue patient

        clock_gettime(CLOCK_MONOTONIC, &sched_e);
        total_sched_latency_sec += (sched_e.tv_sec - sched_s.tv_sec) +
                                   (sched_e.tv_nsec - sched_s.tv_nsec) / 1e9;

        // Context Switch Simulation (ICU transfer)
        if (last_executed_idx != -1 && last_executed_idx != idx) {
            current_time += swap_time;
            total_swaps++;
        }
        last_executed_idx = idx;

        // Record Start Time / Response Time on first treatment
        if (p[idx].first_run == 0) {
            p[idx].st = current_time;
            p[idx].rt = p[idx].st - p[idx].at;
            p[idx].first_run = 1;
        }

        // Execute treatment slice
        double slice = (p[idx].rem_bt < TIME_QUANTUM) ? p[idx].rem_bt : TIME_QUANTUM;
        current_time += slice;
        p[idx].rem_bt -= slice;

        // Check for new patient arrivals
        for (int i = 0; i < n; i++) {
            if (!visited[i] && p[i].at <= current_time) {
                queue[rear++] = i;
                visited[i] = 1;
            }
        }

        // Check if treatment complete or re-queue
        if (p[idx].rem_bt <= 0) {
            p[idx].ft = current_time;
            p[idx].tat = p[idx].ft - p[idx].at;
            p[idx].wt = p[idx].tat - p[idx].bt;
            completed++;
        } else {
            queue[rear++] = idx;
        }
    }

    /* --- STOP EXECUTION TIMER --- */
    clock_gettime(CLOCK_MONOTONIC, &exec_end);
    double exec_time_sec = (exec_end.tv_sec - exec_start.tv_sec) +
                           (exec_end.tv_nsec - exec_start.tv_nsec) / 1e9;

    // Re-sort by ID for table display
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (p[j].id > p[j+1].id) {
                struct Patient temp = p[j];
                p[j] = p[j+1];
                p[j+1] = temp;
            }
        }
    }

    // Calculate totals
    double total_wt = 0, total_tat = 0, total_rt = 0, total_bt = 0;
    double max_wt = 0, min_wt = 1e9, max_tat = 0, min_tat = 1e9;

    for(int i=0; i<n; i++) {
        total_wt += p[i].wt;
        total_tat += p[i].tat;
        total_rt += p[i].rt;
        total_bt += p[i].bt;
        if (p[i].wt > max_wt) max_wt = p[i].wt;
        if (p[i].wt < min_wt) min_wt = p[i].wt;
        if (p[i].tat > max_tat) max_tat = p[i].tat;
        if (p[i].tat < min_tat) min_tat = p[i].tat;
    }

    /* ================= FIXED TABLE ================= */
    printf("\nICU Patient Treatment Table\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| P_ID |   AT  |   TT  |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    for (int i = 0; i < n; i++) {
        printf("| P%02d  | %-5.1f | %-5.1f | %-9.2f | %-9.2f | %-9.2f | %-9.2f | %-9.2f |\n",
               p[i].id, p[i].at, p[i].bt,
               p[i].st, p[i].ft, p[i].wt, p[i].tat, p[i].rt);
    }

    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    /* -------- METRICS -------- */
    printf("\nICU Performance Metrics:\n");
    printf("=================================\n");
    printf("Average Waiting Time       : %.2f units\n", total_wt / n);
    printf("Average Turnaround Time    : %.2f units\n", total_tat / n);
    printf("Average Response Time      : %.2f units\n", total_rt / n);
    printf("Maximum Waiting Time       : %.0f units\n", max_wt);
    printf("Minimum Waiting Time       : %.0f units\n", min_wt);
    printf("Maximum Turnaround Time    : %.0f units\n", max_tat);
    printf("Minimum Turnaround Time    : %.0f units\n", min_tat);
    printf("Throughput                 : %.4f patients/unit\n", (double)n / current_time);
    printf("CPU Utilization            : %.0f%%\n", (total_bt / current_time) * 100);

    printf("\nPatient Swapping Metrics:\n");
    printf("=================================\n");
    printf("Swap Time (Hardware Calc)  : %.6f units\n", swap_time);
    printf("Total Patient Transfers    : %d\n", total_swaps);
    printf("Total Swapping Overhead    : %.6f units\n", total_swaps * swap_time);

    printf("\nReal-Time Execution Metrics:\n");
    printf("=================================\n");
    printf("Program Execution Time     : %.9f seconds\n", exec_time_sec);
    printf("Scheduling Latency         : %.9f seconds\n", total_sched_latency_sec);
    printf("Average Patient Latency    : %.2f units\n", total_wt / n);
    printf("Total Latency              : %.2f units\n", total_wt);
    printf("Worst-Case Latency         : %.0f units\n", max_wt);

    return 0;
}


