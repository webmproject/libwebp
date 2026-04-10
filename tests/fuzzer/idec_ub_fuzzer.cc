#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "src/webp/decode.h"

// OSS-Fuzz entrypoint
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) return 0;

    WebPIDecoder* idec = WebPINewDecoder(NULL);
    if (!idec) return 0;
    
    size_t chunk_size = 1;
    size_t offset = 0;
    
    while (offset < size) {
        size_t current_chunk = size - offset;
        if (current_chunk > chunk_size) {
            current_chunk = chunk_size;
        }
        
        VP8StatusCode status = WebPIAppend(idec, data + offset, current_chunk);
        
        if (status != VP8_STATUS_OK && status != VP8_STATUS_SUSPENDED) {
            break; 
        }
        
        offset += current_chunk;
        chunk_size = (chunk_size * 2) + 1;
    }

    WebPIDelete(idec);
    return 0; 
}
