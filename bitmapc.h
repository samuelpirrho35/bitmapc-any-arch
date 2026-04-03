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


/**
 * 
 * @file bitmapc.h
 * @brief Architecture-optimized bitmap library for embedded systems
 * @author Samuel Pirrho (https://github.com/samuelpirrho35)
 * @version 1.0
 * @date 2026
 * 
 * @copyright Copyright (c) 2026
 * 
 * This library provides efficient bitmap operations with automatic
 * architecture detection (32/64-bit, endianness) and optional fallback
 * implementations when standard libraries are not available.
 */

#ifndef __BITMAPC_H__
#define __BITMAPC_H__

#if __has_include(<stdio.h>)
    #include <stdio.h>
    /**
     * @brief printf wrapper for bitmap library
     * @param _format Format string
     * @param ... Variable arguments
     */
    #define __bitmap_printf(_format, ...) printf(_format, __VA_ARGS__)
#else
    #define __bitmap_printf(_format, ...)
    #define NULL ((void*)0)
#endif


#if __has_include(<stdlib.h>)
    #include <stdlib.h>
    /**
     * @brief malloc wrapper for bitmap library
     * @param _s Size in bytes to allocate
     * @return Pointer to allocated memory or NULL on failure
     */
    #define __bitmap_malloc(_s) malloc(_s)
#else
    #define __bitmap_malloc(_s) NULL
#endif

#if __has_include(<string.h>)
    #include <string.h>
    /**
     * @brief memset wrapper (uses standard library when available)
     */
    #define __bitmap_memset(_s, _c, _n)      memset(_s, _c, _n)
    /**
     * @brief memcpy wrapper (uses standard library when available)
     */
    #define __bitmap_memcpy(_dest, _src, _n) memcpy(_dest, _src, _n)
#else
    /**
     * @brief Fallback memset implementation (no stdlib)
     */
    #define __bitmap_memset(_s, _c, _n)      bitmap_memset(_s, _c, _n)
    /**
     * @brief Fallback memcpy implementation (no stdlib)
     */
    #define __bitmap_memcpy(_dest, _src, _n) bitmap_memcpy(_dest, _src, _n)
#endif

/**
 * @brief Marks functions as private (internal use only)
 * @note These functions are inlined for performance
 */
#define __BITMAP_ATTR_PRIVATE_FUNC__ static inline /** These functions were implemented for internal use. Do not alter them... please */

/**
 * @brief Marks functions as public API
 * @note These functions form the external interface of the library
 */
#define __BITMAP_ATTR_PUBLIC_FUNC__ /** public library functions */

/** @brief Type used for integrity signatures */
#define __bitmap_signature_t    unsigned int
/** @brief Magic number for valid bitmap instances (0x00B1757A) */
#define __bitmap_valid_instance 0x00B1757A

/** @brief Memory alignment requirement (size of pointer) */
#define BITMAP_ALIGNMENT sizeof(void*)

/**
 * @brief Aligns a size value to the system's word boundary
 * @param size Size in bytes to align
 * @return Aligned size (multiple of BITMAP_ALIGNMENT)
 */
#define __bitmap_align(size) (((size) + (BITMAP_ALIGNMENT - 1)) & ~(BITMAP_ALIGNMENT - 1))

/**
 * @brief Converts bits to minimum required bytes
 * @param size Number of bits
 * @return Minimum bytes needed (rounded up)
 */
#define __bitmap_bits_to_bytes(size) ((size + 7) / 8)

#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64)
    /** @brief 64-bit architecture detected */
    #define BITMAP_INVALID_BIT_ADDR 0xFFFFFFFFFFFFFFFF
    /** @brief Word full mask for 64-bit systems */
    #define BITMAP_WORD_FULL 0xFFFFFFFFFFFFFFFFULL
    /** @brief Native word type for 64-bit systems */
    typedef unsigned long long __bitmap_word_t;
#else
    /** @brief 32-bit architecture detected */
    #define BITMAP_INVALID_BIT_ADDR 0xFFFFFFFFUL
    /** @brief Word full mask for 32-bit systems */
    #define BITMAP_WORD_FULL 0xFFFFFFFFUL
    /** @brief Native word type for 32-bit systems */
    typedef unsigned int __bitmap_word_t;
