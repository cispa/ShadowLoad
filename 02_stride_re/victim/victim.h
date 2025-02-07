#error "do not include this header directly! rather include victim.h of an implementation"

// template for victim implementation header.
// the following must be defined (either as macro or functions):

uint64_t VICTIM_BUFFER_SIZE;

int victim_init(void);

uintptr_t victim_buffer_address(void);

uintptr_t victim_load_address(void);

void victim_flush_buffer(void);

void victim_load_gadget(uint64_t offset);

uint64_t victim_probe(uint64_t offset);

void victim_destroy(void);
