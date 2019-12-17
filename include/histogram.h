#ifndef __HISTOGRAM_H
#define __HISTOGRAM_H

int get_threshold(int *h, int perc);
int get_min(int *h);
int get_mean(int *h, int n);
int get_mode(int *h);
void print_hist(int *h);
int get_n_below(int *h, int threshold);

#endif /* __HISTOGRAM_H */
