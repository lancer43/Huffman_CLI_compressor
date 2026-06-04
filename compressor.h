#ifndef HUF_COMPRESSOR
#define HUF_COMPRESSOR

#include "huffman_tree.h"
#include "analytics.h"

#include <stdio.h>


#define MAX_CODE_LEN 256

typedef struct {
	unsigned long long bin_code;
	int length;
} HuffmanCode;

typedef struct {
	HuffmanCode symbols[ASCII_ALP_SIZE];
} CodeTable;

typedef enum {
	PATH_VALID_NO_EXTENSION,
	PATH_VALID_WITH_EXTENSION
} PathStatus_e;

/*
	@brief Подсчёт частоты вхождения каждого символа из таблицы ASCII
	@param stream - указатель на файловый поток для чтения
	@param arr - указатель на массив (строго 256 элементов)
	@return при успешном выполнении возвращает 1, иначе - 0
*/
int frequency_counting(FILE* stream, size_t arr[ASCII_ALP_SIZE]);


/*
	@brief Оболочка под вспомогательную рекурсивную generate_codes()
	@param arr - указатель на массив частот
	@param table - указатель на таблицу под коды символов
	@return успех - 1, иначе - 0
*/
int coding_symbols(size_t arr[ASCII_ALP_SIZE], CodeTable* table);

/*
	@brief Получить расширение файла
	@param source_path - путь к исходнику
	@param extension - строка под расширение
	@return успех - 1, иначе - 0
*/
int get_extension(const char source_path[MAX_PATH_LEN], char extension[MAX_EXT_LENGTH]);

/*
	@brief Запуск цикла сжатия файла
	@brief Открывает файлы, узнает размер исходного файла, его расширение, считает частоты,
	заполняет кодовую таблицу, вызывает сжатие файла
	@param source_path - путь к исходному файлу
	@param compress_path - путь к сжатому файлу
	@param stats_compress - указатель на структуру для сбора данных
	@param extension_flag - флаг наличия/отсутствия расширения исходного файла
	@return успех - 1, иначе - 0
*/
int run_compress(
	const char source_path[MAX_PATH_LEN],
	const char compress_path[MAX_PATH_LEN],
	AlgorithmEfficiency_s* stats_compress,
	PathStatus_e extension_flag
);

/*
	@brief Генерация пути к сжатому файлу
	@brief Копируется путь исходного файла, а затем вместо старого расширения ставится новое (.huf)
	@brief Если расширения не было - оно добавляется в конец пути
	@param source_path - путь к исходному файлу
	@param compress_path - путь к сжатому файлу
	@param ext_flag - флаг наличия/отсутствия расширения
	@return успех - 1, иначе - 0
*/
int create_compress_path(const char source_path[MAX_PATH_LEN], char compress_path[MAX_PATH_LEN], PathStatus_e ext_flag);

#endif