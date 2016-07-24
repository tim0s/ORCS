#ifndef STATISTICS_HPP
#define STATISTICS_HPP

/* Typedefs */
typedef std::vector<int> bucket_t;

/* Prototypes */
void print_raw_data(FILE *fd);
void printbigbucket(FILE *fd);
void print_cable_cong(FILE *fd);
double get_avg_bandwidth();
double get_var_bandwidth(double xq);
double get_max_error(double quantile);
double get_acc_bandwidth(bucket_t *bucket);
void account_stats(bucket_t *bucket);
void print_bucket(FILE *fd, bucket_t *bucket);
void print_statistics_max_congestions(FILE *fd);
double account_stats_max_congestions(double max_congestions);
void print_histogram(FILE *fd);
double *get_results(int *size);
void insert_results(double *buffer, int size);
void add_to_bigbucket(int *buffer, int size);
int *get_bigbucket(int *size);
void insert_into_bucket_maxcon2(cable_cong_map_t *cable_cong, ptrn_t *ptrn, namelist_t *namelist, bucket_t *bucket);
void print_statistics_max_delay(FILE *fd);
void print_raw_data_max_delay(FILE *fd);
void apply_cable_cong_map_to_global_cable_cong_map(cable_cong_map_t *cable_cong);
int get_congestion_by_edgeid(int eid);
int get_max_from_global_cong_map();

#endif
