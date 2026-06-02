#include "autotest.h"
#include "analytics.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define MAX_TEST_FILENAME_LEN		16 // "test" + "<number 0-999>" + '\0' С ЗАПАСОМ
#define MAX_TEST_FILE_SIZE			(5 * 1024 * 1024)

#define DECOMPRESS_EXT_INDICATOR	2 // в конец расширения файла будут добавляться "_d" как _decompressed чтобы не перезаписывать исходный файл

#define NUMBER_OF_TEST_FILES		100
#define NUMBER_OF_TYPES				7 // количество типов для теста из TypeFileContent_e

#define ACCEPTABLE_EXT_CHARS		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" // без точки!!!!



typedef enum {
	CONTENT_EMPTY,		// пустой файл
	CONTENT_ONE_BYTE,	// файл размером 1 байт
	CONTENT_MONO_SYM,	// файл из одного символа случайной длины
	CONTENT_TWO_SYMS,	// файл случайной длины из двух случайных символов
	CONTENT_RANDOM,		// файл с равномерным распределением символов (длина случайна)
	CONTENT_UNEVEN,		// файл с неравномерным распределением символов
	CONTENT_ASCII		// файл с гарантией использования каждого символа ASCII хотя бы 1 раз (распределение равномерное)
} TypeFileContent_e;

// количество файлов каждого типа в каждой сотне тестовых
typedef enum {
	WEIGHT_EMPTY = 5,
	WEIGHT_ONE_BYTE = 5,
	WEIGHT_MONO_SYM = 10,
	WEIGHT_TWO_SYMS = 10,
	WEIGHT_RANDOM = 30,
	WEIGHT_UNEVEN = 30,
	WEIGHT_ASCII = 10
} TypeWeight_e;

// структура для сбора данных в .csv файл для последующей аналитики
typedef struct {
	size_t test_idx;                             // номер теста (idx)
	char extension[MAX_EXT_LENGTH];              // расширение файла (в формате ".<extension>")
	TypeFileContent_e content_type;              // тип содержания (enum)

	size_t source_size;                          // размер исходника (байт)
	size_t compressed_size;                      // размер архива (байт)

	double freq_time;                       // время заполнения массива частот (сек.)
	double bincode_time;                      // время заполнения таблицы бинарных кодов (сек.)
	double compress_time;                        // время сжатия (сек.)
	double decompress_time;                      // время распаковки (сек.)

	int is_identical;                            // результат проверки (0/1)
	size_t unique_symbols;                       // число уникальных символов в файле (0-256)
} DataAnalytics_s;