#endif

/**
 * @brief Endianness detection
 * @details Automatically detects if the system is big-endian
 */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define BITMAP_IS_BIG_ENDIAN 1
#elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)
    #define BITMAP_IS_BIG_ENDIAN 1
#else
    #define BITMAP_IS_BIG_ENDIAN 0
#endif

typedef __bitmap_word_t __bit_t;
typedef __bitmap_word_t __bit_addr_t;

typedef unsigned char __bitmap_byte_t;

/**
 * @brief Boolean type for bitmap operations
 */
typedef enum {
    __bit_false = 0,    /**< False value (0) */
    __bit_true  = 1,    /**< True value (1) */
    __bit_inv   = 2     /**< Invalid state */
} __bit_bool_t;

/**
 * @brief Error codes returned by bitmap functions
 */
typedef enum {
    BITMAP_OK = 0,                      /**< Operation completed successfully */
    BITMAP_INVALID_ARG,                 /**< Invalid argument provided */
    BITMAP_INVALID_INSTANCE,            /**< Invalid bitmap instance (corrupted signature) */
    BITMAP_MALLOC_ERROR,                /**< Memory allocation failed */
    BITMAP_INVALID_EXTERN_BUFFER,       /**< External buffer is invalid */
    BITMAP_OUT_OF_RANGE                 /**< Bit address out of bitmap bounds */
} __bitmap_err_t;

/**
 * @brief Memory origin type for bitmap buffers
 */
typedef enum {
    BITMAP_MEM_STATIC  = 0xB1757A71,    /**< Static/external buffer (no auto-free) */
    BITMAP_MEM_DYNAMIC = 0x000B17D1     /**< Dynamically allocated buffer (auto-freed) */
} __bitmap_mem_origin_t;

/**
 * @brief Main bitmap object structure
 * 
 * This structure holds all state information for a bitmap instance.
 * Fields with double underscores are private and should not be accessed
 * directly by user code.
 */
typedef struct {
    /**
     * @brief Pointer to the base memory buffer where the bitmap is mounted
     * @note This buffer is either externally provided or dynamically allocated
     */
    void  *__ptr;
    
    /**
     * @brief Virtual buffer size in bits
     * @note The actual buffer size in bytes may be larger due to alignment
     */
    __bitmap_word_t __bit_allocated;
    
    /**
     * @brief Actual buffer size in bytes (aligned)
     * @note Always >= __bitmap_bits_to_bytes(__bit_allocated)
     */
    __bitmap_word_t __size;
    
    /**
     * @brief Current number of bits set to 1
     * @note Automatically maintained by set/clear operations
     */
    __bitmap_word_t __bit_nb_1;
    
    /**
     * @brief Current number of bits set to 0
     * @note Automatically maintained by set/clear operations
     */
    __bitmap_word_t __bit_nb_0;
    
    /**
     * @brief Memory origin type
     * @see __bitmap_mem_origin_t
     */
    __bitmap_mem_origin_t __mem_origin;
    
    /**
     * @brief Object signature for integrity verification
     * @note Must equal __bitmap_valid_instance (0x00B1757A)
     */
    __bitmap_signature_t __signature;
} __bitmap_object_t;

/**
 * @brief Initializes a bitmap object
 * 
 * This function initializes a bitmap, optionally using an external static buffer
 * or dynamically allocating memory. The buffer is aligned to the architecture's
 * word size for optimized access.
 * 
 * @param bitmap Pointer to the bitmap object to initialize
 * @param size Number of bits in the bitmap
 * @param extern_buffer External buffer to use (NULL for dynamic allocation)
 * @return __bitmap_err_t Error code:
 *         - BITMAP_OK: Success
 *         - BITMAP_INVALID_ARG: Invalid bitmap pointer or zero size
 *         - BITMAP_MALLOC_ERROR: Dynamic memory allocation failed
 * 
 * @note When extern_buffer is NULL, memory is allocated automatically and
 *       aligned to the system's word boundary
 * @note The bitmap is initially cleared (all bits set to 0)
 * 
 * @example
 * __bitmap_object_t bitmap;
 * bitmap_init(&bitmap, 1024, NULL);  // Dynamically allocated 1024-bit bitmap
 */
