#include "Timer.h"
#include "Tree.h"
// #include "zipf.h"                // Sherman的zipf数据生成器
#include "ScrambledZipfGenerator.h" // Scalestore的zipf数据生成器

#include <city.h>
#include <cstdio>
#include <stdlib.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>


//////////////////// workload parameters /////////////////////

// #define USE_CORO
const int kCoroCnt = 3;

int kReadRatio;
int kThreadCount;
int kNodeCount;
uint64_t kKeySpace = 3 * define::MB;
double kWarmRatio = 0.8;
double zipfan = 0.99;

//////////////////// workload parameters /////////////////////


extern uint64_t cache_miss[MAX_APP_THREAD][8];
extern uint64_t cache_hit[MAX_APP_THREAD][8];

extern uint64_t value_cache_miss[MAX_APP_THREAD][8];
extern uint64_t value_cache_hit[MAX_APP_THREAD][8];

std::thread th[MAX_APP_THREAD];
uint64_t tp[MAX_APP_THREAD][8];

extern uint64_t latency[MAX_APP_THREAD][LATENCY_WINDOWS];
uint64_t latency_th_all[LATENCY_WINDOWS];

Tree *tree;
DSM *dsm;

inline Key to_key(uint64_t k) {
  return (CityHash64((char *)&k, sizeof(k)) + 1) % kKeySpace;
}

Timer bench_timer;
std::atomic<int64_t> warmup_cnt{0};
std::atomic_bool ready{false};
void thread_run(int id) {

 
  dsm->registerThread();

  uint64_t all_thread = kThreadCount * dsm->getClusterSize();
  uint64_t my_id = kThreadCount * dsm->getMyNodeID() + id;
  bindCore(my_id);
  printf("I am thread %ld on compute nodes\n", my_id);

  if (id == 0) {
    bench_timer.begin();
  }

  uint64_t end_warm_key = kWarmRatio * kKeySpace;
  for (uint64_t i = 1; i < end_warm_key; ++i) {
    if (i % all_thread == my_id) {
      tree->insert(to_key(i), i * 2);
    }
  }

  warmup_cnt.fetch_add(1);

  if (id == 0) {
    while (warmup_cnt.load() != kThreadCount)
      ;
    printf("node %d finish\n", dsm->getMyNodeID());
    dsm->barrier("warm_finish");

    uint64_t ns = bench_timer.end();
    printf("warmup time %lds\n", ns / 1000 / 1000 / 1000);

    tree->index_cache_statistics();
    tree->clear_statistics();

    ready = true;

    warmup_cnt.store(0);
  }

  while (warmup_cnt.load() != 0)
    ;

#ifdef USE_CORO
  tree->run_coroutine(coro_func, id, kCoroCnt);
#else

  /// without coro
  unsigned int seed = rdtsc();
  
  ScrambledZipfGenerator zipf_random(0, kKeySpace,zipfan);
  

  Timer timer;
  timespec s, e;
  clock_gettime(CLOCK_REALTIME, &s);
  while (true) {
    uint64_t dis = zipf_random.rand();
  
    uint64_t key = to_key(dis);

    Value v;
    timer.begin();

 
    clock_gettime(CLOCK_REALTIME, &e);
    int timex = (e.tv_sec - s.tv_sec);
    if(timex < 60) {
      kReadRatio = 50;
    }else if(timex >= 60 && timex < 120) {
      kReadRatio = 95;
    }else if(timex >= 120 && timex < 180) {
      kReadRatio = 100;
    }

    if (rand_r(&seed) % 100 < kReadRatio) { // GET
      tree->search_test(key, v);
    } else {
      v = 12;
      tree->insert_test(key, v);
    }

    auto us_10 = timer.end() / 100;
    if (us_10 >= LATENCY_WINDOWS) {
      us_10 = LATENCY_WINDOWS - 1;
    }
    latency[id][us_10]++;

    tp[id][0]++;

  }
#endif

}
void threadrun_task(){
  bindCore(11 - 1 * dsm->getMyNodeID() - kNodeCount );
  dsm->registerThread();
  tree->ThreadTask();
}

void parse_args(int argc, char *argv[]) {
  if (argc != 4) {
    printf("Usage: ./benchmark kNodeCount kReadRatio kThreadCount\n");
    exit(-1);
  }

  kNodeCount = atoi(argv[1]);
  kReadRatio = atoi(argv[2]);
  kThreadCount = atoi(argv[3]);

  printf("kNodeCount %d, kReadRatio %d, kThreadCount %d\n", kNodeCount,
         kReadRatio, kThreadCount);
}

