#include "huffman_tree.h"
#include <stdlib.h>

typedef struct {
	Node** buffer; // указатель на массив указателей на узлы дерева (Node - узел дерева)
	size_t head; // id головы очереди в массиве queue (будет меняться функцией queue_pop())
	size_t tail; // id хвоста очереди в массиве queue (будет меняться функцией queue_push())
	size_t capacity; // максимальная вместимость очереди (защита от переполнения)
} Queue;

/*
	@brief Функция добавления элемента в хвост очереди QueueNode
	@param queue - указатель на очередь
	@param node - указатель на ноду, которую хотим добавить
	@return 1 - успех, 0 - ошибка
*/
static int queue_push(Queue* queue, Node* node) {
	if (queue->tail >= queue->capacity) return 0;

	queue->buffer[queue->tail] = node;
	queue->tail++;

	return 1;
}

/*
	@brief Функция удаления элемента из головы очереди
	@param queue - указатель на очередь, откуда удаляем элемент
	@return возвращает указатель на удалённый узел дерева
*/
static Node* queue_pop(Queue* queue) {
	if (queue->head == queue->tail) return NULL;

	Node* removed = queue->buffer[queue->head];
	queue->head++;

	return removed;
}

static size_t queue_size(Queue* queue) {
	return queue->tail - queue->head;
}

static int compare_nodes(const void* a, const void* b) {
	const Node* nodeA = *(const Node**)a;
	const Node* nodeB = *(const Node**)b;

	if (nodeA->freq > nodeB->freq) return 1;
	else if (nodeA->freq < nodeB->freq) return -1;
	return 0;
}

static Node* extract_min(Queue* q1, Queue* q2) {
	// элементарные случаи когда очередь пустая
	int q1_empty = (q1->head >= q1->tail);
	int q2_empty = (q2->head >= q2->tail);

	if (q1_empty && q2_empty) return NULL;

	if (q1_empty) return queue_pop(q2);
	if (q2_empty) return queue_pop(q1);

	if (q1->buffer[q1->head]->freq < q2->buffer[q2->head]->freq) {
		return queue_pop(q1);
	} else {
		return queue_pop(q2);
	}
}

size_t counting_used_syms(const size_t arr[ASCII_ALP_SIZE]) {
	if (!arr) return 0;

	size_t counter = 0;
	for (size_t i = 0; i < ASCII_ALP_SIZE; i++) {
		if (arr[i] != 0) counter++;
	}

	return counter;
}

Node* build_huffman_tree(const size_t arr[ASCII_ALP_SIZE], Node** nodes_array) {
	// считаем сколько символов из таблицы используется
	size_t count = counting_used_syms(arr); 
	if (count == 0) return NULL;
	
	// выделяем память под максимально возможное количество узлов
	Node* nodes = (Node*)calloc(2 * count - 1, sizeof(Node));
	*nodes_array = nodes;
	if (nodes == NULL) return NULL;

	// делаем два буфера под очереди и заполняем узлами (узлы формируем параллельно)
	Node* buf_q1[ASCII_ALP_SIZE] = { 0 };
	Node* buf_q2[ASCII_ALP_SIZE] = { 0 };

	Queue queue1;
	Queue queue2;

	queue1.buffer = buf_q1;
	queue1.capacity = ASCII_ALP_SIZE;
	queue1.head = 0;
	queue1.tail = 0;

	queue2.buffer = buf_q2;
	queue2.capacity = ASCII_ALP_SIZE;
	queue2.head = 0;
	queue2.tail = 0;

	size_t node_i = 0;
	for (size_t i = 0; i < ASCII_ALP_SIZE; i++) {
		if (arr[i] > 0) {
			nodes[node_i].freq = arr[i]; // частота
			nodes[node_i].sym = (unsigned char)i; // символ

			queue_push(&queue1, &nodes[node_i]);
			node_i++;
		}
	}

	// алгоритм Хаффмана на 2 очередях
	qsort(queue1.buffer, queue1.tail, sizeof(Node*), compare_nodes);

	while (queue_size(&queue1) + queue_size(&queue2) > 1) {
		Node* left = extract_min(&queue1, &queue2);
		Node* right = extract_min(&queue1, &queue2);

		Node* parent = &nodes[node_i++];
		parent->freq = left->freq + right->freq;
		parent->left = left;
		parent->right = right;

		queue_push(&queue2, parent);
	}
	// через минимум для гарантии возврата
	return extract_min(&queue1, &queue2);
}

void free_huffman_tree(Node* nodes_array) {
	free(nodes_array);
}