#include "jvm.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "heap.h"
#include "read_class.h"

typedef struct stack {
    int *items;
    size_t size;
    size_t capacity;
} stack_t;

stack_t *stack_init(size_t capacity) {
    stack_t *stack = malloc(sizeof(stack_t));
    stack->items = malloc(sizeof(int) * capacity);
    stack->size = 0;
    stack->capacity = capacity;
    return stack;
}

void stack_push(stack_t *stack, int item) {
    assert(stack->size < stack->capacity);
    stack->items[stack->size] = item;
    stack->size++;
}

int stack_pop(stack_t *stack) {
    assert(stack->size > 0);
    stack->size--;
    return stack->items[stack->size];
}

const size_t EXIT_NUMBER = 99;

/** The name of the method to invoke to run the class file */
const char MAIN_METHOD[] = "main";
/**
 * The "descriptor" string for main(). The descriptor encodes main()'s signature,
 * i.e. main() takes a String[] and returns void.
 * If you're interested, the descriptor string is explained at
 * https://docs.oracle.com/javase/specs/jvms/se12/html/jvms-4.html#jvms-4.3.2.
 */
const char MAIN_DESCRIPTOR[] = "([Ljava/lang/String;)V";

/**
 * Represents the return value of a Java method: either void or an int or a reference.
 * For simplification, we represent a reference as an index into a heap-allocated array.
 * (In a real JVM, methods could also return object references or other primitives.)
 */
typedef struct {
    /** Whether this returned value is an int */
    bool has_value;
    /** The returned value (only valid if `has_value` is true) */
    int32_t value;
} optional_value_t;

/**
 * Runs a method's instructions until the method returns.
 *
 * @param method the method to run
 * @param locals the array of local variables, including the method parameters.
 *   Except for parameters, the locals are uninitialized.
 * @param class the class file the method belongs to
 * @param heap an array of heap-allocated pointers, useful for references
 * @return an optional int containing the method's return value
 */
