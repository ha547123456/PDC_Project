#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <mpi.h>
#include <limits.h>

#define INF 1000000000

int **read_metis_graph(const char *filename, int *num_nodes) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("File open failed");
        exit(1);
    }

    int nodes, edges, fmt = 1;
    if (fscanf(fp, "%d %d %d", &nodes, &edges, &fmt) != 3) {
        fprintf(stderr, "Error reading graph dimensions\n");
        exit(1);
    }
    *num_nodes = nodes;

    // Allocate pointer array
    int **adj = (int **)malloc(nodes * sizeof(int *));
    if (!adj) {
        perror("Memory allocation failed");
        exit(1);
    }

    // Allocate each row
    for (int i = 0; i < nodes; i++) {
        adj[i] = (int *)calloc(nodes, sizeof(int));
        if (!adj[i]) {
            perror("Memory allocation failed");
            exit(1);
        }
    }

    // Read edges
    for (int i = 0; i < nodes; i++) {
        int neighbor, weight;
        while (fscanf(fp, "%d %d", &neighbor, &weight) == 2) {
            if (neighbor < 1 || neighbor > nodes) {
                fprintf(stderr, "Invalid neighbor %d for node %d\n", neighbor, i+1);
                continue;
            }
            adj[i][neighbor - 1] = weight;
            if (getc(fp) == '\n') break;
        }
    }

    fclose(fp);
    return adj;
}

void dijkstra_parallel(int **graph, int src, int n, int *dist) {
    int *visited = (int *)calloc(n, sizeof(int));
    if (!visited) {
        perror("Memory allocation failed");
        exit(1);
    }

    // Initialize distances
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        dist[i] = INF;
    }
    dist[src] = 0;

    for (int count = 0; count < n - 1; count++) {
        int u = -1, min = INF;

        // Find vertex with minimum distance
        #pragma omp parallel for
        for (int i = 0; i < n; i++) {
            if (!visited[i] && dist[i] < min) {
                #pragma omp critical
                {
                    if (dist[i] < min) {
                        min = dist[i];
                        u = i;
                    }
                }
            }
        }

        if (u == -1) break;  // No more reachable vertices
        visited[u] = 1;

        // Update neighbor distances
        #pragma omp parallel for
        for (int v = 0; v < n; v++) {
            if (!visited[v] && graph[u][v] && dist[u] + graph[u][v] < dist[v]) {
                #pragma omp critical
                {
                    if (dist[u] + graph[u][v] < dist[v]) {
                        dist[v] = dist[u] + graph[u][v];
                    }
                }
            }
        }
    }

    free(visited);
}

int main(int argc, char **argv) {
    int rank, size, num_nodes;
    int **graph = NULL;
    int *distances = NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double start_time, end_time;

    // Read graph on rank 0
    if (rank == 0) {
        graph = read_metis_graph("/home/tooba/Downloads/USA-road-d.NY.metis", &num_nodes);
        distances = (int *)malloc(num_nodes * sizeof(int));
        if (!distances) {
            perror("Memory allocation failed");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    // Broadcast graph size
    MPI_Bcast(&num_nodes, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Allocate memory on other ranks
    if (rank != 0) {
        graph = (int **)malloc(num_nodes * sizeof(int *));
        if (!graph) {
            perror("Memory allocation failed");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        for (int i = 0; i < num_nodes; i++) {
            graph[i] = (int *)malloc(num_nodes * sizeof(int));
            if (!graph[i]) {
                perror("Memory allocation failed");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
    }

    // Broadcast graph data
    for (int i = 0; i < num_nodes; i++) {
        MPI_Bcast(graph[i], num_nodes, MPI_INT, 0, MPI_COMM_WORLD);
    }

    // Allocate distances array on all ranks
    distances = (int *)malloc(num_nodes * sizeof(int));
    if (!distances) {
        perror("Memory allocation failed");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Calculate work distribution
    int chunk = num_nodes / size;
    int remainder = num_nodes % size;
    int start = rank * chunk + (rank < remainder ? rank : remainder);
    int end = start + chunk + (rank < remainder ? 1 : 0);

    start_time = MPI_Wtime();

    // Process assigned sources
    for (int src = start; src < end; src++) {
        int *local_dist = (int *)malloc(num_nodes * sizeof(int));
        if (!local_dist) {
            perror("Memory allocation failed");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        dijkstra_parallel(graph, src, num_nodes, local_dist);
        
        // Gather results to rank 0
        MPI_Gather(local_dist, num_nodes, MPI_INT, 
                  rank == 0 ? distances : NULL, 
                  num_nodes, MPI_INT, 
                  0, MPI_COMM_WORLD);
        
        free(local_dist);
    }

    end_time = MPI_Wtime();

    // Output results
    if (rank == 0) {
        printf("Execution Time: %.6f seconds\n", end_time - start_time);
        printf("Shortest distances from sources:\n");
        for (int i = 0; i < num_nodes; i++) {
            if (distances[i] != INF) {
                printf("To node %d: %d\n", i, distances[i]);
            } else {
                printf("To node %d: unreachable\n", i);
            }
        }
    }

    // Clean up
    for (int i = 0; i < num_nodes; i++) {
        free(graph[i]);
    }
    free(graph);
    free(distances);

    MPI_Finalize();
    return 0;
}
