#ifndef HUF_DECOMPRESSOR
#define HUF_DECOMPRESSOR

#include "analytics.h"

#include <stdio.h>



/*
	@brief Чтение служебной информации для корректной распаковки файла.
	@brief Можно указать NULL для аргументов, чтобы пропустить их чтение.
	@param istream - поток для чтения
	@param extension - строка для чтения расширения исходного файла
	@param file_size - размер исходного файла в байтах
	@param freq_count - массив с распределением частот символов (для построения дерева Хаффмана)
	@return 0 - успешное чтение, 1 - не прочитано расширение, 2 - не прочитан размер, 3 - не прочитаны частоты,
	-1 - неудачное выставление каретки
*/
int read_overhead(FILE* istream, char extension[MAX_EXT_LENGTH], size_t* file_size, size_t freq_count[ASCII_ALP_SIZE]);

/*
	@brief Генерация пути к распакованному файлу
	@brief Копируется путь сжатого файла, а затем вместо расширения архива ставится исходное
	@param compress_path - путь к сжатому файлу
	@param decompress_path - путь к распакованному файлу
	@return успех - 1, иначе - 0
*/
int create_decompress_path(
	const char compress_path[MAX_PATH_LEN],
	char decompress_path[MAX_PATH_LEN]
);

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

/*
	@brief Запуск цикла распаковки файла
	@brief Открывает файлы, вызывает распаковку файла
	@param compress_path - путь к сжатому файлу
	@param decompress_path - путь к распакованному файлу
	@param stats_decompress - указатель на структуру для сбора данных
	@return успех - 1, иначе - 0
*/
int run_decompress(
	const char compress_path[MAX_PATH_LEN],
	const char decompress_path[MAX_PATH_LEN],
	AlgorithmEfficiency_s* stats_compress
);

#endif
