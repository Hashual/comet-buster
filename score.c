#include <stdio.h>

void write_score(int score) {
    FILE *f = fopen("data.txt", "w");
    if (f == NULL) {
        perror("Erreur ouverture fichier");
    }
    fprintf(f, "%d\n", score);
    fclose(f);
}

int read_score() {
    int score;
    FILE *f = fopen("data.txt", "r");
    if (f == NULL) {
        perror("Erreur ouverture fichier");
        return -1;
    }
    fscanf(f, "%d", &score);
    fclose(f);
    return score;
}

