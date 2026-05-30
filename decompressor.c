#include "huffman_tree.h"
#include "decompressor.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>



enum {
	bits_in_byte = 8,
	bits_capacity_buf = 16
};

int read_overhead(FILE* istream, char extension[MAX_EXT_LENGTH], size_t* file_size, size_t freq_count[ASCII_ALP_SIZE]) {
	assert(istream != NULL);

	if (fseek(istream, 0, SEEK_SET)) 
		return -1;

	if (extension == NULL) {
		if (fseek(istream, MAX_EXT_LENGTH, SEEK_CUR)) 
			return -1;
	} else {
		if (fread(extension, sizeof(*extension), MAX_EXT_LENGTH, istream) != MAX_EXT_LENGTH) {
			return 1;
		}
	}

	if (file_size == NULL) {
		if (fseek(istream, sizeof(size_t), SEEK_CUR)) 
			return -1;
	} else {
		if (fread(file_size, sizeof(*file_size), 1, istream) != 1) {
			return 2;
		}
	}

	if (freq_count == NULL) {
		if (fseek(istream, ASCII_ALP_SIZE * sizeof(size_t), SEEK_CUR)) 
			return -1;
	} else {
		if (fread(freq_count, sizeof(*freq_count), ASCII_ALP_SIZE, istream) != ASCII_ALP_SIZE) {
			return 3;
		}
	}

	return 0;
}


// [SEARCH] заменить fputc
/*
	Что надо переделать?
	Сейчас здесь происходит вызов fgetc() и fputc() на каждый отдельный байт для чтения файла
	Каждый вызов функции потенциально влечет за собой системный вызов, который отбирает много тактов процессора
	Ради каждого байта этот подход является крайне неэффективным, поэтому планируется переписать алгоритм следующим образом

	Чтение
	- Создается буфер на стеке размером 4кб (т.к. по умолчанию системный буфер 4кб)
	- за 1 системный вызов читается 4кб в стековый буфер через fread(), а затем побайтово информация забирается оттуда
	- при опустошении стекового буфера чтение повторяется, цикл происходит до тех пор, пока файл не закончится

	Запись
	- Создается буфер на стеке размером 4кб (т.к. по умолчанию системный буфер 4кб)
	- буфер заполняется расшифрованными байтами файла до упора
	- за 1 системный вызов через fwrite() происходит запись буфера в файл
	- после записи буфер очищается и чтение происходит заново до тех пор, пока файл не закончится
*/

/*
	@brief Алгоритм расшифровки бинарного файла через обход дерева
	@param istream - входной поток
	@param ostream - выходной поток
	@param root - корень дерева
	@param file_size - размер распакованного файла в байтах
*/
static void huffman_decompress(
	FILE* istream, 
	FILE* ostream, 
	Node* root,
	size_t file_size
) {
	assert(istream != NULL && ostream != NULL && root != NULL);

	// если был пустой файл до сжатия
	if (file_size == 0) return;

	unsigned short buf = 0; // двухбайтовый буфер для накопления битов
	unsigned char bits_in_buf = 0;

	// стековые буферы для экономии тактов процессора (меньше системных вызовов)
	unsigned char read_buf[READ_BUFFER_SIZE] = { 0 };
	unsigned char write_buf[WRITE_BUFFER_SIZE] = { 0 };

	// указатели для работы со стековыми буферами
	unsigned char* read_ptr = read_buf;
	unsigned char* write_ptr = write_buf;

	size_t read_count = 0; // счетчик прочитанных в read_buf элементов

	Node* cur_node = root; // текущий узел

	read_count = fread(read_buf, sizeof(*read_buf), READ_BUFFER_SIZE, istream);
	
	// читаем двухбайтовый буфер до полного
	for (size_t i = 0; i < sizeof(buf) && i < read_count; i++) {
		int byte = *read_ptr++;

		buf = (buf << bits_in_byte) | byte;
		bits_in_buf += bits_in_byte;
	}

	// известно, что в файле file_size байт, следовательно проходим нужное количество раз
	for (size_t i = 0; i < file_size; i++) {
		cur_node = root;

		// проверяем только правый, т.к. без него не сущ. левый и наоборот
		while (cur_node->right != NULL) {

			// если из нашего двухбайтового буфера прочитано более байта - дозаполняем
			if (bits_in_buf < bits_in_byte) {

				// если байты в стековом буфере закончились - возвращаем указатель в начало и читаем новую порцию с диска
				if (read_ptr >= &read_buf[read_count]) {
					read_ptr = read_buf;

					read_count = fread(read_buf, sizeof(*read_buf), READ_BUFFER_SIZE, istream);
				}

				// если хоть что то прочиталось - добавляем в двухбайтовый буфер
				if (read_count > 0) {
					int byte = *read_ptr++;

					buf = (buf << bits_in_byte) | byte;
					bits_in_buf += bits_in_byte;
				}
			}

			// прочитали самый левый ВАЛИДНЫЙ бит
			unsigned char bit = (buf >> (bits_in_buf - 1))&1;
			
			unsigned char shift = bits_capacity_buf - bits_in_buf; // вспомогательная переменная для очистки головы

			// чистим тот левый ВАЛИДНЫЙ бит (нынешнюю голову)
			buf = (buf << shift) >> shift;
			// уменьшаем длину ВАЛИДНЫХ битов (слева голова - след. бит, левее нее НУЛИ)
			bits_in_buf--;

			// спуск по дереву (1 - в правый лист, 0 - в левый)
			if (bit == 1) {
				cur_node = cur_node->right;
			}
			else {
				cur_node = cur_node->left;
			}
		}

		// записываем символ в стековый буфер 
		*write_ptr++ = cur_node->sym;

		// если стековый буфер заполнился - порцией выгружаем его на диск
		if (write_ptr >= &write_buf[WRITE_BUFFER_SIZE]) {
			write_ptr = write_buf;
			fwrite(write_buf, sizeof(*write_buf), WRITE_BUFFER_SIZE, ostream);
		}
	}

	// сброс остатков на диск
	fwrite(write_buf, sizeof(*write_buf), write_ptr - write_buf, ostream);
}

int decompress_file_v1(
	FILE* istream,
	FILE* ostream
) {
	assert(istream != NULL && ostream != NULL);

	// выставляем каретки в начало для корректного чтения
	if (fseek(istream, 0, SEEK_SET)) return 0;
	if (fseek(ostream, 0, SEEK_SET)) return 0;

	// читаем оверхед
	size_t file_size = 0;
	size_t freq_count[ASCII_ALP_SIZE] = { 0 };

	if (read_overhead(istream, NULL, &file_size, freq_count) != 0) {
		return 0;
	}

	// выделяем память под узлы дерева Хаффмана
	Node* nodes = NULL;
	Node* root_tree = build_huffman_tree(freq_count, &nodes);
	if (root_tree == NULL) return 0;

	huffman_decompress(istream, ostream, root_tree, file_size);

	free_huffman_tree(nodes);

	return 1;
}