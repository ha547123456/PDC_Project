#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int neighbor;
    int weight;
} Edge;

void convert_dimacs_to_metis(const char *input_filename, const char *output_filename) {
    FILE *infile = fopen(input_filename, "r");
    if (!infile) {
        fprintf(stderr, "Error opening input file %s\n", input_filename);
        return;
    }

    FILE *outfile = fopen(output_filename, "w");
    if (!outfile) {
        fprintf(stderr, "Error opening output file %s\n", output_filename);
        fclose(infile);
        return;
    }

    int num_vertices = 0, num_edges = 0;
    char line[256];

    // First pass: Read number of vertices and edges
    while (fgets(line, sizeof(line), infile)) {
        if (line[0] == 'p') {
            sscanf(line, "p sp %d %d", &num_vertices, &num_edges);
            break;
        }
    }

    if (num_vertices == 0 || num_edges == 0) {
        fprintf(stderr, "Invalid graph size in DIMACS file\n");
        fclose(infile);
        fclose(outfile);
        return;
    }

    // Allocate adjacency lists (1-based indexing)
    Edge **adj_list = (Edge **)malloc((num_vertices + 1) * sizeof(Edge *));
    int *adj_size = (int *)malloc((num_vertices + 1) * sizeof(int));
    int *adj_capacity = (int *)malloc((num_vertices + 1) * sizeof(int));

    if (!adj_list || !adj_size || !adj_capacity) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(infile);
        fclose(outfile);
        return;
    }

    // Initialize adjacency lists
    for (int i = 1; i <= num_vertices; i++) {
        adj_capacity[i] = 10;  // Initial capacity
        adj_list[i] = (Edge *)malloc(adj_capacity[i] * sizeof(Edge));
        adj_size[i] = 0;
    }

    // Second pass: Read edges
    rewind(infile);
    while (fgets(line, sizeof(line), infile)) {
        if (line[0] != 'a') continue;

        int u, v, w;
        sscanf(line, "a %d %d %d", &u, &v, &w);

        // Resize if needed
        if (adj_size[u] >= adj_capacity[u]) {
            adj_capacity[u] *= 2;
            adj_list[u] = (Edge *)realloc(adj_list[u], adj_capacity[u] * sizeof(Edge));
        }

        // Add edge (METIS will handle symmetry later)
        adj_list[u][adj_size[u]].neighbor = v;
        adj_list[u][adj_size[u]++].weight = w;
    }

    // Write METIS header (undirected edges = num_edges/2, format=1 for weights)
    fprintf(outfile, "%d %d 1\n", num_vertices, num_edges / 2);

    // Write adjacency lists with weights
    for (int i = 1; i <= num_vertices; i++) {
        for (int j = 0; j < adj_size[i]; j++) {
            fprintf(outfile, "%d %d ", adj_list[i][j].neighbor, adj_list[i][j].weight);
        }
        fprintf(outfile, "\n");
    }

    // Cleanup
    for (int i = 1; i <= num_vertices; i++) {
        free(adj_list[i]);
    }
    free(adj_list);
    free(adj_size);
    free(adj_capacity);

    fclose(infile);
    fclose(outfile);
    printf("Converted %s to METIS format (edge-weighted) in %s\n", input_filename, output_filename);
}

int main() {
    const char *input_file = "USA-road-d.NY.gr";
    const char *output_file = "USA-road-d.NY.metis";
    convert_dimacs_to_metis(input_file, output_file);
    return 0;
}
