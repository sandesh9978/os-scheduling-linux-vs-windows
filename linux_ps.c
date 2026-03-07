/*
Hospital ICU Patient Monitoring System
CPU Scheduling & Real-Time Performance Report
Scheduling Policy: FCFS (First-Come, First-Served)
==================================================
P1-P4 : Life-Critical Alarms
P5-P7 : Continuous Vital Streaming
P8+   : Logging & Maintenance Tasks
--------------------------------------------------
*/

#define _POSIX_C_SOURCE 199309L // Required for clock_gettime
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/resource.h>

#define MAX_PATIENTS 10

struct Patient {
    int id;
    double arrival_time, treatment_time;
    int priority; // Higher = more critical
    double start_time, finish_time, waiting_time, turnaround_time, response_time;
    int is_completed;
};

/* ================= REAL LINUX SWAP HEURISTIC ================= */
// Measures Disk I/O and RAM usage to calculate a realistic penalty
double measure_hardware_swap_linux() {
    struct timespec s, e;

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
    remove("disk_test.bin");

    double disk_time = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9;
    double disk_speed = 5.0 / disk_time;

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    double mem_usage = usage.ru_maxrss / 1024.0;

    clock_gettime(CLOCK_MONOTONIC, &s);
    fp = fopen("lat.txt", "w");
    if (fp) {
        fputc('A', fp);
        fclose(fp);
    }                                                                                                     //
    clock_gettime(CLOCK_MONOTONIC, &e);
    remove("lat.txt");

    double latency = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9;
    double result = 2 * (latency + (mem_usage / disk_speed));
    return (result * 1000) > 1.0 ? (result * 1000) : 1.5;
}

