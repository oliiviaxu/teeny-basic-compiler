#include "compile.h"
#include <stdio.h>
#include <stdlib.h>

extern void print_int(size_t value);

size_t if_counter = 0;
size_t while_counter = 0;

int64_t get_power_of_two_exponent(int64_t value) {
    size_t power = 0;
    if (value != 0) {
        while (value != 1) {
            if (value % 2 != 0) {
                return 0;
            }
            value /= 2;
            power += 1;
        }
        return power;
    }
    return 0;
}

bool is_constant(node_t *node) {
    switch (node->type) {
        case NUM:
            return true;
        case BINARY_OP: {
            binary_node_t *bin_node = (binary_node_t *)node;
            if (bin_node->op == '+' || bin_node->op == '-' || bin_node->op == '*' || bin_node->op == '/') {
                return is_constant(bin_node->left) && is_constant(bin_node->right);
            }
            break;
        }
        default:
            break;
    }
    return false;
}

int64_t evaluate_constant(node_t *node) {
    if (node->type == NUM) {
        return ((num_node_t *)node)->value;
    }

    binary_node_t *bin_node = (binary_node_t *)node;
    int64_t left_val = evaluate_constant(bin_node->left);
    int64_t right_val = evaluate_constant(bin_node->right);

    switch (bin_node->op) {
        case '+':
            return left_val + right_val;
        case '-':
            return left_val - right_val;
        case '*':
            return left_val * right_val;
        case '/':
            return left_val / right_val;
        default:
            return 0;  
    }
}

bool compile_ast(node_t *node) {
    // compiler will output x86-64 assembly code, move NUM to rdi 
    if (node->type == NUM) {
        num_node_t *num_node = (num_node_t *)node;
        // value_t is an uint64_t 
        printf("mov $%lu, %%rdi\n", num_node->value);
        return true;
    }
    else if (node->type == SEQUENCE) {
        sequence_node_t *sequence = (sequence_node_t *) node;
        for (size_t i = 0; i < sequence->statement_count; i++) {
            compile_ast(sequence->statements[i]);
        }
        return true;
    }
    else if (node->type == PRINT) {
        print_node_t *print_node = (print_node_t *)node;
        compile_ast(print_node->expr);
        printf("call print_int\n");
        return true;
    }
    else if (node->type == BINARY_OP) {
        binary_node_t *bin = (binary_node_t *) node;
        if (is_constant(node)) {
            int64_t val = evaluate_constant(node);
            printf("mov $%lu, %%rdi\n", val);
        } else {
            if (bin->op == '*' && is_constant(bin->right)) {
                int64_t val = evaluate_constant(bin->right);
                int exp = get_power_of_two_exponent(val);
                if (exp != 0) {
                    compile_ast(bin->left);
                    printf("sal $%d, %%rdi\n", exp);
                    return true;
                }
            }

            compile_ast(bin->right);
            printf("pushq %%rdi\n");
            compile_ast(bin->left);
            printf("popq %%r8\n");  

            switch (bin->op) {
                case '+':
                    printf("add %%r8, %%rdi\n");
                    break;
                case '-':
                    printf("subq %%r8, %%rdi\n");
                    break;
                case '*':
                    printf("imulq %%r8, %%rdi\n");
                    break;
                case '/':
                    printf("movq %%rdi, %%rax\n");
                    printf("cqto\n");
                    printf("idiv %%r8\n");
                    printf("movq %%rax, %%rdi\n");
                    break;
                default:
                    printf("cmp %%r8, %%rdi\n");
            }
        }
        return true;
    } else if (node->type == VAR) {
        var_node_t *var_node = (var_node_t *)node;
        // since 26 variables in alphabetical order 
        printf("movq -%d(%%rbp), %%rdi\n", 8 * (var_node->name - 'A' + 1));
        return true;
    } else if (node->type == LET) {
        let_node_t *let_node = (let_node_t *)node;
        compile_ast(let_node->value);
        printf("movq %%rdi, -%d(%%rbp)\n", 8 * (let_node->var - 'A' + 1));
        return true;
    } else if (node->type == IF) {
        if_node_t *if_node = (if_node_t *)node;

        if_counter++;
        int counter = if_counter;

        binary_node_t *bin_node = (binary_node_t *)if_node->condition;
        compile_ast((node_t *)bin_node);
        if (bin_node->op == '<') {
            printf("jnl IF_%d\n", counter);
        } else if (bin_node->op == '=') {
            printf("jne IF_%d\n", counter);
        } else if (bin_node->op == '>') {
            printf("jng IF_%d\n", counter);
        }
        printf("ELSE_%d:\n", counter);
        if (if_node->else_branch != NULL) {
            compile_ast(if_node->else_branch);
        }
        printf("jmp ENDIF%d\n", counter);

        printf("IF_%d:\n", counter);
        compile_ast(if_node->if_branch);
        printf("jmp ENDIF%d\n", counter);

        printf("ENDIF%d:\n", counter);
        return true;
    } else if (node->type == WHILE) {
        while_counter++;
        int counter = while_counter;

        while_node_t *while_node = (while_node_t *)node;
        printf("WHILE_%d:\n", counter);
    
        binary_node_t *bin_node = (binary_node_t *)while_node->condition;
        compile_ast((node_t *)bin_node);

        if (bin_node->op == '=') {
            printf("jne WHILE_END_%d\n", counter);
        } else if (bin_node->op == '>') {
            printf("jng WHILE_END_%d\n", counter);
        } else if (bin_node->op == '<') {
            printf("jnl WHILE_END_%d\n", counter);
        }
        compile_ast(while_node->body);
        printf("jmp WHILE_%d\n", counter);
        printf("WHILE_END_%d:\n", counter);
        return true;
    }
    return false;
}
