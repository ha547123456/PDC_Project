#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <limits.h>

#define MAX_VERTICES 10000
#define INF INT_MAX

typedef struct {
    int src;
    int dest;
    int weight;
} Edge;

int V, E;
Edge *edges;
int *distance;
int *parent;
int *queue;
int front = 0, rear = 0;

void enqueue(int v) {
    queue[rear++] = v;
}

int dequeue() {
    return queue[front++];
}

int is_empty() {
    return front == rear;
}

void initialize_sssp(int source) {
    for (int i = 0; i < V; i++) {
        distance[i] = INF;
        parent[i] = -1;
    }
    distance[source] = 0;
    enqueue(source);
}

void parallel_sssp() {
    while (!is_empty()) {
        int v = dequeue();
        #pragma omp parallel for
        for (int i = 0; i < E; i++) {
            if (edges[i].src == v) {
                int w = edges[i].dest;
                int weight = edges[i].weight;

                if (distance[v] + weight < distance[w]) {
                    #pragma omp critical
                    {
                        if (distance[v] + weight < distance[w]) {
                            distance[w] = distance[v] + weight;
                            parent[w] = v;
                            enqueue(w);
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("Unable to open file");
        return 1;
    }

    fscanf(fp, "%d %d", &V, &E);
    edges = (Edge *)malloc(E * sizeof(Edge));
    distance = (int *)malloc(V * sizeof(int));
    parent = (int *)malloc(V * sizeof(int));
    queue = (int *)malloc(V * sizeof(int));

    for (int i = 0; i < E; i++) {
        fscanf(fp, "%d %d %d", &edges[i].src, &edges[i].dest, &edges[i].weight);
    }

    fclose(fp);

    int source = 0;
    initialize_sssp(source);
    
    double start = omp_get_wtime();
    parallel_sssp();
    double end = omp_get_wtime();

    printf("Execution Time: %f seconds\n", end - start);
    for (int i = 0; i < V; i++) {
        printf("Vertex %d, Distance: %d, Parent: %d\n", i, distance[i], parent[i]);
    }

    free(edges);
    free(distance);
    free(parent);
    free(queue);

    return 0;
}