__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_err_t bitmap_init(__bitmap_object_t *bitmap, __bitmap_word_t size, void *extern_buffer);

/**
 * @brief Creates a deep clone of a source bitmap
 * 
 * Creates an exact copy of the source bitmap including all bit states and
 * counters. If the destination bitmap is already initialized, it is properly
 * deinitialized first to prevent memory leaks.
 * 
 * @param bitmap_clone Pointer to the destination bitmap object
 * @param bitmap_src Pointer to the source bitmap object to clone
 * @return __bitmap_err_t Error code:
 *         - BITMAP_OK: Success
 *         - BITMAP_INVALID_ARG: Invalid bitmap pointers or source integrity check fails
 * 
 * @warning This function automatically cleans up the destination bitmap if it
 *          was previously initialized (with love and affection for forgetful souls)
 * 
 * @note The clone receives its own independent memory allocation
 * @note Bit counters (__bit_nb_0 and __bit_nb_1) are preserved exactly
 * 
 * @example
 * __bitmap_object_t original, clone;
 * bitmap_init(&original, 512, NULL);
 * bitmap_set_bit(&original, 42);
 * bitmap_create_clone(&clone, &original);  // clone now has bit 42 set
 */
__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_err_t bitmap_create_clone(__bitmap_object_t *bitmap_clone, __bitmap_object_t *bitmap_src);

/**
 * @brief Deinitializes a bitmap and frees associated memory
 * 
 * Releases memory allocated for dynamic bitmaps. For static bitmaps
 * (using external buffer), only the object structure is cleared.
 * 
 * @param bitmap Pointer to the bitmap object to deinitialize
 * 
 * @note Static bitmaps (BITMAP_MEM_STATIC) do NOT have their external buffer freed
 * @note The bitmap object is zeroed after deinitialization
 * @note Safe to call on uninitialized or already deinitialized bitmaps
 * 
 * @warning Do not use the bitmap after deinitialization without reinitializing
 * 
 * @example
 * __bitmap_object_t bitmap;
 * bitmap_init(&bitmap, 1024, NULL);
 * // ... use bitmap ...
 * bitmap_deinit(&bitmap);  // Memory freed, bitmap zeroed
 */
__BITMAP_ATTR_PUBLIC_FUNC__ void  bitmap_deinit(__bitmap_object_t *bitmap);

/**
 * @brief Sets a specific bit to 1
 * 
 * Sets the bit at the specified address to 1 and updates the internal
 * counters of set and cleared bits.
 * 
 * @param bitmap Pointer to the bitmap object
 * @param bit_addr Address of the bit to set (0-based index)
 * @return __bitmap_err_t Error code:
 *         - BITMAP_OK: Success
 *         - BITMAP_INVALID_ARG: Invalid bitmap or integrity check fails
 *         - BITMAP_OUT_OF_RANGE: Bit address exceeds bitmap size
 * 
 * @note Updates __bit_nb_1 and __bit_nb_0 counters atomically
 * @note If the bit was already 1, counters remain unchanged
 * 
 * @example
 * __bitmap_object_t bitmap;
 * bitmap_init(&bitmap, 100, NULL);
 * bitmap_set_bit(&bitmap, 42);  // Sets bit 42 to 1
 */
__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_err_t bitmap_set_bit(__bitmap_object_t *bitmap, __bit_addr_t bit_addr);

/**
 * @brief Clears a specific bit to 0
 * 
 * Sets the bit at the specified address to 0 and updates the internal
 * counters of set and cleared bits.
 * 
 * @param bitmap Pointer to the bitmap object
 * @param bit_addr Address of the bit to clear (0-based index)
 * @return __bitmap_err_t Error code:
 *         - BITMAP_OK: Success
 *         - BITMAP_INVALID_ARG: Invalid bitmap or integrity check fails
 *         - BITMAP_OUT_OF_RANGE: Bit address exceeds bitmap size
 * 
 * @note Updates __bit_nb_1 and __bit_nb_0 counters atomically
 * @note If the bit was already 0, counters remain unchanged
 * 
 * @example
 * bitmap_set_bit(&bitmap, 42);    // Set bit 42
 * bitmap_clear_bit(&bitmap, 42);  // Clear bit 42 back to 0
 */
