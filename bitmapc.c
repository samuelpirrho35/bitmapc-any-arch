/*

MIT License

Copyright (c) 2026 samuel_pirrho84.c

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "bitmapc.h"

/**
 * @struct __bit_info_s
 * @brief Bit position information structure
 * @internal
 * 
 * Used internally to store the decomposed position of a bit within
 * the bitmap buffer.
 * 
 * @var __bit_info_s::byte_addr
 * Byte index in the bitmap buffer (0-based)
 * 
 * @var __bit_info_s::bit_offset
 * Bit position within the byte (0-7), using MSB-first ordering.
 * A value of BITMAP_INVALID_BIT_ADDR indicates an error.
 */
typedef struct {
    __bitmap_word_t byte_addr;
    __bit_addr_t bit_offset;
} __bit_info_t;

__BITMAP_ATTR_PRIVATE_FUNC__ __bit_bool_t bitmap_verify_integrity(__bitmap_object_t *bitmap);
__BITMAP_ATTR_PRIVATE_FUNC__ __bit_bool_t bitmap_is_word_aligned(__bitmap_object_t *bitmap);
__BITMAP_ATTR_PRIVATE_FUNC__ void bitmap_get_bit_info(__bitmap_object_t *bitmap, __bit_addr_t bit, __bit_info_t *out);
__BITMAP_ATTR_PRIVATE_FUNC__ void *bitmap_memset(void *__s, int __c, unsigned long long __n);
__BITMAP_ATTR_PRIVATE_FUNC__ void *bitmap_memcpy(void *__dest, const void *__src, unsigned long long __n);
__BITMAP_ATTR_PRIVATE_FUNC__ __bit_bool_t bitmap_is_full_align_buffer(__bitmap_object_t *bitmap);
__BITMAP_ATTR_PRIVATE_FUNC__ __bit_bool_t bitmap_is_full_not_align_buffer(__bitmap_object_t *bitmap);

/** Memory allocation/free public functions */
__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_err_t bitmap_init(__bitmap_object_t *bitmap, __bitmap_word_t size, void *extern_buffer){
    if (!bitmap || !size) return BITMAP_INVALID_ARG;

    /** if external buffer is null: alloc dynamic buffer */
    if(!extern_buffer){
        /** align buffer to Arch-Bit length */
        bitmap->__bit_allocated = size;
        bitmap->__bit_nb_1 = 0;
        bitmap->__bit_nb_0 = bitmap->__bit_allocated;
        bitmap->__size = __bitmap_align(__bitmap_bits_to_bytes(size));
        bitmap->__ptr = __bitmap_malloc(bitmap->__size);

        if(!bitmap->__ptr){
            bitmap->__signature = 0;
            return BITMAP_MALLOC_ERROR;
        }

        bitmap->__mem_origin = BITMAP_MEM_DYNAMIC;
        bitmap->__signature = __bitmap_valid_instance;

        __bitmap_memset(bitmap->__ptr, 0, bitmap->__size);
        return BITMAP_OK;
    }

    bitmap->__bit_allocated = size;
    bitmap->__bit_nb_1 = 0;
    bitmap->__bit_nb_0 = bitmap->__bit_allocated;
    bitmap->__size = __bitmap_bits_to_bytes(size);
    bitmap->__ptr = extern_buffer;
    bitmap->__mem_origin = BITMAP_MEM_STATIC;
    bitmap->__signature = __bitmap_valid_instance;

    __bitmap_memset(bitmap->__ptr, 0, bitmap->__size);
    return BITMAP_OK;
}

__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_err_t bitmap_create_clone(__bitmap_object_t *bitmap_clone, __bitmap_object_t *bitmap_src){
    if(!bitmap_clone || !bitmap_verify_integrity(bitmap_src))
        return BITMAP_INVALID_ARG;

    /** For those forgetful souls who forget to clean up the Matrix's mess, like me. With love and affection. */
    if(bitmap_verify_integrity(bitmap_clone))
        bitmap_deinit(bitmap_clone);

    __bitmap_err_t err = bitmap_init(bitmap_clone, bitmap_src->__bit_allocated, NULL);
    if (err) return err;

    bitmap_clone->__bit_nb_1 = bitmap_src->__bit_nb_1;
    bitmap_clone->__bit_nb_0 = bitmap_src->__bit_nb_0;

    __bitmap_memcpy(bitmap_clone->__ptr, bitmap_src->__ptr, bitmap_src->__size);
    return BITMAP_OK;
}

