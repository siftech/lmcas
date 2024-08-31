#include "common.h"
#include <alloca.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct int_list {
  int head;
  struct int_list *tail;
};

int global_int = 0;           // A
char *global_argv_str = NULL; // B
char *global_heap_str = NULL; // C

size_t global_heap_int_vector_cap = 0, global_heap_int_vector_len = 0;
int *global_heap_int_vector = NULL;           // D
struct int_list *global_heap_int_list = NULL; // E

struct thing {
  int int_;       // F, G ; H, I
  char *argv_str; // J, K ; L, M
  char *heap_str; // N, O ; P, Q
  size_t heap_int_vector_cap, heap_int_vector_len;
  int *heap_int_vector;           // R, S ; T, U
  struct int_list *heap_int_list; // V, W ; X, Y
  unsigned int bitfield_1 : 3;    // Z, a ; b, c
  unsigned int bitfield_2 : 5;    // d, e ; f, g
};

struct thing global_array_of_structs[2] = {0};

static void push_vector(int **ptr, size_t *cap, size_t *len, int value);
static void print_vector(int *ptr, size_t len);
static void print_list(struct int_list *link);

int main(int argc, char **argv) {
  struct thing stack_array_of_structs[2] = {0};

  int stack_int = 0;           // h
  char *stack_argv_str = NULL; // i
  char *stack_heap_str = NULL; // j
  size_t stack_alloca_int_vector_cap = 1, stack_alloca_int_vector_len = 0;
  int *stack_alloca_int_vector = alloca(sizeof(int)); // k
  size_t stack_flexible_int_vector_cap = 0, stack_flexible_int_vector_len = 0;
  int *stack_flexible_int_vector = NULL; // l
  size_t stack_heap_int_vector_cap = 0, stack_heap_int_vector_len = 0;
  int *stack_heap_int_vector = NULL;             // m
  struct int_list *stack_alloca_int_list = NULL; // n
  struct int_list *stack_heap_int_list = NULL;   // o

  int opt;
  const char *opts = "A:B:C:D:E:F:G:H:I:J:K:L:M:N:O:P:Q:R:S:T:U:V:W:X:Y:Z:"
                     "a:b:c:d:e:f:g:h:i:j:k:l:m:n:o:";
  // Count the amount of space needed for -l flags.
  while ((opt = getopt(argc, argv, opts)) != -1) {
    if (opt == 'l') {
      stack_flexible_int_vector_cap++;
    }
  }
  optind = 1;
  int stack_flexible_int_vector_array[stack_flexible_int_vector_cap];
  stack_flexible_int_vector = &stack_flexible_int_vector_array[0];

  while ((opt = getopt(argc, argv, opts)) != -1) {
    switch (opt) {
    case 'A':
      assert(sscanf(optarg, "%d", &global_int) == 1);
      break;
    case 'B':
      global_argv_str = optarg;
      break;
    case 'C':
      global_heap_str = strdup(optarg);
      break;
    case 'D': {
      int value;
      assert(sscanf(optarg, "%d", &value) == 1);
      push_vector(&global_heap_int_vector, &global_heap_int_vector_cap,
                  &global_heap_int_vector_len, value);
    } break;
    case 'E': {
      struct int_list *link = malloc(sizeof(*link));
      assert(link);

      assert(sscanf(optarg, "%d", &link->head) == 1);
      link->tail = global_heap_int_list;
      global_heap_int_list = link;
    }; break;
    case 'F':
    case 'G':
      assert(sscanf(optarg, "%d", &global_array_of_structs[opt - 'F'].int_) ==
             1);
      break;
    case 'H':
    case 'I':
      assert(sscanf(optarg, "%d", &stack_array_of_structs[opt - 'H'].int_) ==
             1);
      break;
    case 'J':
    case 'K':
      global_array_of_structs[opt - 'J'].argv_str = optarg;
      break;
    case 'L':
    case 'M':
      stack_array_of_structs[opt - 'L'].argv_str = optarg;
      break;
    case 'N':
    case 'O':
      global_array_of_structs[opt - 'N'].heap_str = strdup(optarg);
      break;
    case 'P':
    case 'Q':
      stack_array_of_structs[opt - 'P'].heap_str = strdup(optarg);
      break;
    case 'R':
    case 'S': {
      int value;
      assert(sscanf(optarg, "%d", &value) == 1);
      push_vector(&global_array_of_structs[opt - 'R'].heap_int_vector,
                  &global_array_of_structs[opt - 'R'].heap_int_vector_cap,
                  &global_array_of_structs[opt - 'R'].heap_int_vector_len,
                  value);
    } break;
    case 'T':
    case 'U': {
      int value;
      assert(sscanf(optarg, "%d", &value) == 1);
      push_vector(&stack_array_of_structs[opt - 'T'].heap_int_vector,
                  &stack_array_of_structs[opt - 'T'].heap_int_vector_cap,
                  &stack_array_of_structs[opt - 'T'].heap_int_vector_len,
                  value);
    } break;
    case 'V':
    case 'W': {
      struct int_list *link = malloc(sizeof(*link));
      assert(link);

      assert(sscanf(optarg, "%d", &link->head) == 1);
      link->tail = global_array_of_structs[opt - 'V'].heap_int_list;
      global_array_of_structs[opt - 'V'].heap_int_list = link;
    }; break;
    case 'X':
    case 'Y': {
      struct int_list *link = malloc(sizeof(*link));
      assert(link);

      assert(sscanf(optarg, "%d", &link->head) == 1);
      link->tail = stack_array_of_structs[opt - 'X'].heap_int_list;
      stack_array_of_structs[opt - 'X'].heap_int_list = link;
    }; break;
    case 'Z': {
      int tmp;
      assert(sscanf(optarg, "%d", &tmp) == 1);
      global_array_of_structs[0].bitfield_1 = tmp;
    }; break;
    case 'a': {
      int tmp;
      assert(sscanf(optarg, "%d", &tmp) == 1);
      global_array_of_structs[1].bitfield_1 = tmp;
    }; break;
    case 'b':
    case 'c': {
      int tmp;
      assert(sscanf(optarg, "%d", &tmp) == 1);
      stack_array_of_structs[opt - 'b'].bitfield_1 = tmp;
    }; break;
    case 'd':
    case 'e': {
      int tmp;
      assert(sscanf(optarg, "%d", &tmp) == 1);
      global_array_of_structs[opt - 'd'].bitfield_2 = tmp;
    }; break;
    case 'f':
    case 'g': {
      int tmp;
      assert(sscanf(optarg, "%d", &tmp) == 1);
      stack_array_of_structs[opt - 'f'].bitfield_2 = tmp;
    }; break;
    case 'h':
      assert(sscanf(optarg, "%d", &stack_int) == 1);
      break;
    case 'i':
      stack_argv_str = optarg;
      break;
    case 'j':
      stack_heap_str = strdup(optarg);
      break;
    case 'k': {
      if (stack_alloca_int_vector_cap == stack_alloca_int_vector_len) {
        stack_alloca_int_vector_cap *= 2;
        int *new_ptr = alloca(sizeof(int) * stack_alloca_int_vector_cap);
        memcpy(new_ptr, stack_alloca_int_vector,
               sizeof(int) * stack_alloca_int_vector_len);
        stack_alloca_int_vector = new_ptr;
      }
      int value;
      assert(sscanf(optarg, "%d", &value) == 1);
      stack_alloca_int_vector[stack_alloca_int_vector_len++] = value;
    } break;
    case 'l': {
      int value;
      assert(sscanf(optarg, "%d", &value) == 1);
      stack_flexible_int_vector[stack_flexible_int_vector_len++] = value;
    } break;
    case 'm': {
      int value;
      assert(sscanf(optarg, "%d", &value) == 1);
      push_vector(&stack_heap_int_vector, &stack_heap_int_vector_cap,
                  &stack_heap_int_vector_len, value);
    } break;
    case 'n': {
      struct int_list *link = alloca(sizeof(*link));
      assert(link);

      assert(sscanf(optarg, "%d", &link->head) == 1);
      link->tail = stack_alloca_int_list;
      stack_alloca_int_list = link;
    }; break;
    case 'o': {
      struct int_list *link = malloc(sizeof(*link));
      assert(link);

      assert(sscanf(optarg, "%d", &link->head) == 1);
      link->tail = stack_heap_int_list;
      stack_heap_int_list = link;
    }; break;
    default:
      fprintf(stderr, "Bad usage; see the src, it's complicated\n");
      return -1;
    }
  }

  printf("A (global_int): %d\n", global_int);
  printf("B (global_argv_str): %p, %s\n", (void *)global_argv_str,
         global_argv_str);
  printf("C (global_heap_str): %p, %s\n", (void *)global_heap_str,
         global_heap_str);

  printf("D (global_heap_int_vector):");
  print_vector(global_heap_int_vector, global_heap_int_vector_len);

  printf("E (global_heap_int_list):");
  print_list(global_heap_int_list);

  printf("F (global_array_of_structs[0].int_): %d\n",
         global_array_of_structs[0].int_);
  printf("G (global_array_of_structs[1].int_): %d\n",
         global_array_of_structs[1].int_);
  printf("H (stack_array_of_structs[0].int_): %d\n",
         stack_array_of_structs[0].int_);
  printf("I (stack_array_of_structs[1].int_): %d\n",
         stack_array_of_structs[1].int_);

  printf("J (global_array_of_structs[0].argv_str): %p %s\n",
         (void *)global_array_of_structs[0].argv_str,
         global_array_of_structs[0].argv_str);
  printf("K (global_array_of_structs[1].argv_str): %p %s\n",
         (void *)global_array_of_structs[1].argv_str,
         global_array_of_structs[1].argv_str);
  printf("L (stack_array_of_structs[0].argv_str): %p %s\n",
         (void *)stack_array_of_structs[0].argv_str,
         stack_array_of_structs[0].argv_str);
  printf("M (stack_array_of_structs[1].argv_str): %p %s\n",
         (void *)stack_array_of_structs[1].argv_str,
         stack_array_of_structs[1].argv_str);

  printf("N (global_array_of_structs[0].heap_str): %p %s\n",
         (void *)global_array_of_structs[0].heap_str,
         global_array_of_structs[0].heap_str);
  printf("O (global_array_of_structs[1].heap_str): %p %s\n",
         (void *)global_array_of_structs[1].heap_str,
         global_array_of_structs[1].heap_str);
  printf("P (stack_array_of_structs[0].heap_str): %p %s\n",
         (void *)stack_array_of_structs[0].heap_str,
         stack_array_of_structs[0].heap_str);
  printf("Q (stack_array_of_structs[1].heap_str): %p %s\n",
         (void *)stack_array_of_structs[1].heap_str,
         stack_array_of_structs[1].heap_str);

  printf("R (global_array_of_structs[0].heap_int_vector):");
  print_vector(global_array_of_structs[0].heap_int_vector,
               global_array_of_structs[0].heap_int_vector_len);
  printf("S (global_array_of_structs[1].heap_int_vector):");
  print_vector(global_array_of_structs[1].heap_int_vector,
               global_array_of_structs[1].heap_int_vector_len);
  printf("T (stack_array_of_structs[0].heap_int_vector):");
  print_vector(stack_array_of_structs[0].heap_int_vector,
               stack_array_of_structs[0].heap_int_vector_len);
  printf("U (stack_array_of_structs[1].heap_int_vector):");
  print_vector(stack_array_of_structs[1].heap_int_vector,
               stack_array_of_structs[1].heap_int_vector_len);

  printf("V (global_array_of_structs[0].heap_int_list):");
  print_list(global_array_of_structs[0].heap_int_list);
  printf("W (global_array_of_structs[1].heap_int_list):");
  print_list(global_array_of_structs[1].heap_int_list);
  printf("X (stack_array_of_structs[0].heap_int_list):");
  print_list(stack_array_of_structs[0].heap_int_list);
  printf("Y (stack_array_of_structs[1].heap_int_list):");
  print_list(stack_array_of_structs[1].heap_int_list);

  printf("Z (global_array_of_structs[0].bitfield_1): %d\n",
         global_array_of_structs[0].bitfield_1);
  printf("a (global_array_of_structs[1].bitfield_1): %d\n",
         global_array_of_structs[1].bitfield_1);
  printf("b (stack_array_of_structs[0].bitfield_1): %d\n",
         stack_array_of_structs[0].bitfield_1);
  printf("c (stack_array_of_structs[1].bitfield_1): %d\n",
         stack_array_of_structs[1].bitfield_1);

  printf("d (global_array_of_structs[0].bitfield_2): %d\n",
         global_array_of_structs[0].bitfield_2);
  printf("e (global_array_of_structs[1].bitfield_2): %d\n",
         global_array_of_structs[1].bitfield_2);
  printf("f (stack_array_of_structs[0].bitfield_2): %d\n",
         stack_array_of_structs[0].bitfield_2);
  printf("g (stack_array_of_structs[1].bitfield_2): %d\n",
         stack_array_of_structs[1].bitfield_2);

  printf("h (stack_int): %d\n", stack_int);
  printf("i (stack_argv_str): %p, %s\n", (void *)stack_argv_str,
         stack_argv_str);
  printf("j (stack_heap_str): %p, %s\n", (void *)stack_heap_str,
         stack_heap_str);

  printf("k (stack_alloca_int_vector):");
  print_vector(stack_alloca_int_vector, stack_alloca_int_vector_len);
  printf("l (stack_flexible_int_vector):");
  print_vector(stack_flexible_int_vector, stack_flexible_int_vector_len);
  printf("m (stack_heap_int_vector):");
  print_vector(stack_heap_int_vector, stack_heap_int_vector_len);

  printf("n (stack_alloca_int_list):");
  print_list(stack_alloca_int_list);
  printf("o (stack_heap_int_list):");
  print_list(stack_heap_int_list);

  return 0;
}

static void push_vector(int **ptr, size_t *cap, size_t *len, int value) {
  assert(ptr);
  assert(cap);
  assert(len);

  if (*cap == 0) {
    assert(!*ptr);
    *cap = 16;
    *ptr = malloc(sizeof(int) * *cap);
  } else if (*cap == *len) {
    *cap *= 2;
    *ptr = realloc(*ptr, sizeof(int) * *cap);
  }
  assert(*ptr);
  (*ptr)[(*len)++] = value;
}

static void print_vector(int *ptr, size_t len) {
  printf(" <%zu>", len);
  for (size_t i = 0; i < len; i++)
    printf(" %d", ptr[i]);
  printf("\n");
}

static void print_list(struct int_list *link) {
  while (link) {
    printf(" %d", link->head);
    link = link->tail;
  }
  printf("\n");
}
