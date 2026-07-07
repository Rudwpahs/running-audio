#include "pr1_sequence.h"

#include <stddef.h>

#include "pr1_protocol.h"

void pr1_sequence_tracker_reset(pr1_sequence_tracker_t *tracker)
{
    if (tracker == NULL) {
        return;
    }

    tracker->initialized = false;
    tracker->highest_sequence = 0u;
    tracker->seen_bitmap = 0u;
}

pr1_sequence_result_t pr1_sequence_tracker_observe(pr1_sequence_tracker_t *tracker,
                                                    uint32_t sequence)
{
    pr1_sequence_result_t result = {
        .classification = PR1_SEQUENCE_TOO_OLD,
        .missing_before = 0u,
    };

    if (tracker == NULL) {
        return result;
    }

    if (!tracker->initialized) {
        tracker->initialized = true;
        tracker->highest_sequence = sequence;
        tracker->seen_bitmap = UINT64_C(1);
        result.classification = PR1_SEQUENCE_FIRST;
        return result;
    }

    if (pr1_seq_after(sequence, tracker->highest_sequence)) {
        const uint32_t advance = sequence - tracker->highest_sequence;
        result.classification = advance == 1u ? PR1_SEQUENCE_IN_ORDER : PR1_SEQUENCE_GAP;
        result.missing_before = advance - 1u;
        tracker->seen_bitmap = advance >= PR1_SEQUENCE_WINDOW_BITS
                                   ? UINT64_C(1)
                                   : (tracker->seen_bitmap << advance) | UINT64_C(1);
        tracker->highest_sequence = sequence;
        return result;
    }

    const uint32_t age = tracker->highest_sequence - sequence;
    if (age >= PR1_SEQUENCE_WINDOW_BITS) {
        result.classification = PR1_SEQUENCE_TOO_OLD;
        return result;
    }

    const uint64_t mask = UINT64_C(1) << age;
    if ((tracker->seen_bitmap & mask) != 0u) {
        result.classification = PR1_SEQUENCE_DUPLICATE;
        return result;
    }

    tracker->seen_bitmap |= mask;
    result.classification = PR1_SEQUENCE_OUT_OF_ORDER;
    return result;
}
