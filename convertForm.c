#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to convert a DIMACS-format graph to METIS format
void convert_dimacs_to_metis(const char *input_filename, const char *output_filename) {
    FILE *infile = fopen(input_filename, "r");
    if (infile == NULL) {
        fprintf(stderr, "Error opening file %s\n", input_filename);
        return;
    }

    FILE *outfile = fopen(output_filename, "w");
    if (outfile == NULL) {
        fprintf(stderr, "Error opening file %s\n", output_filename);
        fclose(infile);
        return;
    }

    int num_vertices = 0, num_edges = 0;
    int vertex1, vertex2;
    char line[256];

    // First pass: read the graph size
    while (fgets(line, sizeof(line), infile)) {
        if (line[0] == 'p') {
            sscanf(line, "p sp %d %d", &num_vertices, &num_edges);
            break;
        }
    }
    
    if (num_vertices == 0) {
        fprintf(stderr, "Error: Could not read graph size\n");
        fclose(infile);
        fclose(outfile);
        return;
    }

    // Rewind to start of file
    rewind(infile);

    // Allocate adjacency list (using num_vertices + 1 since vertices start at 1)
    int **adj_list = (int **)malloc((num_vertices + 1) * sizeof(int *));
    int *adj_size = (int *)malloc((num_vertices + 1) * sizeof(int));
    int *adj_capacity = (int *)malloc((num_vertices + 1) * sizeof(int));
    
    if (!adj_list || !adj_size || !adj_capacity) {
        fprintf(stderr, "Memory allocation failed!\n");
        fclose(infile);
        fclose(outfile);
        return;
    }

    // Initialize adjacency list
    for (int i = 0; i <= num_vertices; i++) {
        adj_size[i] = 0;
        adj_capacity[i] = 10; // Initial capacity
        adj_list[i] = (int *)malloc(adj_capacity[i] * sizeof(int));
    }

    // Read the file line by line
    while (fgets(line, sizeof(line), infile)) {
        line[strcspn(line, "\n")] = 0;  // Remove newline character

        // Skip comment lines starting with 'c'
        if (line[0] == 'c') continue;

        // Read graph description from 'p' line (already done)
        if (line[0] == 'p') continue;

        // Read the edges from 'a' lines
        if (line[0] == 'a') {
            sscanf(line, "a %d %d", &vertex1, &vertex2);

            // Check if we need to resize the adjacency list for vertex1
            if (adj_size[vertex1] >= adj_capacity[vertex1]) {
                adj_capacity[vertex1] *= 2;
                adj_list[vertex1] = (int *)realloc(adj_list[vertex1], adj_capacity[vertex1] * sizeof(int));
            }
            adj_list[vertex1][adj_size[vertex1]++] = vertex2;

            // Check if we need to resize the adjacency list for vertex2
            if (adj_size[vertex2] >= adj_capacity[vertex2]) {
                adj_capacity[vertex2] *= 2;
                adj_list[vertex2] = (int *)realloc(adj_list[vertex2], adj_capacity[vertex2] * sizeof(int));
            }
            adj_list[vertex2][adj_size[vertex2]++] = vertex1;
        }
    }

    // Write the header (vertices and edges)
    // Note: METIS format counts undirected edges once, so we divide by 2
    fprintf(outfile, "%d %d\n", num_vertices, num_edges / 2);

    // Write the adjacency list in METIS format
    for (int i = 1; i <= num_vertices; i++) {
        for (int j = 0; j < adj_size[i]; j++) {
            fprintf(outfile, "%d ", adj_list[i][j]);
        }
        fprintf(outfile, "\n");
    }

    // Clean up memory and close files
    for (int i = 0; i <= num_vertices; i++) {
        free(adj_list[i]);
    }
    free(adj_list);
    free(adj_size);
    free(adj_capacity);

    fclose(infile);
    fclose(outfile);
    printf("Conversion complete! METIS format saved to %s\n", output_filename);
}

int main() {
    const char *input_file = "USA-road-d.NY.gr";
    const char *output_file = "USA-road-d.NY.metis";
    convert_dimacs_to_metis(input_file, output_file);
    return 0;
}
