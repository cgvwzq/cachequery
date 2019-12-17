#include "../include/histogram.h"

#include <linux/vmalloc.h>

#include "../config/settings.h"
#include "../include/cachequery.h"

int get_threshold(int *h, int perc)
{
	size_t sum_h = 0, p_sum_h = 0;
	int i;
	for (i = 0; i < MAX_TIME; i++)
	{
		sum_h += h[i];
	}
	if (sum_h == 0)
	{
		return -1;
	}
	for (i = 0; i < MAX_TIME; i++)
	{
		p_sum_h += h[i];
		if ((100 * p_sum_h / sum_h) >= perc)
		{
			return i;
		}
	}
	return -1;
}

int get_min(int *h)
{
	int i;
	for (i = 0; i < MAX_TIME; i++)
	{
		if (h[i] > 0)
		{
			return i;
		}
	}
	return i;
}

int get_mean(int *h, int n)
{
	size_t sum = 0;
	int i;
	for (i = 0; i < MAX_TIME; i++)
	{
		sum += i * h[i];
	}
	return sum / n;
}

int get_mode(int *h)
{
	int i, j = 0, m = 0;
	for (i = 0; i < MAX_TIME; i++)
	{
		if (h[i] > m)
		{
			m = h[i];
			j = i;
		}
	}
	return j;
}

void print_hist(int *h)
{
	char *out;
	int  i, l = 0;
	out = vmalloc (12096);
	for (i=0; i<MAX_TIME && l < 11000; i++)
	{
		if (h[i] > 0)
		{
			l += sprintf (&out[l], "%d(%d) ", i, h[i]);
		}
	}
	PRINT ("[debug] %s\n", out);
	vfree (out);
}

int get_n_below(int *h, int threshold)
{
       int i, ret = 0;
       for (i = 0; i < MIN(threshold, MAX_TIME); i++)
       {
               ret += h[i];
       }
       return ret;
}