__BITMAP_ATTR_PUBLIC_FUNC__ void bitmap_deinit(__bitmap_object_t *bitmap){
    if(!bitmap_verify_integrity(bitmap) || bitmap->__mem_origin == BITMAP_MEM_STATIC)
        return;

    free(bitmap->__ptr);
    __bitmap_memset(bitmap, 0, sizeof(*bitmap));
}

/** Bit operations */
__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_err_t bitmap_set_bit(__bitmap_object_t *bitmap, __bit_addr_t bit_addr){
    if(!bitmap_verify_integrity(bitmap))
        return BITMAP_INVALID_ARG;

    __bit_t __bit = bitmap_get_bit(bitmap, bit_addr);
    __bit_info_t info = { 0 };

    bitmap_get_bit_info(bitmap, bit_addr, &info);

    if(info.bit_offset == BITMAP_INVALID_BIT_ADDR)
        return BITMAP_OUT_OF_RANGE;

    __bitmap_byte_t *bytes = (__bitmap_byte_t*)bitmap->__ptr;

    bytes[info.byte_addr] |= (1U << info.bit_offset);
    bitmap->__bit_nb_1 += !__bit;
    bitmap->__bit_nb_0 -= !__bit;
    return BITMAP_OK;
}

__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_err_t bitmap_clear_bit(__bitmap_object_t *bitmap, __bit_addr_t bit_addr){
    if(!bitmap_verify_integrity(bitmap))
        return BITMAP_INVALID_ARG;

    __bit_info_t info = { 0 };
    __bit_t __bit = bitmap_get_bit(bitmap, bit_addr);

    bitmap_get_bit_info(bitmap, bit_addr, &info);

    if(info.bit_offset == BITMAP_INVALID_BIT_ADDR)
        return BITMAP_OUT_OF_RANGE;

    __bitmap_byte_t *bytes = (__bitmap_byte_t*)bitmap->__ptr;

    bytes[info.byte_addr] &= ~(1U << info.bit_offset);
    bitmap->__bit_nb_0 += __bit;
    bitmap->__bit_nb_1 -= __bit;
    return BITMAP_OK;
}

__BITMAP_ATTR_PUBLIC_FUNC__ __bit_t bitmap_get_bit(__bitmap_object_t *bitmap, __bit_addr_t bit_addr){
    if(!bitmap_verify_integrity(bitmap))
        return BITMAP_INVALID_BIT_ADDR;

    __bit_info_t info = { 0 };
    bitmap_get_bit_info(bitmap, bit_addr, &info);

    if(info.bit_offset == BITMAP_INVALID_BIT_ADDR)
        return BITMAP_INVALID_BIT_ADDR;

    __bitmap_byte_t *bytes = (__bitmap_byte_t*)bitmap->__ptr;
    return ((bytes[info.byte_addr] & (1 << info.bit_offset)) ? 1 : 0);
}

__BITMAP_ATTR_PUBLIC_FUNC__ __bit_bool_t bitmap_is_full(__bitmap_object_t *bitmap, __bitmap_err_t *err){
    if(!bitmap_verify_integrity(bitmap)){
        *err = BITMAP_INVALID_ARG;
        return __bit_false;
    }

    *err = BITMAP_OK;

    /** Case __bit_allocated < 8: search bit by bit */
    if(bitmap->__bit_allocated < 8){
        for(int b = 0; b < bitmap->__bit_allocated; b++){
            if(!bitmap_get_bit(bitmap, b))
                return __bit_false;
        }

        return __bit_true;
    }

    /** More fasted case. If __bit_allocated == 8: It simply checks if the byte has the value 0xFF */
    if(bitmap->__bit_allocated == 8){
        __bitmap_byte_t *bytes = (__bitmap_byte_t*)bitmap->__ptr;
        return (*bytes == 0xFF ? __bit_true : __bit_false);
    }

    /** search word by word (maybe ._.) */
    if(bitmap_is_word_aligned(bitmap))
        return bitmap_is_full_align_buffer(bitmap);

    /** byte by byte */
    return bitmap_is_full_not_align_buffer(bitmap);
}

/** Statistics and info */
__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_word_t bitmap_count_set_bits(__bitmap_object_t *bitmap){
    if(!bitmap_verify_integrity(bitmap))
        return BITMAP_INVALID_BIT_ADDR;

    return bitmap->__bit_nb_1;
}