/*
	@brief Создание расширения для тестового файла
	@param extention строка для формирования расширения
*/
static void create_test_extension(char extension[MAX_EXT_LENGTH]) {
	// минимальная длина: 3 символа (например, ".x\0")
	// максимальная длина: MAX_EXT_LENGTH (32 символа, индексы 0..31)
	size_t ext_len = 3 + rand() % (MAX_EXT_LENGTH - 3 - DECOMPRESS_EXT_INDICATOR);

	extension[0] = '.'; 

	size_t count = 1; 
	while (count < ext_len - 1) { // оставляем последний байт под '\0'
		char symbol = rand() % ASCII_ALP_SIZE;
		
		while (!strchr(ACCEPTABLE_EXT_CHARS, symbol) || symbol == '\0') {
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
	
	snprintf(filename, MAX_TEST_FILENAME_LEN, "test%zu", number);
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
	@brief Случайный размер тестового файла
	@return Возвращает размер в байтах
*/
static size_t create_test_random_file_size(void) {
	size_t big_random = 0;

	#if RAND_MAX < 65535
		// если мы на Windows (MSVC), где RAND_MAX всего 32767
		big_random = ((size_t)rand() << 15) | (size_t)rand();
	#else
		// если мы на Linux/macOS, где RAND_MAX это 2147483647
		big_random = (size_t)rand();
	#endif

		return big_random % MAX_TEST_FILE_SIZE;
}

/*
	@brief Функция общего алгоритма заполнения файла (содержимое уже сформировано)
	@param stream - поток для записи
	@param file_len - длина файла в байтах
	@param buf - стековый буфер для заполнения
	@return успех - 1, иначе - 0
*/
static int fill_file(FILE* stream, const size_t file_len, const char buf[WRITE_BUFFER_SIZE]) {
	size_t remaining = file_len;

	while (remaining > 0) {
		// выбираем порцию для записи: либо целый буфер, либо остаток
		size_t portion = (remaining > WRITE_BUFFER_SIZE) ? WRITE_BUFFER_SIZE : remaining;

		if (fwrite(buf, sizeof(char), portion, stream) < portion) {

			return 0;
		}

		remaining -= portion;
	}

	return 1;
}

/*
	@brief Генерация содержания файла из 1 символа заданной длины
	@param stream - поток для записи
	@param file_len - длина файла
	@return успех - 1, иначе 0
*/
static int generate_mono_sym(FILE* stream, const size_t file_len) {
	char buf[WRITE_BUFFER_SIZE]; // стековый буфер
	char rand_sym = (char)(rand() % ASCII_ALP_SIZE); // символ для заполнения

	// заполняем одинаковыми символами
	memset(buf, rand_sym, WRITE_BUFFER_SIZE);

	if (!fill_file(stream, file_len, buf)) {
		perror("[MONO_SYM] Ошибка заполнения тестового файла");
		return 0;
	}

	return 1;
}

/*
	@brief Генерация содержания файла из 2 символов заданной длины
	@param stream - поток для записи
	@param file_len - длина файла
	@return успех - 1, иначе 0
*/
static int generate_two_syms(FILE* stream, const size_t file_len) {
	char buf[WRITE_BUFFER_SIZE]; // стековый буфер
	char rand_sym1 = rand() % ASCII_ALP_SIZE; // 1 символ для заполнения
	char rand_sym2 = rand() % ASCII_ALP_SIZE; // 2 символ для заполнения

	// --- формируем рандомную последовательность из двух символов на весь стековый буфер ---
	// мы будем копировать её пока не забъем файл нужным количеством байт
	// объективность теста сохраняется, т.к. алгоритм Хаффмана читает по 1 байту и ему 
	// плевать на закономерно повторяющуюся последовательность
	// мы точно так же "рандомно" формируем распределение символов в рамках стекового буфера
	for (size_t i = 0; i < WRITE_BUFFER_SIZE; i++) {
		
		// [TODO] чтобы не вызывать 4096 раз функцию rand() можно упаковывать рандом пачками по 32 или 64 бита через >> <<
		buf[i] = (rand() % 2) ? rand_sym1 : rand_sym2;
	}

	if (!fill_file(stream, file_len, buf)) {
		perror("[TWO_SYMS] Ошибка заполнения тестового файла");
		return 0;
	}

	return 1;
}

/*
	@brief Генерация рандомного содержания файла заданной длины 
	@param stream - поток для записи
	@param file_len - длина файла
	@return успех - 1, иначе 0
*/
static int generate_random_content(FILE* stream, const size_t file_len) {
	char buf[WRITE_BUFFER_SIZE]; // стековый буфер

	for (size_t i = 0; i < WRITE_BUFFER_SIZE; i++) {
		// [TODO] чтобы не вызывать 4096 раз функцию rand() можно упаковывать рандом пачками по 32 или 64 бита через >> <<
		buf[i] = rand() % ASCII_ALP_SIZE;
	}

	if (!fill_file(stream, file_len, buf)) {
		perror("[RANDOM] Ошибка заполнения тестового файла");
		return 0;
	}

	return 1;
}

/*
	@brief Генерация неравномерно распределённого содержания файла заданной длины
	@param stream - поток для записи
	@param file_len - длина файла
	@return успех - 1, иначе 0
*/
static int generate_uneven_content(FILE* stream, const size_t file_len) {
	char buf[WRITE_BUFFER_SIZE]; // стековый буфер

	size_t n = (rand() % 9) + 2; // шаг с которым будем вставлять "особый" символ, который чаще всего повторяется
	char special_sym = (char)(rand() % ASCII_ALP_SIZE); // особый символ

	for (size_t i = 0; i < WRITE_BUFFER_SIZE; i++) {
		if (i % n == 0) {
			buf[i] = special_sym;
		}
		else {
			// [TODO] чтобы не вызывать 4096 раз функцию rand() можно упаковывать рандом пачками по 32 или 64 бита через >> <<
			buf[i] = (char)(rand() % ASCII_ALP_SIZE);
		}
	}

	if (!fill_file(stream, file_len, buf)) {
		perror("[UNEVEN] Ошибка заполнения тестового файла");
		return 0;
	}

	return 1;
}

/*
	@brief Генерация содержания файла с гарантией всех 256 символов ASCII и равномерным распределением заданной длины
	@param stream - поток для записи
	@param file_len - длина файла
	@return успех - 1, иначе 0
*/
static int generate_ascii_content(FILE* stream, const size_t file_len) {
	char buf[ASCII_ALP_SIZE]; // стековый буфер

	// заполняем файл
	for (size_t i = 0; i < ASCII_ALP_SIZE; i++) {
		buf[i] = (char)i;
	}

	// на случай если длина файла оказалась меньше 256 байт
	if (file_len <= ASCII_ALP_SIZE) {
		if (fwrite(buf, sizeof(*buf), file_len, stream) < file_len) {
			perror("[ASCII] Ошибка заполнения тестового файла");
			return 0;
		}

		return 1;
	} 

	if (fwrite(buf, sizeof(*buf), ASCII_ALP_SIZE, stream) < ASCII_ALP_SIZE) {
		perror("[ASCII] Ошибка заполнения тестового файла");
		return 0;
	}

	if (!generate_random_content(stream, file_len - ASCII_ALP_SIZE)) return 0;

	return 1;
}

/*
	@brief Создание случайного содержания для тестового файла
	@param stream - поток для записи
	@param type - тип содержания
	@return успех - 1, иначе - 0
*/
static int create_test_random_content(FILE* stream, size_t file_len, TypeFileContent_e type) {
	switch (type) {
		case CONTENT_EMPTY: {
			return 1;
		}
		case CONTENT_ONE_BYTE: {
			// забить 1 битом
			char rand_sym = rand() % ASCII_ALP_SIZE;
			if (fwrite(&rand_sym, sizeof(rand_sym), 1, stream) < 1) return 0;

			return 1;
		}
		case CONTENT_MONO_SYM: {
			// рандом длина из 1 символа
			if (!generate_mono_sym(stream, file_len)) return 0;

			return 1;
		}
		case CONTENT_TWO_SYMS: {
			// рандом длина из двух символов
			if (!generate_two_syms(stream, file_len)) return 0;

			return 1;
		}
		case CONTENT_RANDOM: {
			// равномерное распределение
			if (!generate_random_content(stream, file_len)) return 0;

			return 1;
		}
		case CONTENT_UNEVEN: {
			// неравномерное распределение
			if (!generate_uneven_content(stream, file_len)) return 0;

			return 1;
		}
		case CONTENT_ASCII: {
			// гарантия 256 символов + равномерное распределение
			if (!generate_ascii_content(stream, file_len)) return 0;

			return 1;
		}
		default:
			return 0;
	}

	return 1; // на всякий случай
}

/*
	@brief Создание тестового файла с рандомным содержанием
	@param number - порядковый номер файла
	@param type - тип содержания файл
	@param path - путь (передается наружу для подведения статистики)
	@return успех - 1, иначе - 0
*/
static int create_test_random_file(size_t number, size_t file_len, TypeFileContent_e type, char test_path[MAX_PATH_LEN]) {
	char* test_dir = "./autotest_files/"; // папка для тестовых файлов
	char test_filename[MAX_TEST_FILENAME_LEN] = "test";
	char test_extension[MAX_EXT_LENGTH];

	create_test_filename(test_filename, number);
	create_test_extension(test_extension);
	create_test_path(test_path, test_dir, test_filename, test_extension);

	printf("\n[DEBUG] сформированное расширение: %s", test_extension);
	printf("\n[DEBUG] сформированный путь: %s", test_path);

	FILE* new_file = fopen(test_path, "wb");
	if (!new_file) {
		printf("\nОшибка: неудачное создание файла");
		return 0;
	}

	// функция генерации рандомного содержания
	create_test_random_content(new_file, file_len, type);

	fclose(new_file);
	return 1;
}

/*
	@brief Запуск автотеста. 
	@brief ПРИМЕЧАНИЕ: расширения гарантируются корректными (до 32 символов, всегда существует точка-разделитель)
*/
int run_autotest_files(void) {
	size_t weight = 0; 
	size_t idx = 0;

	TypeWeight_e weights[] = {
		WEIGHT_EMPTY,
		WEIGHT_ONE_BYTE,
		WEIGHT_MONO_SYM,
		WEIGHT_TWO_SYMS,
		WEIGHT_RANDOM,
		WEIGHT_UNEVEN,
		WEIGHT_ASCII,
	};

	for (TypeFileContent_e type = 0; type < NUMBER_OF_TYPES; type++) {
		weight += (size_t)weights[type];
		
		while (idx < weight) {
			size_t file_len = 0;

			if (type == CONTENT_EMPTY) {
				file_len = 0;
			}
			else if (type == CONTENT_ONE_BYTE) {
				file_len = 1;
			}
			else {
				file_len = create_test_random_file_size();
			}

			char path[MAX_PATH_LEN] = { 0 };

			if (!create_test_random_file(idx, file_len, type, path)) return 0;



			/*
				сжать
				распаковать
				сравнить
				занести данные в .csv
				удалить
			*/

			idx++;
		}
	}

	return 1;
}