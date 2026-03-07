#define _POSIX_C_SOURCE 199309L // For clock_gettime
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/resource.h>

struct Patient {
    int id;
    double at, bt, st, ft, wt, tat, rt;
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

    // Scale for simulation visibility
    return (result * 1000) > 1.0 ? (result * 1000) : 1.5;
}

int main() {
    int n = 10; // Fixed to 10 automated patients
    struct Patient p[10];
    int total_swaps = 0;

    /* =========================================================
       HOSPITAL ICU PATIENT MONITORING SYSTEM
       CPU SCHEDULING & REAL-TIME PERFORMANCE REPORT
       Scheduling Policy: FCFS (First-Come, First-Served)
    ========================================================= */
    printf("HOSPITAL ICU PATIENT MONITORING SYSTEM\n");
    printf("CPU SCHEDULING & REAL-TIME PERFORMANCE REPORT\n");
    printf("Scheduling Policy: FCFS (First-Come, First-Served)\n");
    printf("==================================================\n");
    printf("P1-P4 : Life-Critical Alarms\n");
    printf("P5-P7 : Continuous Vital Streaming\n");
    printf("P8+   : Logging & Maintenance Tasks\n");
    printf("--------------------------------------------------\n");

    printf("ICU System Benchmark Complete. System Overhead: ");

    // 1. Calculate Real Hardware Swap Cost
    double swap_time = measure_hardware_swap_linux();
    printf("%.6f units\n", swap_time);

    // 2. FIXED INPUT GENERATION (Matching hospital scenario)
    // We manually assign the exact values for Arrival Time (AT) and Burst Time (BT)
    p[0].id = 1; p[0].at = 1.0; p[0].bt = 6.0;  // Life-Critical Alarm (Cardiac Arrest)
    p[1].id = 2; p[1].at = 1.0; p[1].bt = 2.0;  // Ventilator Monitor
    p[2].id = 3; p[2].at = 3.0; p[2].bt = 2.0;  // SpO2 Sensor
    p[3].id = 4; p[3].at = 5.0; p[3].bt = 9.0;  // Data Logging (Vital Signs)
    p[4].id = 5; p[4].at = 4.0; p[4].bt = 3.0;  // General Monitoring (Heart Rate)
    p[5].id = 6; p[5].at = 1.0; p[5].bt = 5.0;  // ICU Light Sensor
    p[6].id = 7; p[6].at = 2.0; p[6].bt = 3.0;  // Life-Critical Alarm (SpO2 Low)
    p[7].id = 8; p[7].at = 5.0; p[7].bt = 7.0;  // ECG Monitoring
    p[8].id = 9; p[8].at = 4.0; p[8].bt = 8.0;  // Blood Pressure Monitoring
    p[9].id = 10; p[9].at = 3.0; p[9].bt = 6.0; // Oxygen Level Monitoring


    /* --- MEASURE SCHEDULING LATENCY --- */
    struct timespec sched_start, sched_end;
    clock_gettime(CLOCK_MONOTONIC, &sched_start);

    /* FCFS Logic: Sort by Arrival Time */
    // Note: The sort will handle the patient correctly based on AT
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - i - 1; j++)
            if (p[j].at > p[j + 1].at) {
                struct Patient t = p[j];
                p[j] = p[j + 1];
                p[j + 1] = t;
            }

    clock_gettime(CLOCK_MONOTONIC, &sched_end);
    double scheduling_latency_sec = (sched_end.tv_sec - sched_start.tv_sec) +
                                    (sched_end.tv_nsec - sched_start.tv_nsec) / 1e9;
    /* ---------------------------------- */


    /* --- START EXECUTION TIMER --- */
    struct timespec exec_start, exec_end;
    clock_gettime(CLOCK_MONOTONIC, &exec_start);

    double current_time = 0;
    double total_wt = 0, total_tat = 0, total_rt = 0, total_bt = 0;
    double max_wt = 0, min_wt = 1e9, max_tat = 0, min_tat = 1e9;

    /* ================= FIXED TABLE ================= */
    printf("\nICU Patient Monitoring Table (FCFS)\n");
    printf("FCFS ICU Scheduling Simulation Completed.\n");
    printf("==============================================\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| P_ID |   AT  |   BT  |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    for (int i = 0; i < n; i++) {
        // CPU Idle check
        if (current_time < p[i].at)
            current_time = p[i].at;

        // --- SWAPPING LOGIC ---
        // If patient waits > 5 units, simulate context switch overhead
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
               p[i].id, p[i].at, p[i].bt,
               p[i].st, p[i].ft, p[i].wt, p[i].tat, p[i].rt);
    }

    printf("+------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    /* --- STOP EXECUTION TIMER --- */
    clock_gettime(CLOCK_MONOTONIC, &exec_end);
    double exec_time_sec = (exec_end.tv_sec - exec_start.tv_sec) +
                           (exec_end.tv_nsec - exec_start.tv_nsec) / 1e9;

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

    printf("\nThank you for using the Hospital ICU Scheduling Simulator!\n");

    return 0;
}
          
