
#include "config.h"
#include "interface.h"
#include "npy_file.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <random>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// string utils
#define XSTR(s) STR(s)
#define STR(s)  #s

constexpr char const *SHM_ATTACKER = "/shm_attacker";
constexpr char const *SHM_VICTIM   = "/shm_victim";

// amplification factor
// 8 for experimenet 64 for amplification
constexpr uint8_t NUMBER_BYTES = 64;

int fd;

static uint8_t core_controller = 5;

struct thread_id {
    uint8_t  core_id;
    uint64_t pid;
};

// core configuration
static std::array id_attacker = { thread_id { .core_id = 0 } };

static_assert(EXPERIMENT_REPEAT % 2 == 0);

static row_t rows[EXPERIMENT_REPEAT] = {};

// get the shared memory for the keys
uint8_t *create_shm_keys(char const *shm_name) {
    int fd = shm_open(shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if ( fd < 0 ) {
        perror("shm_open");
        return nullptr;
    }

    if ( ftruncate(fd, 0x1000) ) {
        perror("ftruncate");
        return nullptr;
    }

    void *addr = mmap(NULL, 0x1000, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    if ( addr == MAP_FAILED ) {
        perror("mmap");
        return nullptr;
    }

    return (uint8_t *)addr;
}

// invoke the aes victim or attacker
template<typename... Ts>
void fork_exec(thread_id &id, const char *exec, Ts... xs) {
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%d", id.core_id);

    int pid = fork();
    if ( pid < 0 ) {
        perror("fork");
        exit(-1);
    }

    if ( pid == 0 ) {
        execlp(exec, exec, buffer, xs..., nullptr);
        _exit(0);
    }

    id.pid = pid;
}

[[gnu::noinline]] void record_one_sample(measurement_t &result) {
    measure_us(result, 10000);
}

[[gnu::noinline]] void generate_state() {

    struct sysinfo info;

    sysinfo(&info);

    float la = info.loads[0];

    for ( size_t e = 0; e < EXPERIMENT_REPEAT / 2; ++e ) {

        row_t &r = rows[2 * e + 0];
        row_t &i = rows[2 * e + 1];

        r.erep = e;
        i.erep = e;

        r.la = la;
        i.la = la;

        strncpy(r.exp, "pf", sizeof(r.exp));
        strncpy(i.exp, "pf", sizeof(i.exp));

        r.dur = 10000;
        i.dur = 10000;

        // draw nibble
        uint8_t v = rand() & 0xF;
        uint8_t g = rand() & 0xF;

        v |= (v << 4);
        g |= (g << 4);

        // fill the guess and value
        memset(r.value, v, NUMBER_BYTES);
        memset(r.guess, g, NUMBER_BYTES);

        // invert
        g = ~g;

        // copy to inverse sample
        memset(i.value, v, NUMBER_BYTES);
        memset(i.guess, g, NUMBER_BYTES);
    }
}

void print_iteration(uint64_t index) {
    fprintf(stdout, "\r%10zu/%10zu ", index * EXPERIMENT_REPEAT, NUMBER_SAMPLES * EXPERIMENT_REPEAT);
    fflush(stdout);
}

bool check_children() {
    return std::all_of(id_attacker.begin(), id_attacker.end(), [](thread_id const &x) {
        return x.pid != waitpid(x.pid, NULL, WNOHANG);
    });
}

void cleanup_children() {
    printf("stopping children!\n");

    for ( thread_id &tid : id_attacker ) {
        kill(tid.pid, SIGKILL);
        waitpid(tid.pid, nullptr, 0);
    }
}

void signal_handler(int signum) {
    if ( signum == SIGUSR1 ) {
        return;
    }
    cleanup_children();
    exit(0);
}

// pin pthread to specific core
uint8_t pin_to_exact_thread_pthread(pthread_t thread, uint8_t core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    int ret = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
    if ( ret ) {
        printf("setaffinity failed\n");
        exit(-1);
    }
    ret = pthread_getaffinity_np(thread, sizeof(cpuset), &cpuset);
    if ( ret ) {
        printf("getaffinity failed\n");
        exit(-1);
    }
    if ( CPU_ISSET(core, &cpuset) == 0 ) {
        printf("core setting failed\n");
        exit(-1);
    }
    return core;
}

int main(int argc, char *argv[]) {

    if ( argc < 2 ) {
        printf("usage: %s output_file\n", argv[0]);
        return -1;
    }

    auto aes_attacker = create_shm_keys(SHM_ATTACKER);
    auto aes_victim   = create_shm_keys(SHM_VICTIM);

    if ( !aes_attacker || !aes_victim ) {
        return -2;
    }

    // we open exactly our kernel module which is named after the parent's parent folder
    fd = open("/dev/" XSTR(NAME), O_RDWR);
    if ( fd < 0 ) {
        printf("cannot open kernel module is it loaded? forgot sudo? %s\n", "/dev/" XSTR(NAME));
        cleanup_children();
        return -1;
    }

    for ( thread_id &tid : id_attacker ) {
        fork_exec(tid, "../../pf/collide_power");
        printf("attacker: %ld\n", tid.pid);
    }

    usleep(1'500'000);

    if ( !check_children() ) {
        printf("child died during init!\n");
        cleanup_children();
        return -1;
    }

    printf("created processes!\n");

    FILE *log = fopen(argv[1], "w");
    if ( !log ) {
        cleanup_children();
        return -1;
    }

    if ( signal(SIGUSR1, signal_handler) ) {
        printf("sigaction failed!\n");
        cleanup_children();
        return -1;
    }

    if ( signal(SIGINT, signal_handler) ) {
        printf("sigaction failed!\n");
        cleanup_children();
        return -1;
    }

    npy_file numpy = { "", row_t_fields };

    pin_to_exact_thread_pthread(pthread_self(), core_controller);

    numpy.write_header(log);

    for ( uint64_t index = 0; index < NUMBER_SAMPLES; ++index ) {

        print_iteration(index);

        generate_state();

        // repeat random selected variables for REP times
        for ( size_t e = 0; e < EXPERIMENT_REPEAT; ++e ) {
            rows[e].time = time(NULL);

            static_assert(sizeof(rows[e].guess) == 16 * 12);

            memcpy(aes_attacker, rows[e].guess, sizeof(rows[e].guess));
            memcpy(aes_victim, rows[e].value, sizeof(rows[e].value));

            // notify the attacker that new data is available
            for ( thread_id &tid : id_attacker ) {
                kill(tid.pid, SIGUSR1);
            }

            record_one_sample(rows[e].measurements);
        }

        numpy.write_rows(log, (uint8_t *)rows, sizeof(rows), EXPERIMENT_REPEAT);
        fsync(fileno(log));

        if ( !check_children() ) {
            printf("child died during run!\n");
            cleanup_children();
            return -1;
        }
    }

    printf("\n");

    return 0;
}
