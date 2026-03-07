#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

#define ITERATIONS 2000000

// --- Shared Hospital Resources ---
volatile int beds_available = 0;       // Total treated patients / beds updated
volatile int total_switches = 0;       // Staff context switches
double staff_overhead = 0;             // Simulated delay per switch

// --- CPU Helpers (Hospital System Load) ---
typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
} CpuStats;

void GetCpuStats(CpuStats *stats) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), fp)) {
        sscanf(buffer, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
               &stats->user, &stats->nice, &stats->system,
               &stats->idle, &stats->iowait, &stats->irq,
               &stats->softirq, &stats->steal);
    }
    fclose(fp);
}

double CalculateCpuUsage(CpuStats *start, CpuStats *end) {
    unsigned long long prevIdle = start->idle + start->iowait;
    unsigned long long currIdle = end->idle + end->iowait;
    unsigned long long prevNonIdle = start->user + start->nice + start->system + start->irq + start->softirq + start->steal;
    unsigned long long currNonIdle = end->user + end->nice + end->system + end->irq + end->softirq + end->steal;
    unsigned long long totalDelta = (currIdle + currNonIdle) - (prevIdle + prevNonIdle);
    unsigned long long idleDelta = currIdle - prevIdle;
    return totalDelta == 0 ? 0.0 : (double)(totalDelta - idleDelta) / totalDelta * 100.0;
}

// --- Staff Overhead Simulation ---
double measure_staff_overhead() {
    struct timespec s, e;
    char buffer[1024 * 1024];
    memset(buffer, 0, sizeof(buffer));

    FILE *fp = fopen("hospital_test.bin", "wb");
    if (!fp) return 0.000001;

    clock_gettime(CLOCK_MONOTONIC, &s);
    for (int i = 0; i < 5; i++) fwrite(buffer, 1, sizeof(buffer), fp);
    fflush(fp); fclose(fp);
    clock_gettime(CLOCK_MONOTONIC, &e);
    remove("hospital_test.bin");

    double disk_time = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec)/1e9;
    double disk_speed = 5.0 / disk_time;

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    double mem_usage = usage.ru_maxrss / 1024.0;

    clock_gettime(CLOCK_MONOTONIC, &s);
    fp = fopen("lat.txt", "w"); if (fp) { fputc('A', fp); fclose(fp); }
    clock_gettime(CLOCK_MONOTONIC, &e);
    remove("lat.txt");

    double latency = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec)/1e9;

    double result = 2 * (latency + (mem_usage / disk_speed));
    return (result * 1000) > 1.0 ? (result * 1000) : 1.5;
}

// --- Staff Thread (Patient Treatment) ---
void* TreatPatients(void* arg) {
    int delay_iters = (int)(staff_overhead * 1000);
    for (int i = 0; i < ITERATIONS; i++) {
        // CRITICAL SECTION: two staff updating beds simultaneously
        int temp = beds_available;

        // Simulate staff switching between patients
        if (i % 50000 == 0) {
            __sync_fetch_and_add(&total_switches, 1);
            volatile int dummy = 0;
            for(int k = 0; k < delay_iters; k++) dummy++;
        }

        beds_available = temp + 1; // Treat a patient / occupy a bed
    }
    return NULL;
}

int main() {
    pthread_t staff1, staff2;
    CpuStats cpuStart, cpuEnd;
    struct timespec start_time, end_time;

    printf("Hospital ICU Process Synchronization Simulation (Linux)\n");
    printf("Simulating Multiple Staff Updating Patient Treatments...\n");

    // Measure hospital overhead
    staff_overhead = measure_staff_overhead();

    // Start timers & snapshot system load
    GetCpuStats(&cpuStart);
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Launch two staff threads treating patients concurrently
    pthread_create(&staff1, NULL, TreatPatients, NULL);
    pthread_create(&staff2, NULL, TreatPatients, NULL);

    pthread_join(staff1, NULL);
    pthread_join(staff2, NULL);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    GetCpuStats(&cpuEnd);

    // Metrics
    double exec_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec)/1e9;
    double cpu_util = CalculateCpuUsage(&cpuStart, &cpuEnd);

    int expected = ITERATIONS * 2;
    int actual = beds_available;
    int lost = expected - actual;

    printf("\n===========================================================\n");
    printf("           HOSPITAL ICU SYNCHRONIZATION REPORT             \n");
    printf("===========================================================\n");

    printf("\nPatient Treatment Status:\n");
    printf("---------------------------------------\n");
    printf("Expected Treatments       : %d\n", expected);
    printf("Actual Treatments         : %d\n", actual);
    if (lost > 0) {
        printf("Status                    : [CRITICAL FAILURE]\n");
        printf("Error Type                : Race Condition Detected\n");
        printf("Lost Updates              : %d (Patients missed / data lost)\n", lost);
    } else {
        printf("Status                    : [SUCCESS]\n");
        printf("Error Type                : None\n");
    }

    printf("\nStaff Switching Metrics (Real Hardware Data):\n");
    printf("---------------------------------------\n");
    printf("Staff Overhead (Benchmark)  : %.6f units\n", staff_overhead);
    printf("Context Switch Loops         : %d\n", total_switches);
    printf("Simulated Overhead           : %.6f units\n", total_switches * staff_overhead);

    printf("\nReal-Time ICU Performance Metrics:\n");
    printf("---------------------------------------\n");
    printf("Execution Time               : %.6f seconds\n", exec_time);
    printf("CPU Utilization              : %.2f%% (System-Wide Load)\n", cpu_util);
    printf("Treatments Per Second        : %.2f ops/sec\n", expected / exec_time);
    printf("===========================================================\n");

    return 0;
}