void cal_latency() {
  uint64_t all_lat = 0;
  for (int i = 0; i < LATENCY_WINDOWS; ++i) {
    latency_th_all[i] = 0;
    for (int k = 0; k < MAX_APP_THREAD; ++k) {
      latency_th_all[i] += latency[k][i];
    }
    all_lat += latency_th_all[i];
  }

  uint64_t th50 = all_lat / 2;
  uint64_t th90 = all_lat * 9 / 10;
  uint64_t th95 = all_lat * 95 / 100;
  uint64_t th99 = all_lat * 99 / 100;
  uint64_t th999 = all_lat * 999 / 1000;

  uint64_t cum = 0;
  for (int i = 0; i < LATENCY_WINDOWS; ++i) {
    cum += latency_th_all[i];

    if (cum >= th50) {
      printf("p50 %f\t", i / 10.0);
      th50 = -1;
    }
    if (cum >= th90) {
      printf("p90 %f\t", i / 10.0);
      th90 = -1;
    }
    if (cum >= th95) {
      printf("p95 %f\t", i / 10.0);
      th95 = -1;
    }
    if (cum >= th99) {
      printf("p99 %f\t", i / 10.0);
      th99 = -1;
    }
    if (cum >= th999) {
      printf("p999 %f\n", i / 10.0);
      th999 = -1;
      return;
    }
  }
}

int singleThreadCorrectnessTest(int argc, char *argv[]) {
  parse_args(argc, argv);

  DSMConfig config;
  config.machineNR = kNodeCount;
  dsm = DSM::getInstance(config);

  dsm->registerThread();
  tree = new Tree(dsm);

  if (dsm->getMyNodeID() == 0) {

    for (uint64_t i = 1; i < 1024000; ++i) {
      tree->insert(i, i * 2);
    }

    for (uint64_t i = 1; i < 1024000; ++i) {
      Value v;
      tree->search(i, v);
      if (v != i * 2) {
        std::cout << "i:" << i << "  v:" << v << std::endl;
      }
      assert(v == i * 2);
    }
  }

  dsm->barrier("benchmark");
  dsm->resetThread();

  return 0;
}

int main(int argc, char *argv[]) {
  // singleThreadCorrectnessTest(argc, argv);
  parse_args(argc, argv);

  DSMConfig config;
  config.machineNR = kNodeCount;
  dsm = DSM::getInstance(config);

  dsm->registerThread();
  tree = new Tree(dsm);

  if (dsm->getMyNodeID() == 0) {
    for (uint64_t i = 1; i < 1000240; ++i) {
      tree->insert(to_key(i), i * 2);
    }
  }

  dsm->barrier("benchmark");
  dsm->resetThread();

  for (int i = 0; i < kThreadCount; i++) {
    th[i] = std::thread(thread_run, i);
  }
 
  std::thread task(threadrun_task); 

  while (!ready.load())
    ;

  timespec s, e;
  uint64_t pre_tp = 0;

  int count = 0;

  clock_gettime(CLOCK_REALTIME, &s);
  while (true) {

    sleep(2);
    clock_gettime(CLOCK_REALTIME, &e);
    int microseconds = (e.tv_sec - s.tv_sec) * 1000000 +
                       (double)(e.tv_nsec - s.tv_nsec) / 1000;

    uint64_t all_tp = 0;
    for (int i = 0; i < kThreadCount; ++i) {
      all_tp += tp[i][0];
    }
    uint64_t cap = all_tp - pre_tp;
    pre_tp = all_tp;

    uint64_t all = 0;
    uint64_t hit = 0;
    uint64_t miss = 0;

    uint64_t value_all = 0;
    uint64_t value_hit = 0;
    uint64_t value_miss = 0;

    for (int i = 0; i < MAX_APP_THREAD; ++i) {
      all += (cache_hit[i][0] + cache_miss[i][0]);
      hit += cache_hit[i][0];
      miss += cache_miss[i][0];
      value_all += (value_cache_hit[i][0] + value_cache_miss[i][0]);
      value_hit += value_cache_hit[i][0];
      value_miss += value_cache_miss[i][0];
    }

    clock_gettime(CLOCK_REALTIME, &s);

    if (++count % 10 == 0 && dsm->getMyNodeID() == 0) {
      cal_latency();
 
    }

    double per_node_tp = cap * 1.0 / microseconds;
    uint64_t cluster_tp = dsm->sum((uint64_t)(per_node_tp * 10000));

    printf("%d, throughput %.4f\n", dsm->getMyNodeID(), per_node_tp);

    if (dsm->getMyNodeID() == 0) {
      printf("cluster throughput %.4f\n", cluster_tp / 10000.0);
      printf("index_cache hit rate: %lf\n", hit * 1.0 / all);
      printf("index_cache miss rate: %lf\n", miss * 1.0 / all);
      printf("value_cache hit rate: %lf\n", value_hit * 1.0 / value_all);
      printf("value_cache miss rate: %lf\n", value_miss * 1.0 / value_all);

    }
  }

  return 0;
}