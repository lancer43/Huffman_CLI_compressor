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

	unsigned short buf = 0;
	unsigned char bits_in_buf = 0;

	Node* cur = root;

	// читаем буфер до полного
	for (size_t i = 0; i < sizeof(buf); i++) {
		int byte = fgetc(istream);
		if (byte == EOF) break; // на всякий, но вроде можно убрать

		buf = (buf << bits_in_byte) | byte;
		bits_in_buf += bits_in_byte;
	}

	for (size_t i = 0; i < file_size; i++) {
		cur = root;

		// проверяем только правый, т.к. без него не сущ. левый и наоборот
		while (cur->right != NULL) {
			// если из нашего буфера прочитано более байта дозаполняем
			if (bits_in_buf < bits_in_byte) {
				int byte = fgetc(istream);
				if (byte != EOF) { // читаем, только если там реальный байт
					buf = (buf << bits_in_byte) | byte;
					bits_in_buf += bits_in_byte;
				}
				// если EOF — мы просто ничего не делаем и спокойно 
				// разбираем те биты, что ОСТАЛИСЬ в буфере от прошлого чтения
			}

			// прочитали самый левый ВАЛИДНЫЙ бит
			unsigned char bit = (buf >> (bits_in_buf - 1))&1;
			// вспомогательная переменная для очистки головы
			unsigned char shift = bits_capacity_buf - bits_in_buf;
			// чистим тот левый ВАЛИДНЫЙ бит (нынешнюю голову)
			buf = (buf << shift) >> shift;
			// уменьшаем длину ВАЛИДНЫХ битов (слева голова - след. бит, левее нее НУЛИ)
			bits_in_buf--;

			if (bit == 1) {
				cur = cur->right;
			}
			else {
				cur = cur->left;
			}
		}
		fputc(cur->sym, ostream);
	}
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