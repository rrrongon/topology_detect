/*from each core to node distance*/
#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU
#include <sched.h>
#include <errno.h>
#include <unistd.h>


#include <time.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <immintrin.h>
#include <x86intrin.h>
#include <inttypes.h>

#ifdef _WIN32
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include <fcntl.h>
#include <numaif.h>
#include <numa.h>
#include <sys/time.h>

typedef struct numa_node_info
{
    size_t core_cnt;
    int numa_node;
    int *cores;
    
} numa_t;


long long int print_duration(struct timespec *b, struct timespec *c)
{
	long long r = c->tv_nsec - b->tv_nsec;
        r += ((long long)(c->tv_sec - b->tv_sec) ) * 1000000000;
	return r;
}

void pin_to_core(size_t core, pthread_t t_id)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

void print_lat(int *dis_lat, int numberOfProcessors){
        int i,j;
        for(i=0; i<numberOfProcessors;i++){
                printf("%d\t",i);
        }
        printf("\n");
        int pos;
        for(i=0;i<1;i++){
                printf("Core %d:\n", i);
                for(j=0; j< numberOfProcessors; j++){
                        pos = (i*numberOfProcessors) + j;
                        printf("%d:%d, ",j, *(dis_lat+pos));
                }
                printf("\n\n\n");
        }
}

void node_distance(int numa_cnt, int *numa_dist_matrix){
	int src_numa, target_numa, pos, numa_count, numa_dist;
	numa_count = numa_cnt;
	for(src_numa=0; src_numa < numa_count; src_numa++){
		for(target_numa=0; target_numa< numa_count; target_numa++){
			pos = (src_numa * numa_count) + target_numa;
			numa_dist = numa_distance(src_numa, target_numa);
			printf("src: %d, dest: %d: %d\n", src_numa, target_numa, numa_dist);
			*(numa_dist_matrix + pos)= numa_dist;
		}
	}
}

void print_numa_distance(int numa_cnt, int *numa_dist_matrix){
	int src_numa, target_numa, pos, numa_count, numa_dist;
        numa_count = numa_cnt;
        for(src_numa=0; src_numa < numa_count; src_numa++){
		printf("From numa %d: ", src_numa);
                for(target_numa=0; target_numa<numa_count; target_numa++){
                        pos = (src_numa * numa_count) + target_numa;
                        numa_dist = *(numa_dist_matrix + pos);
			printf(" %d: %d",target_numa, numa_dist);
                }
		printf("\n");
        }
}

void print_numa_bitmask(struct bitmask *bm)
{
        size_t i; 
    for(i=0;i<bm->size;++i)
    {
        printf("%d ", numa_bitmask_isbitset(bm, i));
    }
}

int count_cpu_onnode(struct bitmask *bm)
{
        size_t i;
	int cnt=0;
    for(i=0;i<bm->size;++i)
    {
         if(numa_bitmask_isbitset(bm, i))
		 cnt++;
    }
    return cnt;
}

void get_cores_onnode(int *cores_onnode, int cnt, struct bitmask *bm){
	size_t i;
        cnt=0;
    for(i=0;i<bm->size;++i){
         if(numa_bitmask_isbitset(bm, i)){
                 *(cores_onnode+cnt) = i;
		 cnt++;
	}
    }
}

void show_numa_infos(numa_t *numa_infos, int numa_cnt){
	int i,j;
	for(i=0;i< numa_cnt; i++){
		printf("numa node: %d, core_cnt: %d\n", numa_infos[i].numa_node, numa_infos[i].core_cnt);
		int * cores = numa_infos[i].cores;
		for(j=0; j<numa_infos[i].core_cnt; j++){
			printf("%d, ", cores[j]);
		}
		printf("\n");
	}
}

int main(int argc, char *argv[]) {
        int i,j,k;
        int numberOfProcessors =sysconf(_SC_NPROCESSORS_ONLN);
        printf("Number of process in the machine: %d\n", numberOfProcessors);

	printf("numa distance: 0-1: %d\n", numa_distance(0,1));

        int numcpus, numa_cnt;
        numcpus = numa_num_configured_cpus();
	numa_cnt = numa_max_node() + 1;

	
	numa_t * numa_infos = (numa_t *) malloc(numa_cnt* sizeof(numa_t));


	int *numa_dist_matrix, cpu_cnt_onnode;
	numa_dist_matrix = (int*)malloc(numa_cnt * numa_cnt);
	node_distance(numa_cnt, numa_dist_matrix);

	print_numa_distance(numa_cnt, numa_dist_matrix);

        struct bitmask* bm = numa_bitmask_alloc(numcpus);
        printf("Max number of NUMA nodes %d\n", numa_max_node());
        for (i=0;i<=numa_max_node();++i){
                numa_node_to_cpus(i, bm);
                printf("Numa nodes %d CPU status:",i);
                print_numa_bitmask(bm);
		cpu_cnt_onnode = count_cpu_onnode(bm);
		int * cores_onnode = (int *)malloc(cpu_cnt_onnode* sizeof(int));
		get_cores_onnode(cores_onnode,cpu_cnt_onnode,bm);
		numa_t temp_numa;
		temp_numa.core_cnt = cpu_cnt_onnode;
		temp_numa.numa_node = i;
		temp_numa.cores = cores_onnode;
		numa_infos[i] = temp_numa;
		printf("on node %d, cpu count: %d\n", i, cpu_cnt_onnode);
        }
        numa_bitmask_free(bm);
	

	show_numa_infos(numa_infos, numa_cnt);	


        int slot_cnt = numberOfProcessors * numberOfProcessors;
	int max_numa, pos, lat_pos, run_cnt, run;
        max_numa = numa_max_node() + 1;
	numberOfProcessors = 70;
        long long * MAT = (long long*)malloc(numberOfProcessors * max_numa * sizeof(long long));
        
        return 0;
}

