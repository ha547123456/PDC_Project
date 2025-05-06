/ sssp_mpi.c
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <mpi.h>

#define MAX_NODES 300000
#define INF 1000000000

typedef struct Edge {
    int dest;
    int weight;
    struct Edge* next;
} Edge;

typedef struct {
    Edge* head;
} AdjList;

AdjList graph[MAX_NODES];
int num_nodes;

void add_edge(int src, int dest, int weight) {
    Edge* newEdge = (Edge*)malloc(sizeof(Edge));
    newEdge->dest = dest;
    newEdge->weight = weight;
    newEdge->next = graph[src].head;
    graph[src].head = newEdge;
}

void read_dimacs_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(1);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'c') continue;

        if (line[0] == 'p') {
            sscanf(line, "p sp %d %*d", &num_nodes);
        } else if (line[0] == 'a') {
            int src, dest, weight;
            sscanf(line, "a %d %d %d", &src, &dest, &weight);
            add_edge(src, dest, weight);
        }
    }

    fclose(file);
}

void dijkstra_parallel(int source, int* dist, int rank, int size) {
    int visited[MAX_NODES] = {0};

    for (int i = 0; i <= num_nodes; i++)
        dist[i] = INF;
    dist[source] = 0;

    for (int i = 1; i <= num_nodes; i++) {
        int local_min = INF, local_u = -1;

        for (int j = 1 + rank; j <= num_nodes; j += size) {
            if (!visited[j] && dist[j] < local_min) {
                local_min = dist[j];
                local_u = j;
            }
        }

        struct {
            int dist;
            int node;
        } local_data = {local_min, local_u}, global_data;

        MPI_Allreduce(&local_data, &global_data, 1, MPI_2INT, MPI_MINLOC, MPI_COMM_WORLD);

        int u = global_data.node;
        if (u == -1) break;

        visited[u] = 1;

        for (Edge* e = graph[u].head; e; e = e->next) {
            int v = e->dest;
            int w = e->weight;
            if (!visited[v] && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
            }
        }

        // Broadcast updated distances to all processes
        MPI_Bcast(dist, num_nodes + 1, MPI_INT, 0, MPI_COMM_WORLD);
    }
}

int main(int argc, char** argv) {
    int rank, size;

    if (argc != 2) {
        printf("Usage: %s <dimacs_file>\n", argv[0]);
        return 1;
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0)
        printf("Reading graph from file: %s\n", argv[1]);

    read_dimacs_file(argv[1]);

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    int* dist = (int*)malloc((num_nodes + 1) * sizeof(int));
    dijkstra_parallel(1, dist, rank, size);

    MPI_Barrier(MPI_COMM_WORLD);
    double end = MPI_Wtime();

    if (rank == 0) {
        printf("MPI execution time: %.6f seconds\n", end - start);
        for (int i = 1; i <= 20; i++) {
            printf("Distance to node %d: %d\n", i, dist[i]);
        }
    }

    free(dist);
    MPI_Finalize();
    return 0;
}

