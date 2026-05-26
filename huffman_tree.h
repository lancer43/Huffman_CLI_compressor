#ifndef HUF_TREE
#define HUF_TREE

#include <stddef.h>

#define ASCII_ALP_SIZE 256


typedef struct Node {
	struct Node* right; // указатель на правый лепесток
	struct Node* left; // указатель на левый лепесток
	size_t freq; // количество в тексте
	unsigned char sym; // имя узла
} Node;

/*
	@brief Подсчёт используемых символов из таблицы ASCII
	@param arr - указатель на массив с распределением частот вхождений каждого символа
	@return при успешном выполнении возвращает количество используемых символов, иначе - 0
*/
size_t counting_used_syms(const size_t arr[ASCII_ALP_SIZE]);

/*
	@brief Построение дерева Хаффмана для заданного распределения символов по частотам
	@param arr - указатель на массив с распределением частот вхождений каждого символа
	@param nodes_array - указатель на выделенную память под дерево Хаффмана
	@return возвращает указатель на корень дерева
*/
Node* build_huffman_tree(const size_t arr[ASCII_ALP_SIZE], Node** nodes_array);

/*
	@brief Очистка памяти всего дерева Хаффмана
	@param nodes_array - указатель на выделенную память под все узлы
*/
void free_huffman_tree(Node* nodes_array);

#endif
