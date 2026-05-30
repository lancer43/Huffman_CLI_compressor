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

/*
	Что здесь надо переделать? [ПЕРЕДЕЛАНО]

	В этой функции мы читаем каждый символ из файла чтобы подсчитать количество вхождений каждого символа в файле
	Сейчас вызов fgetc() для чтения 1 символа может заставлять программу производить системный вызов ради каждого байта - это долго

	- создать буфер на стеке 4кб (т.к. обычно системный буфер имеет размер 4кб)
	- заполнить стековый буфер до полного одним вызовом fread() (эквивалентно 1 системному вызову)
	- читать и раскладывать символы до тех пор, пока не опустошится стековый буфер
	- заполнить буфер снова, и повторять цикл до тех пор, пока не дойдем до конца файла

*/

int frequency_counting(FILE* stream, size_t arr[ASCII_ALP_SIZE]) {
	if (!stream || !arr) return 0;

	// устанавливаем каретку в начальное положение
	if (fseek(stream, 0, SEEK_SET)) return 0;
	
	// очищаем массив 
	memset(arr, 0, ASCII_ALP_SIZE * sizeof(*arr));
	
	// создаем стековый буфер
	unsigned char buf[READ_BUFFER_SIZE] = { 0 };
	size_t count = 0; // переменная для записи числа прочитанных элементов

	// читаем с диска большими порциями и побайтово читаем буфер для экономии тактов
	while ((count = fread(buf, sizeof(*buf), READ_BUFFER_SIZE, stream)) > 0) {
		for (size_t i = 0; i < count; i++) {
			arr[buf[i]] += 1;
		}
	}

	if (ferror(stream))
		return 0;

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
	const size_t size_file
) {
	assert(istream != NULL && ostream != NULL && table != NULL);

	unsigned char buf = 0; // буфер для накопления битов
	unsigned char fr_len = bit_buf_size; // количество свободных битов в buf
	unsigned char ch = 0; // текущий символ из файла
	int cur_len = 0; // длина бинарного кода текущего символа (в битах)
	unsigned long long cur_code = 0; // бинарный код текущего символа

	// стековые буферы для экономии тактов (меньше системных вызовов)
	unsigned char read_buf[READ_BUFFER_SIZE] = { 0 };
	unsigned char write_buf[WRITE_BUFFER_SIZE] = { 0 };

	// указатели для записи в стековые буферы
	unsigned char* read_ptr = read_buf;
	unsigned char* write_ptr = write_buf;

	size_t read_count = 0;
	// первоначально надо заполнить буфер
	// fread(read_buf, sizeof(*read_buf), READ_BUFFER_SIZE, istream);
	
	for (size_t i = 0; i < size_file; i++) {

		// если указатель ушел за пределы - возвращаем на место и читаем новую порцию
		if (read_ptr >= &read_buf[read_count]) {
			read_ptr = read_buf;
			read_count = fread(read_buf, sizeof(*read_buf), READ_BUFFER_SIZE, istream);

			if (read_count == 0) break; // данные закончились
		}

		// читаем символ из стекового буфера
		ch = *read_ptr++;
		cur_len = table->symbols[ch].length; // длина этого символа в таблице кодов (в битах)
		cur_code = table->symbols[ch].bin_code; // код этого символа в таблице кодов

		// пока в буфере (его размер 1 байт) не хватает бит для записи текущего кода
		while (fr_len < cur_len) {

			// заполняем буфер максимально насколько возможно частью кода текущего символа
			buf = (unsigned char)(buf << fr_len) | (cur_code >> (cur_len - fr_len));

			// сбрасываем накопленный байт в выходной буфер
			*write_ptr++ = buf;

			// если накопился полный буфер - отправляем на диск
			if (write_ptr == &write_buf[WRITE_BUFFER_SIZE]) {
				write_ptr = write_buf;
				fwrite(write_buf, sizeof(*write_buf), WRITE_BUFFER_SIZE, ostream);
			}
			
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

	// если в байт-буфере что то осталось - дозаполняем нулями и сбрасываем в стековый буфер 
	if (fr_len < bit_buf_size) {
		buf = buf << fr_len;
		*write_ptr++ = buf;
	}

	// сброс остатков на диск
	fwrite(write_buf, sizeof(*write_buf), write_ptr - write_buf, ostream);
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