__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_word_t bitmap_count_clear_bits(__bitmap_object_t *bitmap){
    if(!bitmap_verify_integrity(bitmap))
        return BITMAP_INVALID_BIT_ADDR;

    return bitmap->__bit_nb_0;
}

__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_word_t bitmap_get_size_in_bytes(__bitmap_object_t *bitmap){
    if(!bitmap_verify_integrity(bitmap))
        return BITMAP_INVALID_BIT_ADDR;

    return bitmap->__size;
}

__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_word_t bitmap_get_bit_capacity(__bitmap_object_t *bitmap){
    if(!bitmap_verify_integrity(bitmap))
        return BITMAP_INVALID_BIT_ADDR;

    return bitmap->__bit_allocated;
}

__BITMAP_ATTR_PUBLIC_FUNC__ void bitmap_clear_map(__bitmap_object_t *bitmap){
    if(!bitmap_verify_integrity(bitmap))
        return;

    bitmap->__bit_nb_1 = 0;
    bitmap->__bit_nb_0 = bitmap->__bit_allocated;

    __bitmap_memset(bitmap->__ptr, 0, bitmap->__size);
}

__BITMAP_ATTR_PUBLIC_FUNC__ void* bitmap_copy_to_buffer(__bitmap_object_t *bitmap, void *buffer, __bitmap_word_t size){
    if(!bitmap_verify_integrity(bitmap) || !buffer || !size)
        return NULL;

    __bitmap_byte_t *ptr_bytes = (__bitmap_byte_t*)bitmap->__ptr;
    __bitmap_byte_t *buf_bytes = (__bitmap_byte_t*)buffer;

    __bitmap_word_t copy_size = (size < bitmap->__size) ? size : bitmap->__size;
    
    // Use memcpy for better performance
    __bitmap_memcpy(buf_bytes, ptr_bytes, copy_size);
    return buffer;
}

/**
 * @brief Verifies the integrity of a bitmap object
 * @internal
 * 
 * Checks if the bitmap pointer is valid, has a valid data buffer, and
 * contains the correct integrity signature. This function is called by
 * all public API functions to ensure operations are performed on valid
 * bitmap objects.
 * 
 * @param bitmap Pointer to the bitmap object to verify
 * @return __bit_bool_t:
 *         - __bit_true: Bitmap object is valid and initialized
 *         - __bit_false: Bitmap is NULL, has NULL buffer, or invalid signature
 * 
 * @note The signature (__bitmap_valid_instance) is set during bitmap_init()
 * @note This function helps detect use-after-free and uninitialized bitmaps
 * 
 * @warning Does not verify the actual data buffer contents, only its existence
 * 
 * @example
 * __bitmap_object_t bitmap;
 * bitmap_init(&bitmap, 100, NULL);
 * 
 * if (bitmap_verify_integrity(&bitmap)) {
 *     // Safe to perform operations
 *     bitmap_set_bit(&bitmap, 42);
 * }
 * 
 * bitmap_deinit(&bitmap);
 * // Now bitmap_verify_integrity(&bitmap) returns __bit_false
 */
__BITMAP_ATTR_PRIVATE_FUNC__ __bit_bool_t bitmap_verify_integrity(__bitmap_object_t *bitmap){
    if(!bitmap || !bitmap->__ptr || bitmap->__signature != __bitmap_valid_instance)
        return __bit_false;

    return __bit_true;
}

/**
 * @brief Checks if the bitmap buffer is word-aligned for optimized access
 * @internal
 * 
 * Determines whether the bitmap's memory buffer is aligned to the system's
 * word size, which enables word-by-word operations for better performance.
 * 
 * @param bitmap Pointer to the bitmap object
 * @return __bit_bool_t:
 *         - __bit_true: Buffer is word-aligned (or dynamically allocated)
 *         - __bit_false: Buffer is not word-aligned
 * 
 * @note Dynamically allocated buffers are always considered aligned because
 *       malloc() returns memory suitable for any alignment
 * @note Static buffers are checked for both pointer alignment and size alignment
 * 
 * @performance Word-aligned buffers allow 4/8 byte comparisons instead of byte-by-byte
 * 
 * @example
 * // Dynamic allocation - always aligned
 * __bitmap_object_t dyn_bitmap;
 * bitmap_init(&dyn_bitmap, 1000, NULL);
 * assert(bitmap_is_word_aligned(&dyn_bitmap) == __bit_true);
 * 
 * // Static buffer - depends on alignment
 * uint8_t buffer[128];
 * __bitmap_object_t static_bitmap;
 * bitmap_init(&static_bitmap, 1000, buffer);
 * if (bitmap_is_word_aligned(&static_bitmap)) {
 *     // Can use word-aligned optimizations
 * }
 */
