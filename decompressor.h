#ifndef CLI_DECOMPRESSOR
#define CLI_DECOMPRESSOR

#include <stdio.h>


/*
	@brief Чтение служебной информации для корректной распаковки файла.
	Можно указать NULL для аргументов, чтобы пропустить их чтение.
	@param istream - поток для чтения
	@param extension - строка для чтения расширения исходного файла
	@param file_size - размер исходного файла в байтах
	@param freq_count - массив с распределением частот символов (для построения дерева Хаффмана)
	@return 0 - успешное чтение, 1 - не прочитано расширение, 2 - не прочитан размер, 3 - не прочитаны частоты,
	-1 - неудачное выставление каретки
*/
int read_overhead(FILE* istream, char extension[MAX_EXT_LENGTH], size_t* file_size, size_t freq_count[ASCII_ALP_SIZE]);

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
