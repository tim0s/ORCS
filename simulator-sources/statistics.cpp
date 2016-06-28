/*
 * Copyright (c) 2009 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 *
 * Author(s): Timo Schneider <timoschn@cs.indiana.edu>
 *            Torsten Hoefler <htor@cs.indiana.edu>
 *
 */

#include <stdlib.h>
#include <cmath>
#include <stdio.h>
#include <map>
#include <assert.h>
#include <string.h>
#include <cgraph.h>
#include <gsl/gsl_histogram.h>
#include "pattern_generator.hpp"
#include "simulator.hpp"
#include "statistics.hpp"

std::vector<double> acc_bandwidths;
bucket_t bigbucket;
cable_cong_map_t cable_cong_global;

int *get_bigbucket(int *size) {
	
	int *buffer;

	*size = bigbucket.size();
	buffer = (int *) malloc(bigbucket.size() * sizeof(int));
	for (int i = 0; i<bigbucket.size(); i++) {
		buffer[i] = bigbucket.at(i);
	}
	return buffer;
}

double *get_results(int *size) {
	
	double *results;

	*size = acc_bandwidths.size();
	results = (double *) malloc(acc_bandwidths.size() * sizeof(double));
	for (int i = 0; i<acc_bandwidths.size(); i++) {
		results[i] = acc_bandwidths.at(i);
	}
	return results;
}

void apply_cable_cong_map_to_global_cable_cong_map(cable_cong_map_t *cable_cong) {
	
	cable_cong_map_t::iterator iter;

	for (iter = cable_cong->begin(); iter != cable_cong->end(); iter++) {
		std::pair<cable_cong_map_t::iterator, bool> ret;
		ret = cable_cong_global.insert(std::make_pair(iter->first, iter->second));
		if (ret.second == false) {
			ret.first->second += iter->second;
		}
	}
}

void insert_results(double *buffer, int size) {
	acc_bandwidths.clear();
	for (int i = 0; i < size; i++) {
		acc_bandwidths.push_back(buffer[i]);
	}
}

void add_to_bigbucket(int *buffer, int size) {
	for (int count = 0; count < size; count++) {
		if (bigbucket.size() < size) {bigbucket.resize(size, 0);}
		bigbucket.at(count) += buffer[count];
	}
}

void insert_into_bucket(used_edges_t *edge_list, named_ptrn_t *mapping, bucket_t *bucket) {
	/*
	named_ptrn_t::iterator iter_ptrn;
	for (iter_ptrn = mapping->begin(); iter_ptrn != mapping->end(); iter_ptrn++) {
		// we need to re-generate the route ... this could also be
		// stored in the previous step
		uroute_t route;
		find_route(&route, iter_ptrn->first, iter_ptrn->second);

		int weight;
		get_max_congestion(&route, edge_list, &weight);
		if (bucket->size() < weight + 1) {
			bucket->resize( weight+5, 0);
		}
		bucket->at(weight) = bucket->at(weight) + 1;

		// The smae for bigbucket
		if (bigbucket.size() < weight + 1) {
			bigbucket.resize( weight+5, 0);
		}
		bigbucket.at(weight) = bigbucket.at(weight) + 1;
	}
*/
}

void insert_into_bucket_maxcon(used_edges_t *edge_list, ptrn_t *ptrn, namelist_t *namelist, bucket_t *bucket) {
	/*
	ptrn_t::iterator iter_ptrn;
	for (iter_ptrn = ptrn->begin(); iter_ptrn != ptrn->end(); iter_ptrn++) {
		// we need to re-generate the route ... this could also be
		// stored in the previous step
		uroute_t route;
		find_route(&route, namelist->at(iter_ptrn->first), namelist->at(iter_ptrn->second));

		int weight;
		get_max_congestion(&route, edge_list, &weight);
		if (bucket->size() < weight + 1) {
			bucket->resize( weight+1, 0);
			bucket->at(weight) = 0;
		}
		bucket->at(weight) = bucket->at(weight) + 1;

		// The smae for bigbucket
		if (bigbucket.size() < weight + 1) {
			bigbucket.resize( weight+5, 0);
		}
		bigbucket.at(weight) = bigbucket.at(weight) + 1;
	}
*/
}

void insert_into_bucket_maxcon2(cable_cong_map_t *cable_cong, ptrn_t *ptrn, namelist_t *namelist, bucket_t *bucket) {

	ptrn_t::iterator iter_ptrn;
	for (iter_ptrn = ptrn->begin(); iter_ptrn != ptrn->end(); iter_ptrn++) {
		/* we need to re-generate the route ... this could also be
		* stored in the previous step
		* timos: I don't think so, It would be better to implement a "cache"
		* into find route */
		uroute_t route;
		find_route(&route, namelist->at(iter_ptrn->first), namelist->at(iter_ptrn->second));

		int weight;
		//get_max_congestion(&route, edge_list, &weight);
		get_max_congestion(&route, cable_cong, &weight);
		if (bucket->size() < weight + 1) {
			bucket->resize( weight+10, 0);
			//bucket->at(weight) = 0;
		}
		bucket->at(weight) = bucket->at(weight) + 1;

		/* The smae for bigbucket */
		if (bigbucket.size() < weight + 1) {
			bigbucket.resize( weight+10, 0);
		}
		bigbucket.at(weight) = bigbucket.at(weight) + 1;
	}

}

