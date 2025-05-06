// sssp_sequential.c
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

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

void dijkstra(int source, int* dist) {
    int visited[MAX_NODES] = {0};

    for (int i = 0; i <= num_nodes; i++)
        dist[i] = INF;

    dist[source] = 0;

    for (int i = 1; i <= num_nodes; i++) {
        int u = -1, min_dist = INF;

        for (int j = 1; j <= num_nodes; j++) {
            if (!visited[j] && dist[j] < min_dist) {
                min_dist = dist[j];
                u = j;
            }
        }

        if (u == -1) break;

        visited[u] = 1;

        for (Edge* e = graph[u].head; e; e = e->next) {
            int v = e->dest;
            int w = e->weight;
            if (!visited[v] && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <dimacs_file>\n", argv[0]);
        return 1;
    }

    read_dimacs_file(argv[1]);

    int* dist = (int*)malloc((num_nodes + 1) * sizeof(int));
    double start = clock() / (double)CLOCKS_PER_SEC;

    dijkstra(1, dist);

    double end = clock() / (double)CLOCKS_PER_SEC;

    printf("Sequential execution time: %.6f seconds\n", end - start);

    for (int i = 1; i <= 20; i++) {
        printf("Distance to node %d: %d\n", i, dist[i]);
    }

    free(dist);
    return 0;
}
