#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include <mpi.h>

#define PARTITIONS_TO_SELECT 10
#define DELETIONS_PER_PARTITION 3
#define MAX_EDGES_PER_VERTEX 50
#define PRINT_INTERVAL 5000

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
bool* PartitionsWithDeletions;
int num_vertices;
int num_edges;
int my_rank;
int num_procs;

void load_graph(const char* graph_path, const char* partition_path) {
    if (my_rank == 0) {
        printf("Loading graph...\n");
        FILE* graph_file = fopen(graph_path, "r");
        FILE* part_file = fopen(partition_path, "r");
        
        int fmt;
        fscanf(graph_file, "%d %d %d", &num_vertices, &num_edges, &fmt);
        printf("Graph header read: %d vertices, %d edges\n", num_vertices, num_edges);
        
        MPI_Bcast(&num_vertices, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&num_edges, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        graph = calloc(num_vertices + 1, sizeof(Vertex));
        int* partitions = malloc(num_vertices * sizeof(int));
        Dist = malloc((num_vertices + 1) * sizeof(int));
        Parent = malloc((num_vertices + 1) * sizeof(int));
        Affected = calloc(num_vertices + 1, sizeof(bool));
        PartitionsWithDeletions = calloc(PARTITIONS_TO_SELECT, sizeof(bool));
        
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
    } else {
        MPI_Bcast(&num_vertices, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&num_edges, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        graph = calloc(num_vertices + 1, sizeof(Vertex));
        Dist = malloc((num_vertices + 1) * sizeof(int));
        Parent = malloc((num_vertices + 1) * sizeof(int));
        Affected = calloc(num_vertices + 1, sizeof(bool));
        PartitionsWithDeletions = calloc(PARTITIONS_TO_SELECT, sizeof(bool));
    }
    
    // Broadcast graph data
    for (int u = 1; u <= num_vertices; u++) {
        MPI_Bcast(&graph[u].degree, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&graph[u].partition, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (graph[u].degree > 0) {
            MPI_Bcast(graph[u].edges, graph[u].degree * sizeof(Edge), MPI_BYTE, 0, MPI_COMM_WORLD);
        }
    }
}

void process_deletions() {
    if (my_rank == 0) {
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
                
                // Mark affected and partition with deletions
                if (Parent[v] == u || Parent[u] == v) {
                    int y = (Dist[u] > Dist[v]) ? u : v;
                    Affected[y] = true;
                    PartitionsWithDeletions[part] = true;
                    if (i % 5 == 0) printf("Marked vertex %d as affected in partition %d\n", y, part);
                }
            }
        }
        printf("Finished processing deletions\n");
    }
    
    // Broadcast deleted edges and affected partitions
    for (int u = 1; u <= num_vertices; u++) {
        for (int e = 0; e < graph[u].degree; e++) {
            int deleted = graph[u].edges[e].deleted ? 1 : 0;
            MPI_Bcast(&deleted, 1, MPI_INT, 0, MPI_COMM_WORLD);
            graph[u].edges[e].deleted = deleted;
        }
    }
    
    MPI_Bcast(PartitionsWithDeletions, PARTITIONS_TO_SELECT, MPI_C_BOOL, 0, MPI_COMM_WORLD);
}

void update_partition(int partition) {
    bool changed;
    int iterations = 0;
    int updates = 0;
    
    do {
        changed = false;
        iterations++;
        
        for (int v = 1; v <= num_vertices; v++) {
            if (graph[v].partition != partition) continue;
            
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
        
        // Synchronize changes for vertices in this partition
        for (int v = 1; v <= num_vertices; v++) {
            if (graph[v].partition == partition) {
                MPI_Bcast(&Dist[v], 1, MPI_INT, my_rank, MPI_COMM_WORLD);
                MPI_Bcast(&Parent[v], 1, MPI_INT, my_rank, MPI_COMM_WORLD);
                MPI_Bcast(&Affected[v], 1, MPI_C_BOOL, my_rank, MPI_COMM_WORLD);
            }
        }
        
    } while (changed && iterations < 10);
    
    printf("[Rank %d] Completed partition %d with %d updates\n", my_rank, partition, updates);
}

void update_affected_vertices() {
    if (my_rank == 0) printf("\nUpdating affected vertices...\n");
    
    // Divide partitions with deletions among processes
    int partitions_per_proc = PARTITIONS_TO_SELECT / num_procs;
    int extra_partitions = PARTITIONS_TO_SELECT % num_procs;
    
    int my_start_part = my_rank * partitions_per_proc;
    int my_end_part = (my_rank + 1) * partitions_per_proc;
    
    if (my_rank < extra_partitions) {
        my_start_part += my_rank;
        my_end_part += my_rank + 1;
    } else {
        my_start_part += extra_partitions;
        my_end_part += extra_partitions;
    }
    
    // Process only partitions with deletions assigned to this process
    for (int part = my_start_part; part < my_end_part; part++) {
        if (PartitionsWithDeletions[part]) {
            update_partition(part);
        }
    }
    
    // Synchronize all distances and parents
    MPI_Allreduce(MPI_IN_PLACE, Dist + 1, num_vertices, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(MPI_IN_PLACE, Parent + 1, num_vertices, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
}

void save_results(const char* output_path) {
    if (my_rank != 0) return;
    
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

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    
    double start_time = MPI_Wtime();
    srand(time(NULL) + my_rank);
    
    if (my_rank == 0) {
        printf("===== Parallel Graph Processing (MPI) =====\n");
        printf("Running with %d processes\n", num_procs);
    }
    
    load_graph("USA-road-d.NY.metis", "USA-road-d.NY.metis.part.25000");
    
    // Initialize
    if (my_rank == 0) printf("\nInitializing data structures...\n");
    for (int i = 1; i <= num_vertices; i++) {
        Dist[i] = INT_MAX;
        Parent[i] = -1;
    }
    Dist[1] = 0;
    
    if (my_rank == 0) printf("\nStarting computation...\n");
    double compute_start = MPI_Wtime();
    process_deletions();
    update_affected_vertices();
    double compute_time = MPI_Wtime() - compute_start;
    
    save_results("output.metis");
    
    if (my_rank == 0) {
        printf("\n===== Results =====\n");
        printf("Compute time: %.4f seconds\n", compute_time);
        printf("Total runtime: %.4f seconds\n", MPI_Wtime() - start_time);
    }
    
    free(graph);
    free(Dist);
    free(Parent);
    free(Affected);
    free(PartitionsWithDeletions);
    
    MPI_Finalize();
    return 0;
}