void print_statistics() {

	int count;
	double min = 1;
	double max = 0;

	for (count=0; count<acc_bandwidths.size(); count++) {
		if (min>acc_bandwidths.at(count)) min = acc_bandwidths.at(count);
		if (max<acc_bandwidths.at(count)) max = acc_bandwidths.at(count);
	}

	std::cout << "Statistical Results\n";
	std::cout << "===================\n";
	std::cout << "Minimal Bandwidth: " << min << "\n";
	std::cout << "Maximal Bandwith " << max << "\n";
	std::cout << "Average Bandwith: " << get_avg_bandwidth() << "\n";
	std::cout << "Bandwith Variance: " << get_var_bandwidth(get_avg_bandwidth()) << "\n";
	std::cout << "Possible Error (99%% confidency): " << get_max_error(2.576) << "\n\n";
	print_histogram(stdout);
	print_bucket(stdout, &bigbucket);
	std::cout << "===================\n\n";
}

void print_vector() {
	
	int count;

	for (count=0; count<acc_bandwidths.size(); count++) {
		std::cout << acc_bandwidths.at(count) << ", ";
	}
	std::cout << "\n";
}

void print_statistics_max_congestions(FILE *fd) {

	int count;
	double min = 99999999;
	double max = 0;

	for (count=0; count<acc_bandwidths.size(); count++) {
		if (min>acc_bandwidths.at(count)) min = acc_bandwidths.at(count);
		if (max<acc_bandwidths.at(count)) max = acc_bandwidths.at(count);
	}

	fprintf(fd, "Statistical Results\n");
	fprintf(fd, "===================\n\n");
	fprintf(fd, "%s %f\n", "Minimal Maximal Congestion:", min);
	fprintf(fd, "%s %f\n", "Maximal Maximal Congestion:", max);
	fprintf(fd, "%s %f\n", "Average Maximal Congestion:", get_avg_bandwidth());
	fprintf(fd, "%s %f\n", "Maximal Congestion Variance:", get_var_bandwidth(get_avg_bandwidth()));
	fprintf(fd, "===================\n\n");
	print_raw_data(fd);
}

void print_statistics_max_delay(FILE *fd) {

	int count;
	double min = 99999999;
	double max = 0;

	for (count=0; count<acc_bandwidths.size(); count++) {
		if (min>acc_bandwidths.at(count)) min = acc_bandwidths.at(count);
		if (max<acc_bandwidths.at(count)) max = acc_bandwidths.at(count);
	}

	fprintf(fd, "Statistical Results\n");
	fprintf(fd, "===================\n\n");
	fprintf(fd, "%s %f\n", "Minimal Delay:", min);
	fprintf(fd, "%s %f\n", "Maximal Delay:", max);
	fprintf(fd, "%s %f\n", "Average Delay:", get_avg_bandwidth());
	fprintf(fd, "%s %f\n", "Delay Variance:", get_var_bandwidth(get_avg_bandwidth()));
	fprintf(fd, "===================\n\n");
	print_raw_data_max_delay(fd);
}

void print_histogram(FILE *fd) {

	int count;
	double min = 9999999;
	double max = 0;

	/*	for (count=0; count<acc_bandwidths.size(); count++) {
		if (min>acc_bandwidths.at(count)) min = acc_bandwidths.at(count);
		if (max<acc_bandwidths.at(count)) max = acc_bandwidths.at(count);
	}
*/
	/*	if (max == min) {
		fprintf(fd, "No Histogramm, all values are the same...\n");
		return;
	}
*/
	//double w = 3.49 * sqrt(get_var_bandwidth(get_avg_bandwidth())) * pow(acc_bandwidths.size(), (double) -1 / 3);
	//fprintf(fd, "%s %f\n", "Histogramm bin width:", w);
	double w=0.05;
	fprintf(fd, "%s %f\n", "Histogramm bin width:", w);
	fprintf(fd, "%s\n", "Fraction of full bandwidt | Number of occurences");
	gsl_histogram *h = gsl_histogram_alloc((size_t)(1.0/w));
	//gsl_histogram_set_ranges_uniform (h, floor(min), ceil(max));
	gsl_histogram_set_ranges_uniform (h, 0, 1.01);
	for (count=0; count<acc_bandwidths.size(); count++) {
		if (gsl_histogram_increment(h, acc_bandwidths.at(count)) != 0) {
			printf("Moo\n");
			printf("%f\n", acc_bandwidths.at(count));
			exit(1);
		}
	}
	gsl_histogram_fprintf(fd, h, "%12.8f", "%5.0f");
	gsl_histogram_free(h);
	std::cout << acc_bandwidths.size() << "\n";
}

