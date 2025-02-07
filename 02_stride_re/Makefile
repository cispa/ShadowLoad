# always rebuild since flags, etc. may change
.PHONY: test test_prefetch_simple test_prefetch_memory_collision test_prefetch_pc_collision test_prefetch_both_collisions

AUTO_TOOL_TIMER ?= rdtsc
AUTO_TOOL_VICTIM ?= userspace
AUTO_TOOL_FLAGS ?= -DUSE_FENCE

# Needed for setting affinity in hyperthread victim
AUTO_TOOL_FLAGS := $(AUTO_TOOL_FLAGS) -D_GNU_SOURCE -O3

test: test_prefetch_simple test_prefetch_memory_collision test_prefetch_pc_collision test_prefetch_both_collisions test_shadow_load

test_prefetch_simple:
	gcc ${AUTO_TOOL_FLAGS} -o tests/test_prefetch_simple -Ivictim/${AUTO_TOOL_VICTIM} -Itime/${AUTO_TOOL_TIMER} -I. ./tests/test_prefetch_simple.c ./uarch.S -pthread

test_prefetch_memory_collision:
	gcc ${AUTO_TOOL_FLAGS} -o tests/test_prefetch_memory_collision -Ivictim/${AUTO_TOOL_VICTIM} -Itime/${AUTO_TOOL_TIMER} -I. ./tests/test_prefetch_memory_collision.c ./uarch.S -pthread

test_prefetch_pc_collision:
	gcc ${AUTO_TOOL_FLAGS} -o tests/test_prefetch_pc_collision -Ivictim/${AUTO_TOOL_VICTIM} -Itime/${AUTO_TOOL_TIMER} -I. ./tests/test_prefetch_pc_collision.c ./uarch.S -pthread

test_prefetch_both_collisions:
	gcc ${AUTO_TOOL_FLAGS} -o tests/test_prefetch_both_collisions -Ivictim/${AUTO_TOOL_VICTIM} -Itime/${AUTO_TOOL_TIMER} -I. ./tests/test_prefetch_both_collisions.c ./uarch.S -pthread

test_shadow_load:
	gcc ${AUTO_TOOL_FLAGS} -o tests/test_shadow_load -Ivictim/kernel -Itime/${AUTO_TOOL_TIMER} -I. ./tests/test_shadow_load.c ./uarch.S -pthread

clean:
	rm -rf tests/out tests/__pycache__ tests/test_prefetch_simple tests/test_prefetch_memory_collision tests/test_prefetch_pc_collision tests/test_prefetch_both_collisions tests/test_shadow_load