int main() {
    struct Patient patients[MAX_PATIENTS];
    int n = MAX_PATIENTS;
    int total_swaps = 0;

    printf("Hospital ICU - Patient Priority Scheduler (Linux Real-Time)\n");
    printf("Configuration: HIGHER NUMBER = HIGHER PRIORITY / CRITICALITY\n");

    double swap_time = measure_hardware_swap_linux();
    printf("Hardware Benchmark Complete. Swap Penalty: %.6f units\n", swap_time);

    // Fixed patient input
    patients[0].id=1; patients[0].arrival_time=2.0; patients[0].treatment_time=9.0; patients[0].priority=9; patients[0].is_completed=0;
    patients[1].id=2; patients[1].arrival_time=3.0; patients[1].treatment_time=6.0; patients[1].priority=1; patients[1].is_completed=0;
    patients[2].id=3; patients[2].arrival_time=4.0; patients[2].treatment_time=3.0; patients[2].priority=5; patients[2].is_completed=0;
    patients[3].id=4; patients[3].arrival_time=3.0; patients[3].treatment_time=9.0; patients[3].priority=4; patients[3].is_completed=0;
    patients[4].id=5; patients[4].arrival_time=2.0; patients[4].treatment_time=9.0; patients[4].priority=1; patients[4].is_completed=0;
    patients[5].id=6; patients[5].arrival_time=2.0; patients[5].treatment_time=9.0; patients[5].priority=9; patients[5].is_completed=0;
    patients[6].id=7; patients[6].arrival_time=5.0; patients[6].treatment_time=3.0; patients[6].priority=9; patients[6].is_completed=0;
    patients[7].id=8; patients[7].arrival_time=3.0; patients[7].treatment_time=9.0; patients[7].priority=7; patients[7].is_completed=0;
    patients[8].id=9; patients[8].arrival_time=5.0; patients[8].treatment_time=9.0; patients[8].priority=2; patients[8].is_completed=0;
    patients[9].id=10; patients[9].arrival_time=5.0; patients[9].treatment_time=5.0; patients[9].priority=9; patients[9].is_completed=0;

    struct timespec exec_start, exec_end;
    clock_gettime(CLOCK_MONOTONIC, &exec_start);

    struct timespec sched_s, sched_e;
    double total_sched_latency_sec = 0;
    double current_time = 0;
    int completed = 0;

    // Rename execution array
    struct Patient *patient_sequence[MAX_PATIENTS];
    int seq_idx = 0;

    while (completed < n) {
        int idx = -1;
        int highest_priority = -1;

        clock_gettime(CLOCK_MONOTONIC, &sched_s);

        for (int i = 0; i < n; i++) {
            if (!patients[i].is_completed && patients[i].arrival_time <= current_time) {
                if (patients[i].priority > highest_priority) {
                    highest_priority = patients[i].priority;
                    idx = i;
                } else if (patients[i].priority == highest_priority) {
                    if (patients[i].arrival_time < patients[idx].arrival_time) idx = i;
                }
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &sched_e);
        total_sched_latency_sec += (sched_e.tv_sec - sched_s.tv_sec) +
                                   (sched_e.tv_nsec - sched_s.tv_nsec) / 1e9;

        if (idx != -1) {
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

            patients[idx].is_completed = 1;
            completed++;

            patient_sequence[seq_idx++] = &patients[idx];
        } else {
            current_time++;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &exec_end);
    double exec_time_sec = (exec_end.tv_sec - exec_start.tv_sec) +
                           (exec_end.tv_nsec - exec_start.tv_nsec) / 1e9;

    // Metrics
    double total_wt=0, total_tat=0, total_rt=0, total_tt=0;
    double max_wt=0, min_wt=1e9, max_tat=0, min_tat=1e9;

    for(int i=0;i<n;i++){
        total_wt += patients[i].waiting_time;
        total_tat += patients[i].turnaround_time;
        total_rt += patients[i].response_time;
        total_tt += patients[i].treatment_time;
        if(patients[i].waiting_time>max_wt) max_wt=patients[i].waiting_time;
        if(patients[i].waiting_time<min_wt) min_wt=patients[i].waiting_time;
        if(patients[i].turnaround_time>max_tat) max_tat=patients[i].turnaround_time;
        if(patients[i].turnaround_time<min_tat) min_tat=patients[i].turnaround_time;
    }

    // Table
    printf("\nICU Patient Schedule (Execution Sequence)\n");
    printf("+------+-------+-------+------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| P_ID |  AT   |  TT   | Prio |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+------+-------+-------+------+-----------+-----------+-----------+-----------+-----------+\n");

    for(int i=0;i<n;i++){
        struct Patient *p = patient_sequence[i];
        printf("| P%02d  | %-5.1f | %-5.1f | %-4d | %-9.2f | %-9.2f | %-9.2f | %-9.2f | %-9.2f |\n",
               p->id, p->arrival_time, p->treatment_time, p->priority,
               p->start_time, p->finish_time, p->waiting_time, p->turnaround_time, p->response_time);
    }

    printf("+------+-------+-------+------+-----------+-----------+-----------+-----------+-----------+\n");

    printf("\nPerformance Metrics:\n");
    printf("=================================\n");
    printf("Average Waiting Time       : %.2f units\n", total_wt/n);
    printf("Average Turnaround Time    : %.2f units\n", total_tat/n);
    printf("Average Response Time      : %.2f units\n", total_rt/n);
    printf("Maximum Waiting Time       : %.0f units\n", max_wt);
    printf("Minimum Waiting Time       : %.0f units\n", min_wt);
    printf("Maximum Turnaround Time    : %.0f units\n", max_tat);
    printf("Minimum Turnaround Time    : %.0f units\n", min_tat);
    printf("Throughput                 : %.4f patients/unit\n", (double)n/current_time);
    printf("CPU Utilization            : %.0f%%\n", (total_tt/current_time)*100);

    printf("\nSwapping Metrics:\n");
    printf("=================================\n");
    printf("Swap Time (Hardware Calc)  : %.6f units\n", swap_time);
    printf("Total Context Switches     : %d\n", total_swaps);
    printf("Total Swapping Overhead    : %.6f units\n", total_swaps*swap_time);

    printf("\nReal-Time Execution Metrics:\n");
    printf("=================================\n");
    printf("Program Execution Time     : %.9f seconds\n", exec_time_sec);
    printf("Scheduling Latency         : %.9f seconds\n", total_sched_latency_sec);
    printf("Average Process Latency    : %.2f units\n", total_wt/n);
    printf("Total Latency              : %.2f units\n", total_wt);
    printf("Worst-Case Latency         : %.0f units\n", max_wt);

    return 0;
}

