#include "huffman_tree.h"
#include "decompressor.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


enum {
	bits_in_byte = 8,
	bits_capacity_buf = 16
};


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

	// запоминаем размер файла байтах (сколько должно быть символов после распаковки)
	size_t file_size = 0;
	fread(&file_size, sizeof(file_size), 1, istream);


	// восстанавливаем массив частот
	size_t freq_count[ASCII_ALP_SIZE] = { 0 };
	fread(freq_count, sizeof(*freq_count), ASCII_ALP_SIZE, istream);

	size_t count = counting_used_syms(freq_count);
	if (!count) return 0;

	// выделяем память под узлы дерева Хаффмана
	Node* nodes = NULL;
	Node* root_tree = build_huffman_tree(freq_count, &nodes);
	if (root_tree == NULL) return 0;

	huffman_decompress(istream, ostream, root_tree, file_size);

	free_huffman_tree(nodes);

	return 1;
}