optional_value_t execute(method_t *method, int32_t *locals, class_file_t *class,
                         heap_t *heap) {
    /* You should remove these casts to void in your solution.
     * They are just here so the code compiles without warnings. */
    code_t method_code = method->code;
    size_t max_stack = method_code.max_stack;
    stack_t *stack = stack_init(max_stack);
    u1 *code = method_code.code;

    size_t pc = 0;
    while (pc < method_code.code_length) {
        u1 instruction = code[pc];
        switch (instruction) {
            case i_bipush: {
                int8_t b = code[pc + 1];
                stack_push(stack, b);
                pc += 2;
                break;
            }
            case i_iadd: {
                int b1 = stack_pop(stack);
                int b2 = stack_pop(stack);
                stack_push(stack, b1 + b2);
                pc++;
                break;
            }
            case i_getstatic: {
                pc += 3;
                break;
            }
            case i_invokevirtual: {
                int item = stack_pop(stack);
                printf("%d\n", item);
                pc += 3;
                break;
            }
            case i_iconst_m1 ... i_iconst_5: {
                stack_push(stack, instruction - i_iconst_m1 - 1);
                pc++;
                break;
            }
            case i_sipush: {
                u1 b1 = code[pc + 1];
                u1 b2 = code[pc + 2];
                int16_t item = (b1 << 8) | b2;
                stack_push(stack, item);
                pc += 3;
                break;
            }
            case i_isub: {
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                stack_push(stack, a - b);
                pc++;
                break;
            }
            case i_imul: {
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                stack_push(stack, a * b);
                pc++;
                break;
            }
            case i_idiv: {
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                stack_push(stack, a / b);
                pc++;
                break;
            }
            case i_irem: {
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                stack_push(stack, a % b);
                pc++;
                break;
            }
            case i_ineg: {
                int a = stack_pop(stack);
                stack_push(stack, -a);
                pc++;
                break;
            }
            case i_ishl: {
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                stack_push(stack, a << b);
                pc++;
                break;
            }
            case i_ishr: {
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                stack_push(stack, a >> b);
                pc++;
                break;
            }
            case i_iushr: {
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                stack_push(stack, ((uint32_t) a) >> b);
                pc++;
                break;
            }
            case i_iand: {
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                stack_push(stack, a & b);
                pc++;
                break;
            }
            case i_ior: {
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                stack_push(stack, a | b);
                pc++;
                break;
            }
            case i_ixor: {
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                stack_push(stack, a ^ b);
                pc++;
                break;
            }
            case i_iload: {
                u1 i = code[pc + 1];
                stack_push(stack, locals[i]);
                pc += 2;
                break;
            }
            case i_istore: {
                u1 i = code[pc + 1];
                int a = stack_pop(stack);
                locals[i] = a;
                pc += 2;
                break;
            }
            case i_iinc: {
                u1 i = code[pc + 1];
                int8_t b = code[pc + 2];
                locals[i] += b;
                pc += 3;
                break;
            }
            case i_iload_0 ... i_iload_3: {
                u1 i = instruction - i_iload_0;
                stack_push(stack, locals[i]);
                pc++;
                break;
            }
            case i_istore_0 ... i_istore_3: {
                u1 i = instruction - i_istore_0;
                int a = stack_pop(stack);
                locals[i] = a;
                pc++;
                break;
            }
            case i_ldc: {
                u1 b = code[pc + 1];
                int *item = class->constant_pool[b - 1].info;
                stack_push(stack, *item);
                pc += 2;
                break;
            }
            case i_ifeq: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int a = stack_pop(stack);
                if (a == 0) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_ifne: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int a = stack_pop(stack);
                if (a != 0) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_iflt: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int a = stack_pop(stack);
                if (a < 0) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_ifge: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int a = stack_pop(stack);
                if (a >= 0) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_ifgt: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int a = stack_pop(stack);
                if (a > 0) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_ifle: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int a = stack_pop(stack);
                if (a <= 0) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_if_icmpeq: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                if (a == b) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_if_icmpne: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                if (a != b) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_if_icmplt: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                if (a < b) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_if_icmpge: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                if (a >= b) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_if_icmpgt: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                if (a > b) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_if_icmple: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                int b = stack_pop(stack);
                int a = stack_pop(stack);
                if (a <= b) {
                    pc += (b1 << 8) | b2;
                }
                else {
                    pc += 3;
                }
                break;
            }
            case i_goto: {
                int8_t b1 = code[pc + 1];
                int8_t b2 = code[pc + 2];
                pc += (b1 << 8) | b2;
                break;
            }
            case i_ireturn: {
                int a = stack_pop(stack);
                free(stack->items);
                free(stack);
                optional_value_t result = {.has_value = true, .value = a};
                return result;
            }
            case i_return: {
                free(stack->items);
                free(stack);
                optional_value_t result = {.has_value = false};
                return result;
            }
            case i_invokestatic: {
                // part 1
                u1 b1 = code[pc + 1];
                u1 b2 = code[pc + 2];
                uint16_t pool_index = (b1 << 8) | b2;
                method_t *method2 = find_method_from_index(pool_index, class);

                // part 2
                code_t method_code2 = method2->code;
                int32_t *locals2 = malloc(sizeof(int32_t) * method_code2.max_locals);
                uint16_t parameters = get_number_of_parameters(method2);
                for (uint16_t i = 0; i < parameters; i++) {
                    int item = stack_pop(stack);
                    locals2[parameters - i - 1] = item;
                }

                // part 3, 4
                optional_value_t output = execute(method2, locals2, class, heap);

                // part 5
                if (output.has_value) {
                    stack_push(stack, output.value);
                }
                pc += 3;
                free(locals2);
                break;
            }
            case i_nop: {
                pc++;
                break;
            }
            case i_dup: {
                int a = stack_pop(stack);
                stack_push(stack, a);
                stack_push(stack, a);
                pc++;
                break;
            }
            case i_newarray: {
                u1 b = code[pc + 1];
                (void) b;
                int count = stack_pop(stack);
                int32_t *array = malloc(sizeof(int32_t) * (count + 1));
                array[0] = count;
                for (size_t i = 1; i <= (size_t) count; i++) {
                    array[i] = 0;
                }
                int32_t ref = heap_add(heap, array);
                stack_push(stack, ref);
                pc += 2;
                break;
            }
            case i_arraylength: {
                int32_t ref = stack_pop(stack);
                int32_t *array = heap_get(heap, ref);
                int count = array[0];
                stack_push(stack, count);
                pc++;
                break;
            }
            case i_areturn: {
                int32_t ref = stack_pop(stack);
                free(stack->items);
                free(stack);
                optional_value_t result = {.has_value = true, .value = ref};
                return result;
            }
            case i_iastore: {
                int value = stack_pop(stack);
                int index = stack_pop(stack);
                int32_t ref = stack_pop(stack);
                int32_t *array = heap_get(heap, ref);
                array[index + 1] = value;
                pc++;
                break;
            }
            case i_iaload: {
                int index = stack_pop(stack);
                int32_t ref = stack_pop(stack);
                int32_t *array = heap_get(heap, ref);
                stack_push(stack, array[index + 1]);
                pc++;
                break;
            }
            case i_aload: {
                u1 i = code[pc + 1];
                stack_push(stack, locals[i]);
                pc += 2;
                break;
            }
            case i_astore: {
                u1 i = code[pc + 1];
                int32_t ref = stack_pop(stack);
                locals[i] = ref;
                pc += 2;
                break;
            }
            case i_aload_0 ... i_aload_3: {
                u1 i = instruction - i_aload_0;
                stack_push(stack, locals[i]);
                pc++;
                break;
            }
            case i_astore_0 ... i_astore_3: {
                u1 i = instruction - i_astore_0;
                int32_t ref = stack_pop(stack);
                locals[i] = ref;
                pc++;
                break;
            }
            default: {
                fprintf(stderr, "Unhandled value! %d\n", instruction);
                exit(EXIT_NUMBER);
                break;
            }
        }
    }

    (void) heap;

    // free objects
    free(stack->items);
    free(stack);

    // Return void
    optional_value_t result = {.has_value = false};
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <class file>\n", argv[0]);
        return 1;
    }

    // Open the class file for reading
    FILE *class_file = fopen(argv[1], "r");
    assert(class_file != NULL && "Failed to open file");

    // Parse the class file
    class_file_t *class = get_class(class_file);
    int error = fclose(class_file);
    assert(error == 0 && "Failed to close file");

    // The heap array is initially allocated to hold zero elements.
    heap_t *heap = heap_init();

    // Execute the main method
    method_t *main_method = find_method(MAIN_METHOD, MAIN_DESCRIPTOR, class);
    assert(main_method != NULL && "Missing main() method");
    /* In a real JVM, locals[0] would contain a reference to String[] args.
     * But since TeenyJVM doesn't support Objects, we leave it uninitialized. */
    int32_t locals[main_method->code.max_locals];
    // Initialize all local variables to 0
    memset(locals, 0, sizeof(locals));
    optional_value_t result = execute(main_method, locals, class, heap);
    assert(!result.has_value && "main() should return void");

    // Free the internal data structures
    free_class(class);

    // Free the heap
    heap_free(heap);
}
