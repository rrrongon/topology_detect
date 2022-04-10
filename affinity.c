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
#define _XOPEN_SOURCE
#include <sys/shm.h>


typedef struct thread_info_s
{
    void **x;
    size_t core;
    size_t N;
    size_t M;
    size_t B;
    size_t T;
    int lat;
} thread_info;

typedef struct sorted_lat
{
	int lat;
	int core_no;
}sort_obj;

void print_numa_bitmask(struct bitmask *bm)
{
    for(size_t i=0;i<bm->size;++i)
    {
        printf("%d ", numa_bitmask_isbitset(bm, i));
    }
}

void* thread1(void *arg){
        thread_info *t = (thread_info *) arg;
        size_t N = t->N, M = t->M, core = t->core, T = t->T, B = t->B;
        pin_to_core(core);

	//void* y = numa_alloc_local(N);
        //N = N - (B-1);

	int       shm_id;
	key_t     mem_key;
	void       *shm_ptr;
	mem_key = ftok(".", 'a');
	shm_id = shmget(mem_key, N*sizeof(char), IPC_CREAT | 0666);
	if (shm_id < 0) {
     		printf("*** shmget error (server) ***\n");
     		exit(1);
	}

	shm_ptr = (char *) shmat(shm_id, NULL, 0);  /* attach */
	if ((int) shm_ptr == -1) {
     		printf("*** shmat error (server) ***\n");
		t->lat = -1;
     		pthread_exit(1);    
	}

	void *y ;
	y = shm_ptr;
	int suc;
	int node;
	//printf("core: %d is in %d node\n", core, numa_node_of_cpu(core));
	numa_tonode_memory( y, N, numa_node_of_cpu(core));

	//printf("Core: %d writing at address of %p\n",core, y);
        char c;
	struct timeval t1, t2;
	gettimeofday(&t1, NULL);
        for(size_t j = 0;j<T;++j)
        {
		//*(((char*)y) + j) += 1;
		*(((char*)y) + j ) += 1;
        }
	
	gettimeofday(&t2, NULL);
        t->lat = (((t2.tv_sec - t1.tv_sec)*100000 +
         (t2.tv_usec - t1.tv_usec)));
//	t->lat = 0;
        *(t->x) = y;
	pthread_exit(1);    

}


void* thread2(void *arg)
{
	pthread_t t_id = pthread_self(thread2);

    	thread_info *t = (thread_info *) arg;
    	size_t N = t->N, M = t->M, core = t->core, T = t->T, B = t->B;
    	void *x = *(t->x);

	if(t->lat!=-1){
    	N = N - (B-1);
    	pin_to_core(core);

	//void* y = numa_alloc_local(N);
	int       shm_id;
        key_t     mem_key;
        void       *shm_ptr;
        mem_key = ftok(".", 'c');
        shm_id = shmget(mem_key, N*sizeof(char), IPC_CREAT | 0666);
        if (shm_id < 0) {
                printf("*** shmget error (server) ***\n");
                pthread_exit(1);
        }

        shm_ptr = (char *) shmat(shm_id, NULL, 0);  /* attach */
        if ((int) shm_ptr == -1) {
                printf("*** shmat error (server) ***\n");
                pthread_exit(1);    
        }

        void *y ;
        y = shm_ptr;
        numa_tonode_memory( y, N, numa_node_of_cpu(core));


	struct timeval t1, t2;

//printf("Reading at address of x: %p and local: %p\n",x, y);
	gettimeofday(&t1, NULL);
        for (size_t i = 0;i<M;++i){
                for(size_t j = 0;j<T;++j)
                {
                        //*(((char*)y) + ((j * 1009) % N))  =  *(((char*)x) + ((j * 1009) % N)) + 1; ;
			*(((char*)y) +j )  =  *(((char*)x) + j) + 1; ;
                }
        }

	gettimeofday(&t2, NULL);

         int total_lat= t->lat +  ((t2.tv_sec - t1.tv_sec)*100000 +
         (t2.tv_usec - t1.tv_usec));

	 int avg_lat = total_lat/M;
	 t->lat =avg_lat;
	numa_free(y,N);
	numa_free(x,N);
	pthread_exit(1);
	}else{
		pthread_exit(1);
	}
}

void pin_to_core(size_t core)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

void print_lat(int *dis_lat, int numberOfProcessors){
	for(int i=0; i<numberOfProcessors;i++){
		printf("%d\t",i);
	}
	printf("\n");
	int pos;
	for(int i=0;i<numberOfProcessors;i++){
		printf("Core %d:\n", i);
		for(int j=0; j< numberOfProcessors; j++){
			pos = (i*numberOfProcessors) + j;
			printf("%d: %d  ",j, *(dis_lat+pos));
		}
		printf("\n\n\n");
	}
}

int main(int argc, char *argv[]) {
        int numberOfProcessors =sysconf(_SC_NPROCESSORS_ONLN);
        pthread_t threads[numberOfProcessors];
	printf("Starting detecting topology.\n");
        printf("Number of process in the machine: %d\n", numberOfProcessors);
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        thread_info *tinfo0;

        int numcpus = numa_num_configured_cpus();
        printf("Numa available cpus: %d, numa_available() %d\n",numcpus, numa_available());
        numa_set_localalloc();

        struct bitmask* bm = numa_bitmask_alloc(numcpus);
	printf("Max number of NUMA nodes %d\n", numa_max_node());
        for (int i=0;i<=numa_max_node();++i){
                numa_node_to_cpus(i, bm);
                printf("Numa nodes %d CPU status:",i);
                print_numa_bitmask(bm);
                printf("Memory size: %ld\n",numa_node_size(i, 0));
        }
        numa_bitmask_free(bm);


        tinfo0 = malloc(1 * sizeof(thread_info));
        void* x;
        size_t N = 100000, M = 3, T = 100000, B;

	int slot_cnt = numberOfProcessors * numberOfProcessors;
	int* dist_mat = malloc(slot_cnt * sizeof(int));
	int lat_pos;

	//int procs [24] = {1,2,3,24,25,26,47,48,49, 63,64,152,191,193, 70,71,72, 110,195,198,210,220,250,254};	
        for (int i = 0; i < numberOfProcessors; i++) {
                for(int j=0; j< numberOfProcessors; j++){
                        if(i!=j){
				//int core_j = procs[j];
				tinfo0[0].x = &x;
                                tinfo0[0].core = j;
                                tinfo0[0].N = N;
                                tinfo0[0].M = M;
                                tinfo0[0].B = B;
                                tinfo0[0].T = T;
				tinfo0[0].lat = -1;

                                pthread_create(&threads[j], &attr, thread1, &tinfo0[0]);
                                pthread_join(threads[j], NULL);

                                tinfo0[0].core = i;
                        	pthread_create(&threads[i], &attr, thread2, &tinfo0[0]);
                        	pthread_join(threads[i], NULL);
			
				int latency = tinfo0[0].lat;
				lat_pos = (i* numberOfProcessors) + j;
				dist_mat[lat_pos] = latency;
                        }else{
				lat_pos = (i* numberOfProcessors) + j ;
				dist_mat[lat_pos]  = 0;
			}

                }

}

	print_lat(dist_mat, numberOfProcessors);
        return 0;
}