__BITMAP_ATTR_PRIVATE_FUNC__ __bit_bool_t bitmap_is_word_aligned(__bitmap_object_t *bitmap){
    if(bitmap->__mem_origin == BITMAP_MEM_DYNAMIC)
        return __bit_true;

    return (!((__bitmap_word_t)bitmap->__ptr % sizeof(void*)) && !(bitmap->__size % sizeof(void*)));
}

/**
 * @brief Calculates byte address and bit offset for a given bit position
 * @internal
 * 
 * Converts a linear bit address into byte address and bit offset within that byte.
 * Uses MSB-first ordering (bit 0 is the most significant bit of the first byte).
 * 
 * @param bitmap Pointer to the bitmap object (used for bounds checking)
 * @param bit Linear bit address to convert (0-based index)
 * @param out Pointer to __bit_info_t structure to fill with results
 * 
 * @note On success, out->byte_addr and out->bit_offset are set
 * @note On failure (bit out of range), out->bit_offset is set to BITMAP_INVALID_BIT_ADDR
 * 
 * @details Bit numbering follows MSB-first convention:
 *          Byte 0: bits 0-7 (bit 0 = MSB, bit 7 = LSB)
 *          Byte 1: bits 8-15, etc.
 * 
 * @warning The bitmap parameter is only used for bounds checking, not modified
 * 
 * @example
 * __bit_info_t info;
 * __bitmap_object_t bitmap;
 * bitmap_init(&bitmap, 100, NULL);
 * 
 * bitmap_get_bit_info(&bitmap, 5, &info);
 * // For 100-bit bitmap, bit 5 is in byte 0 (5/8=0) at offset 2 (7-5=2)
 * // info.byte_addr = 0, info.bit_offset = 2
 * 
 * bitmap_get_bit_info(&bitmap, 100, &info);
 * // info.bit_offset = BITMAP_INVALID_BIT_ADDR (out of range)
 */
__BITMAP_ATTR_PRIVATE_FUNC__ void bitmap_get_bit_info(__bitmap_object_t *bitmap, __bit_addr_t bit, __bit_info_t *out){
    if(bit >= bitmap->__bit_allocated){
        out->bit_offset = BITMAP_INVALID_BIT_ADDR;
        return;
    }

    out->byte_addr = bit / 8;
    __bit_addr_t bit_index = bit % 8;

    out->bit_offset = 7 - bit_index;
}

/**
 * @brief Custom memory set function with byte-level granularity
 * @internal
 * 
 * Sets a block of memory to a specific byte value. This is a simple
 * implementation that doesn't rely on standard library functions.
 * 
 * @param __s Pointer to the memory block to fill
 * @param __c Value to set (converted to unsigned char)
 * @param __n Number of bytes to set
 * @return void* Pointer to the memory block (__s), or NULL on error
 * 
 * @note Returns NULL if __s is NULL or __n is 0
 * @note This implementation is simple but not optimized for large blocks
 * 
 * @warning For production use, consider using the standard library memset()
 *         or an optimized assembly version
 * 
 * @see bitmap_memcpy() for copying memory
 * 
 * @example
 * uint8_t buffer[100];
 * bitmap_memset(buffer, 0xFF, sizeof(buffer));
 * // buffer is now filled with 0xFF
 * 
 * // Returns NULL on invalid input
 * void *result = bitmap_memset(NULL, 0, 10);  // result == NULL
 */
__BITMAP_ATTR_PRIVATE_FUNC__ void *bitmap_memset(void *__s, int __c, unsigned long long __n){
    if(!__s || !__n)
        return NULL;

    __bitmap_byte_t *bytes = (__bitmap_byte_t*)__s;

    for(unsigned long long i = 0; i < __n; i++)
        bytes[i] = __c;

    return __s;
}

