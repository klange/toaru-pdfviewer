#include <stdlib.h>
#include <stdint.h>

void * memalign(size_t alignment, size_t size) {
	return malloc(size);
}
