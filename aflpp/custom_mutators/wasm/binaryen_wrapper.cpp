#include <cstdio>
#include <cstdlib>
#include <exception>
#include "binaryen-c.h"

extern "C" {

/* Safe wrapper around BinaryenModuleRead that catches C++ exceptions.
   Binaryen can throw exceptions when parsing invalid or unsupported WASM
   features (e.g., "control flow inputs are not supported yet"). Since
   we're fuzzing, we want to gracefully handle these cases rather than
   crashing the fuzzer.

   Returns NULL if parsing fails for any reason (invalid input or exception). */
BinaryenModuleRef safe_BinaryenModuleRead(char* input, size_t inputSize) {
    try {
        return BinaryenModuleRead(input, inputSize);
    } catch (const std::exception& e) {
        /* Log the exception for debugging, but don't crash. */
        static int log_count = 0;
        if (log_count < 10) {  // Limit spam
            fprintf(stderr, "[safe_BinaryenModuleRead] Caught exception during parse: %s\n", e.what());
            log_count++;
        }
        return nullptr;
    } catch (...) {
        /* Catch any other C++ exceptions (not derived from std::exception). */
        static int log_count = 0;
        if (log_count < 10) {
            fprintf(stderr, "[safe_BinaryenModuleRead] Caught unknown exception during parse\n");
            log_count++;
        }
        return nullptr;
    }
}

/* Safe wrapper around BinaryenModuleAllocateAndWrite that catches exceptions
   during serialization. */
int safe_BinaryenModuleAllocateAndWrite(BinaryenModuleRef module,
                                        BinaryenModuleAllocateAndWriteResult* result) {
    try {
        *result = BinaryenModuleAllocateAndWrite(module, nullptr);
        return 1;  // Success
    } catch (const std::exception& e) {
        static int log_count = 0;
        if (log_count < 10) {
            fprintf(stderr, "[safe_BinaryenModuleAllocateAndWrite] Caught exception: %s\n", e.what());
            log_count++;
        }
        return 0;  // Failure
    } catch (...) {
        static int log_count = 0;
        if (log_count < 10) {
            fprintf(stderr, "[safe_BinaryenModuleAllocateAndWrite] Caught unknown exception\n");
            log_count++;
        }
        return 0;  // Failure
    }
}

}  // extern "C"