/**
 * @brief Custom memory copy function with byte-level granularity
 * @internal
 * 
 * Copies a block of memory from source to destination. This is a simple
 * implementation that doesn't rely on standard library functions.
 * 
 * @param __dest Pointer to the destination memory block
 * @param __src Pointer to the source memory block
 * @param __n Number of bytes to copy
 * @return void* Pointer to the destination (__dest), or NULL on error
 * 
 * @note Returns NULL if __dest, __src, or __n is invalid (NULL or zero)
 * @note Overlapping regions are not handled specially (simple byte copy)
 * @note This implementation is not optimized for performance
 * 
 * @warning Does NOT handle overlapping source and destination safely!
 *          For overlapping regions, use memmove() instead.
 * 
 * @see bitmap_memset() for setting memory
 * 
 * @example
 * uint8_t src[50] = {0x01, 0x02, 0x03, ...};
 * uint8_t dest[50];
 * 
 * bitmap_memcpy(dest, src, sizeof(src));
 * // dest now contains identical data to src
 * 
 * // Returns NULL on invalid input
 * void *result = bitmap_memcpy(NULL, src, 10);  // result == NULL
 * 
 * @warning This example is DANGEROUS (overlapping regions):
 * // bitmap_memcpy(src + 10, src, 20);  // Undefined behavior!
 */
__BITMAP_ATTR_PRIVATE_FUNC__ void *bitmap_memcpy(void *__dest, const void *__src, unsigned long long __n){
    if(!__dest || !__src || !__n)
        return NULL;

    __bitmap_byte_t *_dest_bytes = (__bitmap_byte_t*)__dest;
    __bitmap_byte_t *_src_bytes = (__bitmap_byte_t*)__src;

    for(unsigned long long i = 0; i < __n; i++)
        _dest_bytes[i] = _src_bytes[i];

    return __dest;
}

/**
 * @brief Checks if bitmap is full using word-aligned access
 * @internal
 * 
 * Optimized for word-aligned memory buffers. Processes full words first,
 * then handles remaining bits individually.
 * 
 * @param bitmap Pointer to the bitmap object (must be word-aligned)
 * @return __bit_bool_t __bit_true if full, __bit_false otherwise
 * 
 * @pre bitmap_is_word_aligned(bitmap) must be true
 * @pre bitmap->__bit_allocated >= 8
 */
__BITMAP_ATTR_PRIVATE_FUNC__ __bit_bool_t bitmap_is_full_align_buffer(__bitmap_object_t *bitmap){
    __bitmap_word_t bits_to_words = bitmap->__bit_allocated / (8 * sizeof(__bitmap_word_t));
    __bitmap_word_t bit_diff = bitmap->__bit_allocated - bits_to_words * 8 * sizeof(__bitmap_word_t);

    if(!bits_to_words)
        return bitmap_is_full_not_align_buffer(bitmap);

    __bitmap_word_t *words = (__bitmap_word_t*)bitmap->__ptr;

    for(__bitmap_word_t w = 0; w < bits_to_words; w++){
        if(words[w] != BITMAP_WORD_FULL)
            return __bit_false;
    }

    if(bit_diff){
        __bit_addr_t bit_pos = bits_to_words * sizeof(__bitmap_word_t);

        for(; bit_pos < bitmap->__bit_allocated; bit_pos++){
            if(!bitmap_get_bit(bitmap, bit_pos))
                return __bit_false;
        }
    }

    return __bit_true;
}

/**
 * @brief Checks if bitmap is full using byte-by-byte access
 * @internal
 * 
 * Fallback function for non-aligned buffers. Processes full bytes first,
 * then handles remaining bits individually.
 * 
 * @param bitmap Pointer to the bitmap object
 * @return __bit_bool_t __bit_true if full, __bit_false otherwise
 * 
 * @note Works correctly for any buffer alignment
 */
__BITMAP_ATTR_PRIVATE_FUNC__ __bit_bool_t bitmap_is_full_not_align_buffer(__bitmap_object_t *bitmap){
    __bitmap_word_t bits_to_bytes = bitmap->__bit_allocated / 8;
    __bitmap_word_t bit_diff = bitmap->__bit_allocated - bits_to_bytes * 8;

    __bitmap_byte_t *bytes = (__bitmap_byte_t*)bitmap->__ptr;

    for(__bitmap_word_t b = 0; b < bits_to_bytes; b++){
        if(bytes[b] != 0xFF)
            return __bit_false;
    }
    
    if(bit_diff){
        __bit_addr_t bit_pos = bits_to_bytes * sizeof(__bitmap_word_t);

        for(; bit_pos < bitmap->__bit_allocated; bit_pos++){
            if(!bitmap_get_bit(bitmap, bit_pos))
                return __bit_false;
        }
    }

    return __bit_true;
}
