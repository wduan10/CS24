#include "compile.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    bool is_constant;
    int64_t val;
} return_t;

return_t fold(node_t *node) {
    if (node->type == NUM) {
        num_node_t *num_node = (num_node_t *) node;
        return_t ret = {.is_constant = true, .val = num_node->value};
        return ret;
    }
    else if (node->type == BINARY_OP) {
        binary_node_t *binary_node = (binary_node_t *) node;
        return_t left = fold(binary_node->left);
        return_t right = fold(binary_node->right);
        if (left.is_constant && right.is_constant) {
            if (binary_node->op == '+') {
                return_t ret = {.is_constant = true, .val = left.val + right.val};
                return ret;
            }
            else if (binary_node->op == '-') {
                return_t ret = {.is_constant = true, .val = left.val - right.val};
                return ret;
            }
            else if (binary_node->op == '*') {
                return_t ret = {.is_constant = true, .val = left.val * right.val};
                return ret;
            }
            else if (binary_node->op == '/') {
                return_t ret = {.is_constant = true, .val = left.val / right.val};
                return ret;
            }
            else {
                return_t ret = {.is_constant = false, .val = 0};
                return ret;
            }
        }
        else {
            return_t ret = {.is_constant = false, .val = 0};
            return ret;
        }
    }
    else {
        return_t ret = {.is_constant = false, .val = 0};
        return ret;
    }
}

int32_t if_id = 0;
int32_t while_id = 0;

int32_t is_power_of_two(int64_t num) {
    int32_t count = 0;
    int32_t power = 0;
    int32_t i = 0;
    while (num > 0) {
        if (num & 1) {
            power = i;
            count++;
        }
        num >>= 1;
        i++;
        if (count > 1) return 0;
    }
    return power;
}

bool compile_ast(node_t *node) {
    return_t ret = fold(node);
    if (ret.is_constant) {
        printf("movq $%ld, %%rdi\n", ret.val);
        return true;
    }
    else if (node->type == NUM) {
        num_node_t *num_node = (num_node_t *) node;
        printf("movq $%ld, %%rdi\n", num_node->value);
        return true;
    }
    else if (node->type == PRINT) {
        print_node_t *print_node = (print_node_t *) node;
        compile_ast(print_node->expr);
        printf("call print_int\n");
        return true;
    }
    else if (node->type == SEQUENCE) {
        sequence_node_t *sequence_node = (sequence_node_t *) node;
        for (size_t i = 0; i < sequence_node->statement_count; i++) {
            compile_ast(sequence_node->statements[i]);
        }
        return true;
    }
    else if (node->type == BINARY_OP) {
        binary_node_t *binary_node = (binary_node_t *) node;
        compile_ast(binary_node->right);
        printf("push %%rdi\n");
        compile_ast(binary_node->left);
        printf("pop %%r10\n");

        if (binary_node->op == '+') {
            printf("addq %%r10, %%rdi\n");
        }
        else if (binary_node->op == '-') {
            printf("subq %%r10, %%rdi\n");
        }
        else if (binary_node->op == '*') {
            if (binary_node->right->type == NUM) {
                num_node_t *num_node = (num_node_t *) binary_node->right;
                int64_t num = num_node->value;
                int32_t power = is_power_of_two(num);
                if (power != 0) {
                    printf("shl $%d, %%rdi\n", power);
                }
                else {
                    printf("imulq %%r10, %%rdi\n");
                }
            }
            else {
                printf("imulq %%r10, %%rdi\n");
            }
        }
        else if (binary_node->op == '/') {
            printf("movq %%rdi, %%rax\n");
            printf("cqto\n");
            printf("idivq %%r10\n");
            printf("movq %%rax, %%rdi\n");
        }
        else {
            printf("cmp %%r10, %%rdi\n");
        }
        return true;
    }
    else if (node->type == VAR) {
        var_node_t *var_node = (var_node_t *) node;
        int32_t offset = var_node->name - 'A' + 1;
        printf("movq %d(%%rbp), %%rdi\n", offset * -8);
        return true;
    }
    else if (node->type == LET) {
        let_node_t *let_node = (let_node_t *) node;
        int32_t offset = let_node->var - 'A' + 1;
        compile_ast(let_node->value);
        printf("movq %%rdi, %d(%%rbp)\n", offset * -8);
        return true;
    }
    else if (node->type == IF) {
        if_node_t *if_node = (if_node_t *) node;
        compile_ast((node_t *) if_node->condition);

        int32_t temp_id = if_id;
        if_id++;

        if (if_node->condition->op == '=') {
            printf("je IF_%d\n", temp_id);
        }
        else if (if_node->condition->op == '<') {
            printf("jl IF_%d\n", temp_id);
        }
        else if (if_node->condition->op == '>') {
            printf("jg IF_%d\n", temp_id);
        }

        // perform else
        if (if_node->else_branch != NULL) {
            compile_ast(if_node->else_branch);
        }
        printf("jmp ENDIF_%d\n", temp_id);

        // perform if
        printf("IF_%d:\n", temp_id);
        compile_ast(if_node->if_branch);
        printf("jmp ENDIF_%d\n", temp_id);

        // end
        printf("ENDIF_%d:\n", temp_id);
        return true;
    }
    else if (node->type == WHILE) {
        while_node_t *while_node = (while_node_t *) node;

        int32_t temp_id = while_id;
        while_id++;

        printf("WHILE_%d:\n", temp_id);
        compile_ast((node_t *) while_node->condition);
        if (while_node->condition->op == '=') {
            printf("jne ENDWHILE_%d\n", temp_id);
        }
        else if (while_node->condition->op == '<') {
            printf("jge ENDWHILE_%d\n", temp_id);
        }
        else if (while_node->condition->op == '>') {
            printf("jle ENDWHILE_%d\n", temp_id);
        }
        compile_ast(while_node->body);
        printf("jmp WHILE_%d\n", temp_id);

        // jump to end
        printf("ENDWHILE_%d:\n", temp_id);
        return true;
    }
    else {
        fprintf(stderr, "\nUnknown node type: %d\n", node->type);
        assert(false);
    }
    return false;
}
