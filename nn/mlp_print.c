#include "mlp.h"
#include <stdio.h>
#include <stdlib.h>

static void _print_vector_preview(const char *name, const float *v, int n, int limit) {
    int L = min(n, limit);
    printf("  %s [len=%d]: [", name, n);
    for (int i = 0; i < L; ++i) {
        printf("%g", (double)v[i]);
        if (i + 1 < L) printf(", ");
    }
    if (n > L) printf(", ...");
    printf("]\n");
}

static void _print_matrix_preview_rowmajor(const char *name,
                                           const float *m, int rows, int cols,
                                           int max_rows, int max_cols) {
    int R = min(rows, max_rows);
    int C = min(cols, max_cols);
    printf("  %s [shape=%d x %d] (row-major, showing %d x %d):\n",
           name, rows, cols, R, C);
    for (int r = 0; r < R; ++r) {
        printf("    row %d: [", r);
        const float *row = m + (long)r * cols;
        for (int c = 0; c < C; ++c) {
            printf("%g", (double)row[c]);
            if (c + 1 < C) printf(", ");
        }
        if (cols > C) printf(", ...");
        printf("]\n");
    }
    if (rows > R) printf("    ...\n");
}

static void _print_layer_bubbles(const char *label, int nodes, int max_nodes) {
    int shown = min(nodes, max_nodes);
    printf("%s ", label);
    for (int i = 0; i < shown; ++i) printf("o");
    if (nodes > shown) printf(" ...");
    printf("  (%d)\n", nodes);
}

void printMLP(const MLP *net) {
    if (!net) { printf("MLP: (null)\n"); return; }

    printf("=== MLP ===\n");
    printf("layers: %d | input: %d | output: %d\n",
           net->layerCount, net->inputCount, net->outputCount);

    if (net->layerCount > 0 && net->layers) {
        int in0 = net->layers[0].wW;
        printf("topology: %d", in0);
        for (int i = 0; i < net->layerCount; ++i) printf(" -> %d", net->layers[i].wH);
        printf("\n");
    } else {
        printf("topology: (empty)\n");
    }

    printf("\n--- Structure (ASCII) ---\n");
    if (net->layerCount > 0 && net->layers) {
        int in_nodes = net->layers[0].wW;
        _print_layer_bubbles("Input  :", in_nodes, 32);
        for (int i = 0; i < net->layerCount; ++i) {
            printf("   |\n   v\n");
            _print_layer_bubbles("Layer  :", net->layers[i].wH, 32);
        }
    } else {
        printf("(no layers)\n");
    }

    printf("\n--- Layers ---\n");
    for (int i = 0; i < net->layerCount; ++i) {
        const Layer *L = &net->layers[i];
        printf("Layer %d:\n", i);
        printf("  weights: %p  biases: %p  out: %p\n",
               (void*)L->weights, (void*)L->biases, (void*)L->out);
        printf("  dims: out=%d  in=%d  (weights shape = [%d x %d])\n",
               L->wH, L->wW, L->wH, L->wW);

        if (L->weights) _print_matrix_preview_rowmajor("W", L->weights, L->wH, L->wW, 4, 6);
        else            printf("  W: (null)\n");

        if (L->biases)  _print_vector_preview("b", L->biases, L->wH, 8);
        else            printf("  b: (null)\n");

        if (L->out)     _print_vector_preview("out (cached activations)", L->out, L->wH, 8);
        else            printf("  out: (null)\n");

        if (i + 1 < net->layerCount) printf("\n");
    }

    printf("=== end MLP ===\n");
}