__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_err_t bitmap_clear_bit(__bitmap_object_t *bitmap, __bit_addr_t bit_addr);

/**
 * @brief Retrieves the value of a specific bit
 * 
 * Reads and returns the current state (0 or 1) of the bit at the specified address.
 * 
 * @param bitmap Pointer to the bitmap object
 * @param bit_addr Address of the bit to read (0-based index)
 * @return __bit_t Bit value:
 *         - 0: Bit is cleared
 *         - 1: Bit is set
 *         - BITMAP_INVALID_BIT_ADDR: Invalid bitmap or address out of range
 * 
 * @note Performs boundary checking to prevent out-of-bounds access
 * 
 * @example
 * __bit_t state = bitmap_get_bit(&bitmap, 42);
 * if (state == 1) {
 *     printf("Bit 42 is set!\n");
 * }
 */
__BITMAP_ATTR_PUBLIC_FUNC__ __bit_t bitmap_get_bit(__bitmap_object_t *bitmap, __bit_addr_t bit_addr);

/**
 * @brief Checks if the bitmap is completely full (all bits set to 1)
 * 
 * Optimized function that uses architecture-specific word sizes and alignment
 * to minimize operations. Implements several optimization strategies:
 * - For < 8 bits: Linear bit-by-bit search
 * - For == 8 bits: Single byte comparison against 0xFF
 * - For > 8 bits: Word-aligned block comparisons when possible
 * 
 * @param bitmap Pointer to the bitmap object
 * @param err Pointer to store error code (can be NULL)
 * @return __bit_bool_t:
 *         - __bit_true: All bits are set to 1
 *         - __bit_false: At least one bit is 0 or an error occurred
 * 
 * @note When err is not NULL, it receives:
 *       - BITMAP_OK: Success
 *       - BITMAP_INVALID_ARG: Invalid bitmap or integrity check fails
 * 
 * @performance The function automatically selects the fastest method based on:
 *              1. Bitmap size
 *              2. Memory alignment
 *              3. Architecture word size
 * 
 * @example
 * __bitmap_err_t err;
 * if (bitmap_is_full(&bitmap, &err)) {
 *     printf("Bitmap is completely full!\n");
 * }
 */
__BITMAP_ATTR_PUBLIC_FUNC__ __bit_bool_t bitmap_is_full(__bitmap_object_t *bitmap, __bitmap_err_t *err);

/**
 * @brief Returns the number of bits set to 1 in the bitmap
 * 
 * This function provides O(1) access to the count of set bits by returning
 * the cached value maintained during all bit operations. The counter is
 * automatically updated by bitmap_set_bit() and bitmap_clear_bit().
 * 
 * @param bitmap Pointer to the bitmap object
 * @return __bitmap_word_t Number of bits set to 1, or BITMAP_INVALID_BIT_ADDR on error
 * 
 * @performance O(1) - Constant time operation
 * @note The counter is guaranteed to be accurate as long as all bit modifications
 *       are performed through the library's API functions
 * 
 * @warning Direct manipulation of the underlying buffer will invalidate the counter
 * 
 * @example
 * __bitmap_object_t bitmap;
 * bitmap_init(&bitmap, 1000, NULL);
 * bitmap_set_bit(&bitmap, 10);
 * bitmap_set_bit(&bitmap, 20);
 * bitmap_set_bit(&bitmap, 30);
 * 
 * __bitmap_word_t set_bits = bitmap_count_set_bits(&bitmap);
 * printf("Bits set to 1: %u\n", set_bits);  // Output: 3
 */
__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_word_t bitmap_count_set_bits(__bitmap_object_t *bitmap);

/**
 * @brief Returns the number of bits set to 0 in the bitmap
 * 
 * This function provides O(1) access to the count of cleared bits by returning
 * the cached value maintained during all bit operations. The counter is
 * automatically updated by bitmap_set_bit() and bitmap_clear_bit().
 * 
 * @param bitmap Pointer to the bitmap object
 * @return __bitmap_word_t Number of bits set to 0, or BITMAP_INVALID_BIT_ADDR on error
 * 
 * @performance O(1) - Constant time operation
 * @note The value is derived from: clear_bits = total_bits - set_bits
 * @note The counter is maintained internally for performance reasons
 * 
 * @warning Direct manipulation of the underlying buffer will invalidate the counter
 * 
 * @example
 * __bitmap_object_t bitmap;
 * bitmap_init(&bitmap, 100, NULL);
 * bitmap_set_bit(&bitmap, 42);
 * 
 * __bitmap_word_t clear_bits = bitmap_count_clear_bits(&bitmap);
 * printf("Bits set to 0: %u\n", clear_bits);  // Output: 99 (if total = 100)
 */