void print_raw_data(FILE *fd) {

	int count;
	double min = 9999999;
	double max = 0;
	std::map<double, int> bucket;

	if (max == min) {
		fprintf(fd, "No Histogramm, all values are the same...\n");
		return;
	}

	for (count=0; count < acc_bandwidths.size(); count++) {
		bucket[acc_bandwidths.at(count)]+=1;
	}

	std::map<double, int>::iterator iter;
	for( iter = bucket.begin(); iter != bucket.end(); iter++ ) {
		fprintf(fd, "Congestion sum of %.0f occured %i times.\n", iter->first, iter->second);
	}

}

void print_raw_data_max_delay(FILE *fd) {

	int count;
	double min = 9999999;
	double max = 0;
	std::map<double, int> bucket;

	if (max == min) {
		fprintf(fd, "No Histogramm, all values are the same...\n");
		return;
	}

	for (count=0; count < acc_bandwidths.size(); count++) {
		bucket[acc_bandwidths.at(count)]+=1;
	}

	std::map<double, int>::iterator iter;
	for( iter = bucket.begin(); iter != bucket.end(); iter++ ) {
		fprintf(fd, "Delay of %.0f occured %i times.\n", iter->first, iter->second);
	}

}

double get_avg_bandwidth() {
	
	int count;
	double xq=0;

	for (count=0; count<acc_bandwidths.size(); count++) {
		xq += acc_bandwidths.at(count);
	}
	xq /= acc_bandwidths.size();
	
	return xq;
}

double get_var_bandwidth(double xq) {
	
	int count;
	double sq=0;

	for (count=0; count<acc_bandwidths.size(); count++) {
		sq += pow(acc_bandwidths.at(count) - xq, 2.0);
	}
	sq /= acc_bandwidths.size();

	return sq;
}

double get_max_error(double quantile) {
	
	double sq, xq;
	
	xq = get_avg_bandwidth();
	sq = get_var_bandwidth(xq);
	/* HOW CAN I CALCULATE NORMAL DISTRIBUTION QUANTILES (without PITA)???*/
	double err = quantile * sq / sqrt(acc_bandwidths.size());
	
	return err;
}

double get_acc_bandwidth(bucket_t *bucket) {

	int count;
	double sum=0;
	double res=0;

	for (count=1; count<bucket->size(); count++) {

		assert(bucket->at(0) == 0);
		if (bucket->at(count) > 0) {
			sum += bucket->at(count);
			res += (((double) bucket->at(count)) / count);
		}
	}
	return res/sum;
}

void account_stats(bucket_t *bucket) {
	
	acc_bandwidths.push_back(get_acc_bandwidth(bucket));

	/*	if (acc_bandwidths.size() > 50) {  // The confidence interval we choose only works for n>50
		return get_max_error(2.576);
	}
	else {
		return -1;
	}
*/
}

double account_stats_max_congestions(double max_congestions) {
	
	float xq=0, sq=0;
	int count;
	
	acc_bandwidths.push_back(max_congestions);

	if (acc_bandwidths.size() > 50) {  // The confidence interval we choose only works for n>50
		return get_max_error(2.576);

	}

	else {
		return -1;
	}
}

void printbigbucket(FILE *fd) {
	print_bucket(fd, &bigbucket);
	fprintf(fd, "\nBW: %f\n", get_acc_bandwidth(&bigbucket));
}

void print_cable_cong(FILE *fd) {
	cable_cong_map_t::iterator iter;
	
	fprintf(fd, "\nCable Congestions:\n\n Edge-ID\tacc. cong\n");
	for (iter = cable_cong_global.begin(); iter != cable_cong_global.end(); iter++) {
		fprintf(fd, "%i\t%i\n", iter->first, iter->second);
	}
}

int get_congestion_by_edgeid(int eid) {

	cable_cong_map_t::iterator iter = cable_cong_global.find(eid);
	return iter->second;

}

int get_max_from_global_cong_map() {

	cable_cong_map_t::iterator iter;
	int max=0;

	for (iter = cable_cong_global.begin(); iter != cable_cong_global.end(); iter++) {
		if (iter->second > max) max = iter->second;
	}
	return max;

}

int get_min_from_global_cong_map() {

	cable_cong_map_t::iterator iter;
	int min=-1;

	for (iter = cable_cong_global.begin(); iter != cable_cong_global.end(); iter++) {
		if (min == -1) min = iter->second;
		if (iter->second < min) min = iter->second;
	}
	return min;

}



void print_bucket(FILE *fd, bucket_t *bucket) {

	int count=0;
	int sum=0;

	for (count=0; count<bucket->size(); count++) {
		sum += bucket->at(count);
	}

	for (count=0; count<bucket->size(); count++) {
		if (bucket->at(count) > 0) {
			fprintf(fd, "weight %i: %i of the %i connections (%.2lf%%)\n",
					count, bucket->at(count), sum,  bucket->at(count) / (double) sum * 100);
		}
	}

}
