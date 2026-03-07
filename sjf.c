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
    int is_completed; // Flag for SJF logic
};

/* ================= REAL WINDOWS SWAP HEURISTIC ================= */
// Measures Disk I/O and RAM usage to calculate a realistic penalty
double measure_hardware_swap_windows() {
    LARGE_INTEGER freq, s, e;
    QueryPerformanceFrequency(&freq);

    /* 1. Disk latency test */
    FILE *fp = fopen("disk_test.bin", "wb");
    if (!fp) return 2.0;

    char buffer[1024 * 1024]; // 1MB Buffer
    memset(buffer, 0, sizeof(buffer));

    QueryPerformanceCounter(&s);
    for (int i = 0; i < 5; i++)
        fwrite(buffer, 1, sizeof(buffer), fp); // Write 5MB
    fflush(fp);
    fclose(fp);
    QueryPerformanceCounter(&e);
    DeleteFileA("disk_test.bin");

    double disk_time = (double)(e.QuadPart - s.QuadPart) / freq.QuadPart;
    double disk_speed = 5.0 / disk_time;   // MB/sec

    /* 2. Process memory usage */
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    double mem_usage = pmc.WorkingSetSize / (1024.0 * 1024.0); // MB

    /* 3. File open latency (Simulate OS overhead) */
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
    int n = 10; // Fixed to 10 ICU patients
    struct Patient p[10];
    int total_swaps = 0;

    printf("HOSPITAL ICU PATIENT MONITORING SYSTEM - SJF Scheduler (Windows Real-Time)\n");
    printf("Configuring System...\n");
    
    // 1. Calculate Real Hardware Swap Cost
    double swap_time = measure_hardware_swap_windows();
    printf("Hardware Benchmark Complete. Swap Penalty: %.6f units\n", swap_time);

    /* 2. Predefined Patient Input (matches ICU scenario) */
    double at_values[10] = {4,2,4,5,3,5,4,1,4,2}; // Arrival times
    double bt_values[10] = {2,4,8,9,9,3,8,8,9,8};   // Treatment durations

    for (int i = 0; i < n; i++) {
        p[i].id = i + 1;
        p[i].at = at_values[i];
        p[i].bt = bt_values[i];
        p[i].is_completed = 0;
    }

    /* --- START EXECUTION TIMER --- */
    LARGE_INTEGER freq, exec_start, exec_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&exec_start);

    double current_time = 0;
    double total_wt = 0, total_tat = 0, total_rt = 0, total_bt = 0;
    double max_wt = 0, min_wt = 1e9, max_tat = 0, min_tat = 1e9;
    int completed = 0;

    // Variables for Scheduling Latency Measurement
    LARGE_INTEGER sched_s, sched_e;
    double total_sched_latency_sec = 0;

    // To store execution sequence of patients for printing later
    struct Patient *execution_patient[10];
    int exec_idx = 0;

    /* ================= SJF LOGIC (Shortest Treatment First for patients) ================= */
    
    while (completed < n) {
        int idx = -1;
        double min_bt = 1e9;

        // --- MEASURE DECISION LATENCY FOR PATIENT SELECTION ---
        QueryPerformanceCounter(&sched_s);
        
        // Find the patient with the shortest treatment time that has arrived
        for (int i = 0; i < n; i++) {
            if (p[i].at <= current_time && p[i].is_completed == 0) {
                if (p[i].bt < min_bt) {
                    min_bt = p[i].bt;
                    idx = i;
                }
                // Tie-Breaker: FCFS (Arrival Time) for patients
                if (p[i].bt == min_bt) {
                    if (p[i].at < p[idx].at) {
                        idx = i;
                    }
                }
            }
        }
        
        QueryPerformanceCounter(&sched_e);
        total_sched_latency_sec += (double)(sched_e.QuadPart - sched_s.QuadPart) / freq.QuadPart;
        // --------------------------------

        if (idx != -1) {
            // Found a patient to treat
            
            // --- SWAPPING LOGIC (simulate patient waiting overhead) ---
            if ((current_time - p[idx].at) > 5) {
                current_time += swap_time;
                total_swaps++;
            }

            p[idx].st = current_time;
            p[idx].rt = p[idx].st - p[idx].at;
            
            current_time += p[idx].bt;
            p[idx].ft = current_time;
            
            p[idx].tat = p[idx].ft - p[idx].at;
            p[idx].wt = p[idx].tat - p[idx].bt;

            total_wt += p[idx].wt;
            total_tat += p[idx].tat;
            total_rt += p[idx].rt;
            total_bt += p[idx].bt;

            if (p[idx].wt > max_wt) max_wt = p[idx].wt;
            if (p[idx].wt < min_wt) min_wt = p[idx].wt;
            if (p[idx].tat > max_tat) max_tat = p[idx].tat;
            if (p[idx].tat < min_tat) min_tat = p[idx].tat;

            p[idx].is_completed = 1;
            completed++;
            
            execution_patient[exec_idx++] = &p[idx];
        } else {
            // CPU Idle (No patient arrived yet)
            current_time++;
        }
    }

    /* --- STOP EXECUTION TIMER --- */
    QueryPerformanceCounter(&exec_end);
    double exec_time_sec = (double)(exec_end.QuadPart - exec_start.QuadPart) / freq.QuadPart;

    /* ================= FIXED TABLE FOR PATIENT EXECUTION ================= */
    printf("\nCalculation Table (Execution Sequence of Patients)\n");
    printf("+-------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");
    printf("| P_ID  |   AT  |   BT  |  Start    |  Finish   |   WT      |   TAT     |   RT      |\n");
    printf("+-------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    // Print patients in the arrival they were treated
    for (int i = 0; i < n; i++) {
        struct Patient *o = execution_patient[i];
        printf("| P%02d   | %-5.1f | %-5.1f | %-9.2f | %-9.2f | %-9.2f | %-9.2f | %-9.2f |\n",
               o->id, o->at, o->bt,
               o->st, o->ft, o->wt, o->tat, o->rt);
    }

    printf("+-------+-------+-------+-----------+-----------+-----------+-----------+-----------+\n");

    /* -------- PATIENT PERFORMANCE METRICS -------- */
    printf("\nPerformance Metrics:\n");
    printf("=================================\n");
    printf("Average Waiting Time       : %.2f units\n", total_wt / n);
    printf("Average Turnaround Time    : %.2f units\n", total_tat / n);
    printf("Average Response Time      : %.2f units\n", total_rt / n);
    printf("Maximum Waiting Time       : %.0f units\n", max_wt);
    printf("Minimum Waiting Time       : %.0f units\n", min_wt);
    printf("Maximum Turnaround Time    : %.0f units\n", max_tat);
    printf("Minimum Turnaround Time    : %.0f units\n", min_tat);
    // Throughput: Patients / Total Execution Time (Finish time of last patient)
    printf("Throughput                 : %.4f patients/unit\n", (double)n / current_time);
    printf("CPU Utilization            : %.0f%%\n", (total_bt / current_time) * 100);

    printf("\nSwapping Metrics:\n");
    printf("=================================\n");
    printf("Swap Time (Hardware Calc)  : %.6f units\n", swap_time);
    printf("Total Swapped Patients     : %d\n", total_swaps);
    printf("Total Swapping Overhead    : %.6f units\n", total_swaps * swap_time);

    printf("\nReal-Time Execution Metrics:\n");
    printf("=================================\n");
    printf("Program Execution Time     : %.9f seconds\n", exec_time_sec);
    // Real accumulated time spent in the SJF decision loop
    printf("Scheduling Latency         : %.9f seconds\n", total_sched_latency_sec); 
    printf("Average Patient Latency    : %.2f units\n", total_wt / n);
    printf("Total Patient Latency      : %.2f units\n", total_wt);
    printf("Worst-Case Patient Latency : %.0f units\n", max_wt);

    return 0;
}

