#include "huffman_tree.h"
#include "compressor.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

enum {
	bit_buf_size = 8
};

/*
	@brief Запись служебной информации для корректной распаковки файла
	@param ostream - поток для записи (сжатый файл)
	@param extension - строка расширения файла
	@param arr - массив частот
	@param file_size - размер файла в байтах
	@return 0 - успешная запись, 1 - неудачная запись расширения, 2 - неудачная запись длины файла, 3 - неудачная запись таблицы частот,
	-1 - ошибка выставления каретки
*/
static int write_overhead(FILE* ostream, char extension[MAX_EXT_LENGTH], size_t* file_size, const size_t arr[ASCII_ALP_SIZE]) {
	assert(ostream != NULL && extension != NULL && arr != NULL && file_size != NULL);
	if (fseek(ostream, 0, SEEK_SET)) return -1;

	// записываем расширение
	if (fwrite(extension, sizeof(*extension), MAX_EXT_LENGTH, ostream) != MAX_EXT_LENGTH) {
		return 1;
	}

	// считаем количество байт файла
	for (size_t i = 0; i < ASCII_ALP_SIZE; i++) {
		*file_size += arr[i];
	}
	if (fwrite(file_size, sizeof(*file_size), 1, ostream) != 1)
		return 2;
	
	// записываем таблицу частот (итого оверхед чуть больше 2кб)
	if (fwrite(arr, sizeof(*arr), ASCII_ALP_SIZE, ostream) != ASCII_ALP_SIZE)
		return 3;

	return 0;
}

int frequency_counting(FILE* stream, size_t arr[ASCII_ALP_SIZE]) {
	if (!stream || !arr) return 0;
	
	memset(arr, 0, ASCII_ALP_SIZE * sizeof(*arr));
	
	int ch;
	while ((ch = fgetc(stream)) != EOF) {
		arr[(unsigned char)ch] += 1;
	}

	return 1;
}

/*
	@brief Генерация кодов для символов по дереву Хаффмана
	@param node - указатель на узел
	@param depth - глубина дерева (она же глубина рекурсии)
	@param cur_code - бинарный код текущего узла
*/
static void generate_codes(
	Node* node,
	CodeTable* table, 
	unsigned char depth, 
	unsigned long long cur_code
) {
	if (node == NULL) return;

	// проверяем только один указатель т.к. без левого не сущ. правого и наоборот
	if (node->right != NULL) {
		generate_codes(node->right, table, depth + 1, (cur_code << 1) | 1);
		generate_codes(node->left, table, depth + 1, cur_code << 1);
		return;
	}

	table->symbols[(unsigned char)node->sym].bin_code = cur_code;
	table->symbols[(unsigned char)node->sym].length = depth;

	return;
}

int coding_symbols(size_t arr[ASCII_ALP_SIZE], CodeTable* table) {
	assert(table != NULL);

	Node* nodes = NULL;
	Node* root_tree = build_huffman_tree(arr, &nodes);
	if (root_tree == NULL) return 0;

	// крайний случай когда у нас использован 1 символ любое количество раз
	if (root_tree->right == NULL) {
		table->symbols[(unsigned char)root_tree->sym].bin_code = 0;
		table->symbols[(unsigned char)root_tree->sym].length = 1;
		free_huffman_tree(nodes);
		return 1;
	}

	generate_codes(root_tree, table, 0, 0);

	free_huffman_tree(nodes);
	
	return 1;
}

/*
	@brief Вспомогательная функция побитового заполнения файла 
	@param istream - исходный файл
	@param ostream - файл куда пишем
	@param table - таблица кодов
	@param size_file - длина istream в байтах
*/
static void fill_bits(
	FILE* istream, 
	FILE* ostream, 
	CodeTable* table, 
	size_t size_file
) {
	assert(istream != NULL && ostream != NULL && table != NULL);

	unsigned char buf = 0;
	unsigned char fr_len = bit_buf_size; // количество свободных битов в buf
	unsigned char ch = 0;
	int cur_len = 0;
	unsigned long long cur_code = 0;

	for (size_t i = 0; i < size_file; i++) {
		ch = (unsigned char)fgetc(istream);
		cur_len = table->symbols[ch].length;
		cur_code = table->symbols[ch].bin_code;

		while (fr_len < cur_len) {
			// заполняем буфер максимально насколько возможно частью кода символа
			buf = (unsigned char)(buf << fr_len) | (cur_code >> (cur_len - fr_len));

			putc(buf, ostream);
			
			// маской чистим использованные биты
			cur_code = cur_code & ((1ULL << (cur_len - fr_len)) - 1);
			cur_len -= fr_len; // обновили длину (оставшийся код)

			buf = 0;
			fr_len = bit_buf_size;
		}

		// дозаписываем оставшийся код
		buf = (unsigned char)(buf << cur_len) | cur_code;
		fr_len -= cur_len;
	}

	if (fr_len < bit_buf_size) {
		buf = buf << fr_len;
		putc(buf, ostream);
	}
}

int compress_file_v1(
	FILE* istream,
	FILE* ostream,
	const size_t arr[ASCII_ALP_SIZE],
	CodeTable* table,
	char extension[MAX_EXT_LENGTH]
) {
	if (istream == NULL || ostream == NULL || arr == NULL || table == NULL) 
		return 0;

	// выставляем каретки в начало для корректного чтения
	if (fseek(istream, 0, SEEK_SET)) return 0;
	if (fseek(ostream, 0, SEEK_SET)) return 0;

	

	// записываем оверхед
	size_t file_size = 0;

	if (write_overhead(ostream, extension, &file_size, arr) != 0) {
		return 0;
	}

	// заполняем файл сжатым кодом
	fill_bits(istream, ostream, table, file_size);
	
	return 1;
}