#include "analytics.h"

#include <assert.h>


int result_analysis(FILE* istream, FILE* ostream, Clocks_s* clocks, Mode_e mode, AlgorithmEfficiency_s* stats) {
	assert(istream != NULL && ostream != NULL && clocks != NULL);
	assert(mode == RUN_COMPRESS || mode == RUN_DECOMPRESS);


	stats->freq_time = (double)(clocks->stop_freq - clocks->start_total) / CLOCKS_PER_SEC;
	stats->bincode_time = (double)(clocks->stop_bincode - clocks->start_bincode) / CLOCKS_PER_SEC;
	stats->compress_time = (double)(clocks->stop_total - clocks->start_compress) / CLOCKS_PER_SEC;
	stats->total_time = stats->freq_time + stats->bincode_time + stats->compress_time;

	if (fseek(istream, 0, SEEK_END)) {
		perror("Ошибка установления каретки в конечное положение");
		return 0;
	}
	if (fseek(ostream, 0, SEEK_END)) {
		perror("Ошибка установления каретки в конечное положение");
		return 0;
	}

	stats->in_size = ftell(istream);
	stats->out_size = ftell(ostream);

	if (fseek(istream, 0, SEEK_SET)) {
		perror("Ошибка установления каретки в начальное положение");
		return 0;
	}
	if (fseek(ostream, 0, SEEK_SET)) {
		perror("Ошибка установления каретки в начальное положение");
		return 0;
	}

	if (mode == RUN_COMPRESS) {
		stats->coefficient = (double)stats->in_size / stats->out_size;
	}
	if (mode == RUN_DECOMPRESS) {
		stats->coefficient = (double)stats->out_size / stats->in_size;
	}

	return 1;
}