__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_word_t bitmap_count_clear_bits(__bitmap_object_t *bitmap);

/**
 * @brief Returns the memory footprint of the bitmap in bytes
 * 
 * Retrieves the actual buffer size allocated for the bitmap data, which may
 * be larger than the theoretical minimum due to alignment requirements.
 * 
 * @param bitmap Pointer to the bitmap object
 * @return __bitmap_word_t Size of the bitmap buffer in bytes, or BITMAP_INVALID_BIT_ADDR on error
 * 
 * @note The returned size includes padding added for memory alignment
 * @note For dynamically allocated bitmaps, this is the size passed to malloc()
 * @note For static bitmaps with external buffer, this is the minimum required size
 * 
 * @see bitmap_get_bit_capacity() for the number of bits
 * 
 * @example
 * __bitmap_object_t bitmap;
 * bitmap_init(&bitmap, 1000, NULL);  // 1000 bits ≈ 125 bytes
 * 
 * __bitmap_word_t bytes = bitmap_get_size_in_bytes(&bitmap);
 * printf("Buffer size: %u bytes\n", bytes);  // May output 128 (aligned to 8/16/32/64)
 * 
 * // Useful for debugging memory usage
 * __bitmap_word_t overhead = bytes - ((1000 + 7) / 8);
 * printf("Alignment overhead: %u bytes\n", overhead);
 */
__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_word_t bitmap_get_size_in_bytes(__bitmap_object_t *bitmap);

/**
 * @brief Returns the total bit capacity of the bitmap
 * 
 * Retrieves the maximum number of bits that the bitmap can store, which is
 * the value specified during initialization. This represents the logical
 * capacity rather than the physical memory footprint.
 * 
 * @param bitmap Pointer to the bitmap object
 * @return __bitmap_word_t Total number of bits the bitmap can hold, or 
 *         BITMAP_INVALID_BIT_ADDR on error (invalid bitmap or integrity check fails)
 * 
 * @performance O(1) - Constant time operation, returns cached value
 * 
 * @note The bit capacity is fixed at initialization time and cannot be changed
 * @note Valid bit addresses range from 0 to (capacity - 1) inclusive
 * @note The actual memory usage may be larger due to alignment (see bitmap_get_size_in_bytes())
 * 
 * @see bitmap_get_size_in_bytes() for the actual memory footprint
 * @see bitmap_count_set_bits() for number of currently set bits
 * @see bitmap_count_clear_bits() for number of currently cleared bits
 * 
 * @example
 * __bitmap_object_t bitmap;
 * bitmap_init(&bitmap, 1000, NULL);
 * 
 * __bitmap_word_t capacity = bitmap_get_bit_capacity(&bitmap);
 * printf("Bitmap can store %u bits\n", capacity);  // Output: 1000
 * 
 * // Relationship between capacity, set bits, and clear bits:
 * __bitmap_word_t set = bitmap_count_set_bits(&bitmap);
 * __bitmap_word_t clear = bitmap_count_clear_bits(&bitmap);
 * assert(capacity == set + clear);  // Always true
 * 
 * @example
 * // Boundary checking example
 * __bitmap_word_t max_bits = bitmap_get_bit_capacity(&bitmap);
 * for (__bit_addr_t i = 0; i < max_bits; i++) {
 *     // Safe to access bits 0 to max_bits-1
 *     __bit_t bit = bitmap_get_bit(&bitmap, i);
 * }
 * 
 * // This would be out of bounds
 * // bitmap_get_bit(&bitmap, max_bits);  // Returns BITMAP_INVALID_BIT_ADDR
 */
__BITMAP_ATTR_PUBLIC_FUNC__ __bitmap_word_t bitmap_get_bit_capacity(__bitmap_object_t *bitmap);

