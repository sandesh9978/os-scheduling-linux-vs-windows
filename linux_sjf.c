#define _POSIX_C_SOURCE 199309L // Required for clock_gettime
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/resource.h>

// Patient Structure
struct Patient {
    int id;
    double arrival_time, treatment_time, start_time, finish_time, waiting_time, turnaround_time, response_time;
    int is_treated; // Flag for SJF logic
};

/* ================= REAL LINUX SWAP HEURISTIC ================= */
// Measures Disk I/O and RAM usage to calculate a realistic penalty
double measure_hardware_swap_linux() {
    struct timespec s, e;

    /* Disk latency test */
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

    /* Process memory usage */
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    double mem_usage = usage.ru_maxrss / 1024.0; // MB (Linux reports KB)

    /* File open latency (Simulate OS overhead) */
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
    int n = 10; // Number of patients
    struct Patient patients[10];
    int total_swaps = 0;

    printf("Hospital ICU - SJF Patient Scheduler (Linux Real-Time)\n");
    printf("Initializing ICU System...\n");

    // Calculate Real Hardware Swap Cost
    double swap_time = measure_hardware_swap_linux();
    printf("Hardware Benchmark Complete. Swap Penalty: %.6f units\n", swap_time);

    // FIXED INPUT GENERATION
    patients[0] = (struct Patient){1, 4.0, 2.0, 0,0,0,0,0,0};
    patients[1] = (struct Patient){2, 2.0, 4.0, 0,0,0,0,0,0};
    patients[2] = (struct Patient){3, 4.0, 8.0, 0,0,0,0,0,0};
    patients[3] = (struct Patient){4, 5.0, 9.0, 0,0,0,0,0,0};
    patients[4] = (struct Patient){5, 3.0, 9.0, 0,0,0,0,0,0};
    patients[5] = (struct Patient){6, 5.0, 3.0, 0,0,0,0,0,0};
    patients[6] = (struct Patient){7, 4.0, 8.0, 0,0,0,0,0,0};
    patients[7] = (struct Patient){8, 1.0, 8.0, 0,0,0,0,0,0};
    patients[8] = (struct Patient){9, 4.0, 9.0, 0,0,0,0,0,0};
    patients[9] = (struct Patient){10, 2.0, 8.0, 0,0,0,0,0,0};

    /* --- START EXECUTION TIMER --- */
    struct timespec exec_start, exec_end;
    clock_gettime(CLOCK_MONOTONIC, &exec_start);

    double current_time = 0;
    double total_wt = 0, total_tat = 0, total_rt = 0, total_treatment = 0;
    double max_wt = 0, min_wt = 1e9, max_tat = 0, min_tat = 1e9;
    int treated_count = 0;

    // Scheduling Latency
    struct timespec sched_s, sched_e;
    double total_sched_latency_sec = 0;

    struct Patient *patient_order[10];
    int exec_idx = 0;

    /* ================= SJF LOGIC ================= */
    while (treated_count < n) {
        int idx = -1;
        double min_tt = 1e9;

        clock_gettime(CLOCK_MONOTONIC, &sched_s);

        for (int i = 0; i < n; i++) {
            if (patients[i].arrival_time <= current_time && patients[i].is_treated == 0) {
                if (patients[i].treatment_time < min_tt) {
                    min_tt = patients[i].treatment_time;
                    idx = i;
                }
                if (patients[i].treatment_time == min_tt && patients[i].arrival_time < patients[idx].arrival_time) {
                    idx = i;
                }
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &sched_e);
        total_sched_latency_sec += (sched_e.tv_sec - sched_s.tv_sec) +
                                   (sched_e.tv_nsec - sched_s.tv_nsec) / 1e9;

        if (idx != -1) {
            // Swap overhead if patient waited too long
            if ((current_time - patients[idx].arrival_time) > 5) {
                current_time += swap_time;
                total_swaps++;
            }

            patients[idx].start_time = current_time;
            patients[idx].response_time = patients[idx].start_time - patients[idx].arrival_time;

            current_time += patients[idx].treatment_time;
            patients[idx].finish_time = current_time;

            patients[idx].turnaround_time = patients[idx].finish_time - patients[idx].arrival_time;
            patients[idx].waiting_time = patients[idx].turnaround_time - patients[idx].treatment_time;

            total_wt += patients[idx].waiting_time;
            total_tat += patients[idx].turnaround_time;
            total_rt += patients[idx].response_time;
            total_treatment += patients[idx].treatment_time;

            if (patients[idx].waiting_time > max_wt) max_wt = patients[idx].waiting_time;
            if (patients[idx].waiting_time < min_wt) min_wt = patients[idx].waiting_time;
            if (patients[idx].turnaround_time > max_tat) max_tat = patients[idx].turnaround_time;
            if (patients[idx].turnaround_time < min_tat) min_tat = patients[idx].turnaround_time;

            patients[idx].is_treated = 1;
            treated_count++;
            patient_order[exec_idx++] = &patients[idx];
        } else {
            current_time++;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &exec_end);
    double exec_time_sec = (exec_end.tv_sec - exec_start.tv_sec) +
                           (exec_end.tv_nsec - exec_start.tv_nsec) / 1e9;

    /* ================= ICU PATIENT TABLE ================= */
    printf("\nICU Patient Monitoring Table (SJF Scheduling)\n");
    printf("+-------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
printf("| P_ID  |  AT   |  TT   |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
printf("+-------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

for (int i = 0; i < n; i++) {
    struct Patient *p = patient_order[i];
    printf("| P%02d   | %-5.1f | %-5.1f | %-9.2f | %-9.2f | %-9.2f | %-9.2f | %-9.2f |\n",
           p->id,
           p->arrival_time,
           p->treatment_time,
           p->start_time,
           p->finish_time,
           p->waiting_time,
           p->turnaround_time,
           p->response_time);
}

printf("+-------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

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
    printf("Throughput                 : %.4f patients/unit\n", (double)n / current_time);
    printf("CPU Utilization            : %.0f%%\n", (total_treatment / current_time) * 100);

    printf("\nSwapping Metrics:\n");
    printf("=================================\n");
    printf("Swap Time (Hardware Calc)  : %.6f units\n", swap_time);
    printf("Total Context Swaps        : %d\n", total_swaps);
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


