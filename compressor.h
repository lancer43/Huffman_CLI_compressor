#ifndef CLI_COMPRESSOR
#define CLI_COMPRESSOR

#include "huffman_tree.h"

#include <stdio.h>


#define MAX_CODE_LEN 256

typedef struct {
	unsigned long long bin_code;
	int length;
} HuffmanCode;

typedef struct {
	HuffmanCode symbols[ASCII_ALP_SIZE];
} CodeTable;

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
	@brief Сжатие файла. Версия с оверхедом из массива частот
	@param istream - указатель на исходный файл (необходим режим "rb")
	@param ostream - указатель на файл для записи (необходим режим "wb")
	@param arr - указатель на массив частот
	@param table - указатель на таблицу для кодирования
	@param extension - строка с расширением (для оверхеда)
	@return успех - 1, иначе - 0
*/
int compress_file_v1(
	FILE* istream,
	FILE* ostream,
	const size_t arr[ASCII_ALP_SIZE],
	CodeTable* table,
	char extension[MAX_EXT_LENGTH]
);

#endif