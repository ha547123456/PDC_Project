#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>

#define PARTITIONS_TO_SELECT 10
#define DELETIONS_PER_PARTITION 3
#define MAX_EDGES_PER_VERTEX 50
#define PRINT_INTERVAL 5000  // Print progress every N vertices

typedef struct {
    int v;
    int weight;
    bool is_boundary;
    bool deleted;
} Edge;

typedef struct {
    Edge edges[MAX_EDGES_PER_VERTEX];
    int degree;
    int partition;
} Vertex;

Vertex* graph;
int* Dist;
int* Parent;
bool* Affected;
int num_vertices;
int num_edges;

void load_graph(const char* graph_path, const char* partition_path) {
    printf("Loading graph...\n");
    FILE* graph_file = fopen(graph_path, "r");
    FILE* part_file = fopen(partition_path, "r");
    
    int fmt;
    fscanf(graph_file, "%d %d %d", &num_vertices, &num_edges, &fmt);
    printf("Graph header read: %d vertices, %d edges\n", num_vertices, num_edges);
    
    graph = calloc(num_vertices + 1, sizeof(Vertex));
    int* partitions = malloc(num_vertices * sizeof(int));
    Dist = malloc((num_vertices + 1) * sizeof(int));
    Parent = malloc((num_vertices + 1) * sizeof(int));
    Affected = calloc(num_vertices + 1, sizeof(bool));
    
    printf("Reading partitions...\n");
    for (int i = 0; i < num_vertices; i++) {
        if (i % PRINT_INTERVAL == 0) printf("Reading partition %d/%d\n", i, num_vertices);
        fscanf(part_file, "%d", &partitions[i]);
    }
    
    printf("Building graph structure...\n");
    for (int u = 1; u <= num_vertices; u++) {
        if (u % PRINT_INTERVAL == 0) printf("Processing vertex %d/%d\n", u, num_vertices);
        
        int v, weight;
        while (fscanf(graph_file, "%d %d", &v, &weight) == 2 && v != 0) {
            if (graph[u].degree < MAX_EDGES_PER_VERTEX) {
                graph[u].edges[graph[u].degree] = (Edge){
                    .v = v,
                    .weight = weight,
                    .is_boundary = (partitions[u-1] != partitions[v-1]),
                    .deleted = false
                };
                graph[u].degree++;
            }
        }
        graph[u].partition = partitions[u-1];
    }
    
    fclose(graph_file);
    fclose(part_file);
    free(partitions);
    printf("Graph loaded successfully!\n");
}

void process_deletions() {
    printf("\nProcessing deletions...\n");
    int total_deletions = PARTITIONS_TO_SELECT * DELETIONS_PER_PARTITION;
    
    for (int i = 0; i < total_deletions; i++) {
        if (i % 5 == 0) printf("Deletion %d/%d\n", i+1, total_deletions);
        
        int part = i % PARTITIONS_TO_SELECT;
        int u;
        int attempts = 0;
        
        do {
            u = 1 + rand() % num_vertices;
            attempts++;
            if (attempts > num_vertices) {
                printf("Warning: Can't find vertex in partition %d with edges\n", part);
                break;
            }
        } while (graph[u].partition != part || graph[u].degree == 0);
        
        if (graph[u].degree > 0) {
            int e = rand() % graph[u].degree;
            graph[u].edges[e].deleted = true;
            
            // Mark both directions
            int v = graph[u].edges[e].v;
            for (int k = 0; k < graph[v].degree; k++) {
                if (graph[v].edges[k].v == u) {
                    graph[v].edges[k].deleted = true;
                    break;
                }
            }
            
            // Mark affected
            if (Parent[v] == u || Parent[u] == v) {
                int y = (Dist[u] > Dist[v]) ? u : v;
                Affected[y] = true;
                if (i % 5 == 0) printf("Marked vertex %d as affected\n", y);
            }
        }
    }
    printf("Finished processing deletions\n");
}

void update_affected_vertices() {
    printf("\nUpdating affected vertices...\n");
    bool changed;
    int iterations = 0;
    int updates = 0;
    
    do {
        changed = false;
        iterations++;
        printf("Iteration %d: ", iterations);
        
        for (int v = 1; v <= num_vertices; v++) {
            if (v % PRINT_INTERVAL == 0) printf(".");
            
            if (Affected[v]) {
                for (int e = 0; e < graph[v].degree; e++) {
                    if (graph[v].edges[e].deleted) continue;
                    
                    int u = graph[v].edges[e].v;
                    int new_dist = Dist[v] + graph[v].edges[e].weight;
                    
                    if (new_dist < Dist[u]) {
                        Dist[u] = new_dist;
                        Parent[u] = v;
                        Affected[u] = true;
                        changed = true;
                        updates++;
                    }
                }
                Affected[v] = false;
            }
        }
        printf("\n");
    } while (changed && iterations < 10);
    
    printf("Completed %d iterations with %d distance updates\n", iterations, updates);
}

void save_results(const char* output_path) {
    printf("\nSaving results to %s...\n", output_path);
    FILE* out = fopen(output_path, "w");
    
    int remaining_edges = 0;
    for (int u = 1; u <= num_vertices; u++) {
        if (u % PRINT_INTERVAL == 0) printf("Counting edges for vertex %d/%d\n", u, num_vertices);
        for (int e = 0; e < graph[u].degree; e++) {
            if (!graph[u].edges[e].deleted) remaining_edges++;
        }
    }
    
    fprintf(out, "%d %d 1\n", num_vertices, remaining_edges/2);
    printf("Writing graph data...\n");
    
    for (int u = 1; u <= num_vertices; u++) {
        if (u % PRINT_INTERVAL == 0) printf("Writing vertex %d/%d\n", u, num_vertices);
        for (int e = 0; e < graph[u].degree; e++) {
            if (!graph[u].edges[e].deleted && u < graph[u].edges[e].v) {
                fprintf(out, "%d %d ", graph[u].edges[e].v, graph[u].edges[e].weight);
            }
        }
        fprintf(out, "\n");
    }
    fclose(out);
    printf("Results saved successfully!\n");
}

int main() {
    clock_t start = clock();
    srand(time(NULL));
    
    printf("===== Sequential Graph Processing =====\n");
    load_graph("USA-road-d.NY.metis", "USA-road-d.NY.metis.part.25000");
    
    // Initialize
    printf("\nInitializing data structures...\n");
    for (int i = 1; i <= num_vertices; i++) {
        Dist[i] = INT_MAX;
        Parent[i] = -1;
    }
    Dist[1] = 0;
    
    printf("\nStarting computation...\n");
    clock_t compute_start = clock();
    process_deletions();
    update_affected_vertices();
    double compute_time = (double)(clock() - compute_start) / CLOCKS_PER_SEC;
    
    save_results("output.metis");
    
    printf("\n===== Results =====\n");
    printf("Compute time: %.4f seconds\n", compute_time);
    printf("Total runtime: %.4f seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);
    
    free(graph);
    free(Dist);
    free(Parent);
    free(Affected);
    
    return 0;
}