/**
 * @brief Clears all bits in the bitmap (sets all to 0)
 * 
 * Resets every bit in the bitmap to 0 and updates the internal counters
 * accordingly. This operation is O(n) where n is the buffer size in bytes,
 * but optimized using memory set operations.
 * 
 * @param bitmap Pointer to the bitmap object to clear
 * 
 * @performance O(buffer_size) - Uses efficient memory set operation
 * @note After clearing, all bits are 0: bitmap_is_empty() will return true
 * @note Counters are updated atomically: __bit_nb_1 = 0, __bit_nb_0 = total_bits
 * 
 * @warning This operation cannot be undone unless you have a backup
 * @see bitmap_fill() for setting all bits to 1 (if implemented)
 * @see bitmap_clear_bit() for clearing individual bits
 * 
 * @example
 * __bitmap_object_t bitmap;
 * bitmap_init(&bitmap, 1000, NULL);
 * 
 * // Set some bits
 * bitmap_set_bit(&bitmap, 10);
 * bitmap_set_bit(&bitmap, 20);
 * bitmap_set_bit(&bitmap, 30);
 * 
 * printf("Set bits: %u\n", bitmap_count_set_bits(&bitmap)); // Output: 3
 * 
 * // Clear everything
 * bitmap_clear_map(&bitmap);
 * 
 * printf("Set bits after clear: %u\n", bitmap_count_set_bits(&bitmap)); // Output: 0
 * printf("Clear bits: %u\n", bitmap_count_clear_bits(&bitmap)); // Output: 1000
 * 
 * @example
 * // Useful for resetting state machines
 * void reset_device_state(void) {
 *     bitmap_clear_map(&device_flags);
 *     bitmap_clear_map(&error_bits);
 *     bitmap_clear_map(&pending_operations);
 * }
 */
__BITMAP_ATTR_PUBLIC_FUNC__ void bitmap_clear_map(__bitmap_object_t *bitmap);

/**
 * @brief Copies the entire bitmap data to an external buffer
 * 
 * Exports the raw bitmap data (byte-aligned) to a user-provided buffer.
 * This is useful for serialization, network transmission, or interfacing
 * with hardware that expects raw bitmap data.
 * 
 * @param bitmap Pointer to the bitmap object (must be initialized)
 * @param buffer Destination buffer to receive the bitmap data
 * @param size Size of the destination buffer in bytes
 * @return void* Pointer to the destination buffer on success, NULL on error
 * 
 * @note The buffer should be at least bitmap_get_size_in_bytes() bytes large
 * @note If the buffer is larger than needed, only the required bytes are copied
 * @note The data is copied in byte order (LSB-first within each byte)
 * 
 * @warning No bounds checking beyond the provided size parameter
 * @warning Ensure buffer is large enough to prevent buffer overflow
 * 
 * @see bitmap_import() for the inverse operation
 * @see bitmap_get_size_in_bytes() to determine required buffer size
 * 
 * @example
 * __bitmap_object_t bitmap;
 * bitmap_init(&bitmap, 1024, NULL);
 * 
 * // Set some bits
 * bitmap_set_bit(&bitmap, 0);
 * bitmap_set_bit(&bitmap, 7);
 * bitmap_set_bit(&bitmap, 8);
 * 
 * // Export to buffer
 * __bitmap_word_t required_size = bitmap_get_size_in_bytes(&bitmap);
 * uint8_t *export_buffer = malloc(required_size);
 * 
 * if (bitmap_copy_to_buffer(&bitmap, export_buffer, required_size)) {
 *     // Successfully copied - can now save to disk or send over network
 *     save_to_file(export_buffer, required_size);
 * }
 * 
 * free(export_buffer);
 * 
 * @example
 * // Interfacing with hardware that expects bitmap data
 * void update_hardware_buffer(__bitmap_object_t *bitmap, void *hw_register) {
 *     __bitmap_word_t size = bitmap_get_size_in_bytes(bitmap);
 *     bitmap_copy_to_buffer(bitmap, hw_register, size);
 *     // Hardware now has the latest bitmap state
 * }
 */
__BITMAP_ATTR_PUBLIC_FUNC__ void* bitmap_copy_to_buffer(__bitmap_object_t *bitmap, void *buffer, __bitmap_word_t size);

#endif // __BITMAPC_H__
