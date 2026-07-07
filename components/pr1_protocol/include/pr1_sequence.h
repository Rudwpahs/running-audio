#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PR1_SEQUENCE_WINDOW_BITS 64u

typedef struct {
    bool initialized;
    uint32_t highest_sequence;
    uint64_t seen_bitmap;
} pr1_sequence_tracker_t;

typedef enum {
    PR1_SEQUENCE_FIRST = 0,
    PR1_SEQUENCE_IN_ORDER,
    PR1_SEQUENCE_GAP,
    PR1_SEQUENCE_DUPLICATE,
    PR1_SEQUENCE_OUT_OF_ORDER,
    PR1_SEQUENCE_TOO_OLD,
} pr1_sequence_class_t;

typedef struct {
    pr1_sequence_class_t classification;
    uint32_t missing_before;
} pr1_sequence_result_t;

void pr1_sequence_tracker_reset(pr1_sequence_tracker_t *tracker);
pr1_sequence_result_t pr1_sequence_tracker_observe(pr1_sequence_tracker_t *tracker,
                                                    uint32_t sequence);

#ifdef __cplusplus
}
#endif
