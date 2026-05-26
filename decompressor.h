#ifndef CLI_DECOMPRESSOR
#define CLI_DECOMPRESSOR

#include <stdio.h>
/*
	@brief Распаковка файла, сжатого функцией compress_file_v1()
	@param istream - указатель на поток сжатого файла
	@param ostream - указатель на поток для распаковки файла
	@return успех - 1, иначе - 0
*/
int decompress_file_v1(
	FILE* istream,
	FILE* ostream
);

#endif
