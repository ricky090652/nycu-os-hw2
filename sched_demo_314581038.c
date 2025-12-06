#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

typedef struct{
    int id;
    int sched_policies;
    int sched_priorities;
    double wait_time;
    pthread_barrier_t *barrier;
    cpu_set_t cpuset;
}thread_arg_t;


void set_thread_attr(pthread_attr_t *attr, int policy, int priority, cpu_set_t *cpuset) {\
    //printf("Setting thread attribute: policy=%d, priority=%d\n", policy, priority);
    pthread_attr_init(attr);
    pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED); // 使用自訂排程屬性
    pthread_attr_setschedpolicy(attr, policy);

    struct sched_param param = {0};//sched_parmam結構體用來設定priority，只有一個sched_priority成員，只有SCHED_RR或SCHED_FIFO才有效，其他為0
    param.sched_priority = priority;
    pthread_attr_setschedparam(attr, &param);

    // 設定 CPU affinity 到指定 CPU
    pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), cpuset);
}


static inline double get_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void *thread_func(void *arg){
    //printf("Thread function started\n");
    thread_arg_t *targ = (thread_arg_t *)arg;

    // 等所有thread一起啟動
    pthread_barrier_wait(targ->barrier);

    for (int i = 0; i < 3; i++) {
        printf("Thread %d is running\n", targ->id);

        double start = get_time_sec();
        while (get_time_sec() - start < targ->wait_time) {
            // busywait不能用sleep,要持續讓cpu跑
        }
    }
    return NULL;
}



void parse_args(int argc, char *argv[], int *num_threads, double *time_wait, char ***policy_array, char ***priority_array){//main是傳二位陣列的地址過來，二為陣列是**，所以在接收要用指標來接要再多一個*
    int opt;
    char *policy_str = NULL;
    char *priority_str = NULL;
    const char *optstring = "n:t:s:p:";

    while((opt = getopt(argc, argv, optstring)) != -1){
        switch(opt){
            case 'n':
                *num_threads = atoi(optarg); //optarg為char *型態
                break;
            case 't':
                *time_wait = atof(optarg);
                break;
            case 's':
                policy_str = optarg;//先用policy_str字元陣列來接optarg
                break;     
            case 'p': 
                priority_str = optarg;//先用priority_str字元陣列來接optarg
                break;
            case '?':
                printf("error optopt: %c\n", optopt);
                printf("unknown option: %c\n",opt);
                break;
                
        }
    }

    *policy_array = malloc(*num_threads * sizeof(char *));
    *priority_array = malloc(*num_threads * sizeof(char *));

    char *rest_policy = NULL;//for strokt_r to store the tmp pointer
    char *rest_priority = NULL;//for strokt_r to store the tmp pointer
    char *token_policy;
    char *token_priority;
    int i = 0;

    //strtok_r 分割字串，可以用在多線程環境中
    token_policy = strtok_r(policy_str, ",", &rest_policy); 
    token_priority = strtok_r(priority_str, ",", &rest_priority);

    while (token_policy != NULL && token_priority != NULL && i < *num_threads) {
        (*policy_array)[i] = token_policy;
        (*priority_array)[i] = token_priority;
        i++;

        token_policy = strtok_r(NULL, ",", &rest_policy);
        token_priority = strtok_r(NULL, ",", &rest_priority);
    }
}


int main(int argc, char *argv[]){

    int num_threads;
    double time_wait;
    char **policy; //二維陣列
    char **priority;//二維陣列

    parse_args(argc, argv, &num_threads, &time_wait, &policy, &priority);

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, num_threads);

    // 對 CPU 0 設定 affinity（所有 thread 綁 CPU0）
    cpu_set_t cpuset;//用一個 cpu_set_t 型態來產生一CPU集合
    CPU_ZERO(&cpuset);//清空給定變數cpuset 中的CPU set
    CPU_SET(0, &cpuset);//將CPU 0加進給定變數cpuset集合中

    // 動態分配thread參數與pthread_t陣列
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    thread_arg_t *args = malloc(num_threads * sizeof(thread_arg_t));

    for (int i = 0; i < num_threads; i++) {
        //printf("Creating thread %d with policy %s and priority %s\n", i, policy[i], priority[i]);
        args[i].id = i;
        args[i].wait_time = time_wait;
        args[i].barrier = &barrier;
        args[i].cpuset = cpuset;

        // 設定排程策略與優先權
        if (strcmp(policy[i], "FIFO") == 0) {
            args[i].sched_policies = SCHED_FIFO;
            args[i].sched_priorities = atoi(priority[i]);
        } else {
            args[i].sched_policies = SCHED_OTHER; // SCHED_NORMAL的POSIX 名稱
            args[i].sched_priorities = 0;         // NORMAL的priority為0
        }

        // 設定屬性並建立thread
        pthread_attr_t attr;
        set_thread_attr(&attr, args[i].sched_policies, args[i].sched_priorities, &cpuset);
        int ret = pthread_create(&threads[i], &attr, thread_func, &args[i]);
        if(ret != 0) {
            printf("Thread %d create error: %d\n", i, ret); // 回傳值可以判斷出了什麼錯誤
        }
        pthread_attr_destroy(&attr);
    }

    // 等所有thread結束
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&barrier);
    free(threads);
    free(args);

    // free policies, priorities 由 parse_args 管理
    return 0;
}