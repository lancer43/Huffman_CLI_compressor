#include "autotest.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define MAX_TEST_FILENAME_LEN	8 // "test" + "<number 0-999>" + '\0'
#define ACCEPTABLE_EXT_CHARS	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" // без точки!!!!

#define NUMBER_OF_TEST_FILES	100

typedef enum {
	CONTENT_EMPTY, // пустой файл
	CONTENT_ONE_BYTE, // файл размером 1 байт
	CONTENT_MONO_SYM, // файл из одного символа случайной длины
	CONTENT_TWO_SYMS, // файл случайной длины из двух случайных символов
	CONTENT_RANDOM, // файл с равномерным распределением символов (длина случайна)
	CONTENT_UNEVEN, // файл с неравномерным распределением символов
	CONTENT_ASCII // файл с гарантией использования каждого символа ASCII хотя бы 1 раз (распределение равномерное)
} FileContent_e;

/*
	========== ФУНКЦИОНАЛ МОДУЛЯ ==========

	- генерация файлов случайной длины и случайного содержания (случайный формат случайной длины)
	- сравнение сжатых и распакованных файлов на содержание и формат
	- замер времени сжатия и распаковки каждого файла и сведение этих данных в .csv файл (+ можно записывать формат файла, массив частот, таблицу кодов, длину файла)

	========== ГЕНЕРАЦИЯ ФАЙЛОВ ==========
	-

*/

/*
	@brief Создание расширения для тестового файла
	@param extention строка для формирования расширения
*/
static void create_test_extension(char extension[MAX_EXT_LENGTH]) {
	// минимальная длина: 3 символа (например, ".x\0")
	// максимальная длина: MAX_EXT_LENGTH (32 символа, индексы 0..31)
	size_t ext_len = 3 + rand() % (MAX_EXT_LENGTH - 3);

	extension[0] = '.'; 

	size_t count = 1; 
	while (count < ext_len - 1) { // оставляем последний байт под '\0'
		char symbol = rand() % ASCII_ALP_SIZE;
		while (!strchr(ACCEPTABLE_EXT_CHARS, symbol)) {
			symbol = rand() % ASCII_ALP_SIZE;
		}
		extension[count++] = symbol;
	}

	extension[count] = '\0';
}

/*
	@brief Создание имени тестового файла "test" + "<number 0-999>"
	@param filename - строка для формирования имени файла
	@param number - порядковый номер файла
*/
static void create_test_filename(char filename[MAX_TEST_FILENAME_LEN], const size_t number) {
	assert(number < 1000); // я планирую подавать на тест 100 файлов
	
	char string_number[4]; // 0-999
	
	snprintf(string_number, sizeof(string_number), "%zu", number);
	snprintf(filename, MAX_TEST_FILENAME_LEN, "%s%s", filename, string_number);
}

/*
	@brief Создание относительного пути для тестового файла
	@param path - строка для формирования пути
	@param directory - относительный путь к папке
	@param filename - имя файла
	@param extension - расширение файла
*/
static void create_test_path(
	char path[MAX_PATH_LEN], 
	const char* directory, 
	const char filename[MAX_TEST_FILENAME_LEN], 
	const char extension[MAX_EXT_LENGTH]
) {
	snprintf(path, MAX_PATH_LEN, "%s%s%s", directory, filename, extension); // гарантированно помещается
}

/*
	@brief Создание случайного содержания для тестового файла
	@param stream - поток для записи
*/
void create_test_random_content(FILE* stream) {
	
}

/*
	@brief Создание тестового файла с рандомным содержанием
	@param number - порядковый номер файла
*/
static int create_test_random_file(size_t number) {
	char* test_dir = "./autotest_files/"; // папка для тестовых файлов
	char test_filename[MAX_TEST_FILENAME_LEN] = "test";
	char test_extension[MAX_EXT_LENGTH];
	
	char test_path[MAX_PATH_LEN];

	create_test_filename(test_filename, number);
	create_test_extension(test_extension);
	create_test_path(test_path, test_dir, test_filename, test_extension);

	printf("\n[DEBUG] сформированное расширение: %s", test_extension);
	printf("\n[DEBUG] сформированный путь: %s", test_path);

	FILE* new_file = fopen(test_path, "w");
	if (!new_file) {
		printf("\nОшибка: неудачное создание файла");
		return 0;
	}

	// функция генерации рандомного содержания

	fclose(new_file);
	return 1;
}

int run_autotest_files(void) {
	for (size_t i = 0; i < NUMBER_OF_TEST_FILES; i++) {
		if (!create_test_random_file(i)) {
			printf("\nОшибка: неудачное создание файла");
		}
	}
}