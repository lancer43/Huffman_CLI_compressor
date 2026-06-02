#include "autotest.h"
#include "analytics.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#define MAX_TEST_FILENAME_LEN		16 // "test" + "<number 0-999>" + '\0' С ЗАПАСОМ
#define MAX_TEST_FILE_SIZE			(50 * 1024 * 1024)

#define DECOMPRESS_EXT_INDICATOR	2 // в конец расширения файла будут добавляться "_d" как _decompressed чтобы не перезаписывать исходный файл

#define NUMBER_OF_TEST_FILES		100
#define NUMBER_OF_TYPES				7 // количество типов для теста из TypeFileContent_e

#define ACCEPTABLE_EXT_CHARS		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" // без точки!!!!
#define AUTOTEST_DIRECTORY			"./autotest_files/"
#define AUTOTEST_DATA_FILENAME		"autotest_data.csv"

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
	@brief Создание пути к тестовому сжатому файлу
	@param source_path - путь к исходному файлу
	@param compress_path - путь к сжатому файлу
	@return успех - 1, иначе - 0
*/
static int create_test_compress_path(const char source_path[MAX_PATH_LEN], char compress_path[MAX_PATH_LEN]) {
	char temp_path[MAX_PATH_LEN] = { 0 };

	int written = snprintf(temp_path, MAX_PATH_LEN, "%s", source_path);
	if (written < 0 || written > MAX_PATH_LEN) return 0;

	// printf("\n[DEBUG] COPYING temp_path: '%s'", temp_path);

	char* point_ptr = strrchr(temp_path, '.');
	assert(point_ptr != NULL);
	*point_ptr = '\0';

	// printf("\n[DEBUG] POINT temp_path: '%s'", temp_path);

	// константами длина вымеряна, вроде проверка не нужна, но место узкое
	written = snprintf(compress_path, MAX_PATH_LEN, "%s%s", temp_path, COMPRESSED_EXTENSION);
	if (written < 0 || written > MAX_PATH_LEN) return 0;

	return 1;
}

static void create_test_decompress_path(const char source_path[MAX_PATH_LEN], char decompress_path[MAX_PATH_LEN]) {
	snprintf(decompress_path, MAX_PATH_LEN, "%s%s", source_path, "_d"); // [TODO] можно сделать проверку
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
	@param path - путь (передается наружу для работы автотеста)
	@param extension - расширение файла (передается наружу для подведения статистики) 
	@return успех - 1, иначе - 0
*/
static int create_test_random_file(
	size_t number, 
	size_t file_len, 
	TypeFileContent_e type, 
	char test_path[MAX_PATH_LEN], 
	char extension[MAX_EXT_LENGTH]
) {
	char* test_dir = AUTOTEST_DIRECTORY; // папка для тестовых файлов
	char test_filename[MAX_TEST_FILENAME_LEN] = "test";

	create_test_filename(test_filename, number);
	create_test_extension(extension);
	create_test_path(test_path, test_dir, test_filename, extension);

	// printf("\n[DEBUG] сформированное расширение: %s", test_extension);
	// printf("\n[DEBUG] сформированный путь: %s", test_path);

	FILE* new_file = fopen(test_path, "wb");
	if (!new_file) {
		return 0;
	}

	// функция генерации рандомного содержания
	if (!create_test_random_content(new_file, file_len, type)) return 0;

	fclose(new_file);
	return 1;
}

/*
	@brief Сжатие тестового файла с записью аналитики в промежуточную структуру
	@param source_path - путь к исходному файлу
	@param compress_path - путь к сжатому файлу
	@param stats_compress - структура с данными сжатия
	@param used_syms - количество использованных симолов (передается наружу для подведения статистики)
	@return успех - 1, иначе - 0
*/
static int compress_test_file(
	const char source_path[MAX_PATH_LEN], 
	const char compress_path[MAX_PATH_LEN], 
	AlgorithmEfficiency_s* stats_compress,
	size_t* used_syms
) {
	FILE* source = fopen(source_path, "rb");
	FILE* compressed_file = fopen(compress_path, "wb");

	int flag = 1;

	// структура для записи данных о количестве тактов на каждом этапе сжатия
	Clocks_s clocks = { 0 };

	if (source == NULL) {
		printf("\n\nОшибка открытия исходного файла (для чтения)");
		
		flag = 0;
		goto cleanup;
	}
	if (compressed_file == NULL) {
		printf("\n\nОшибка открытия сжатого файла (для записи)");

		flag = 0;
		goto cleanup;
	}

	if (fseek(source, 0, SEEK_END)) return 0;
	size_t file_size = ftell(source);

	size_t freq_count[ASCII_ALP_SIZE] = { 0 };

	clocks.start_total = clock();
	int success = frequency_counting(source, freq_count);
	clocks.stop_freq = clock();

	if (!success) {
		printf("\nПодсчет частоты неудачный");

		flag = 0;
		goto cleanup;
	}

	*used_syms = counting_used_syms(freq_count);

	// [DEBUG] Частоты
	/*
	for (size_t i = 0; i < ASCII_ALP_SIZE; i++) {
		printf("\n[DEBUG] char number %zu: %zu count", i, freq_count[i]);
	}
	*/

	CodeTable table = { 0 };

	
	// printf("\n[DEBUG] file_size = %zu", file_size);

	clocks.start_bincode = clock();
	if (file_size != 0) {
		success = coding_symbols(freq_count, &table);
	}
	clocks.stop_bincode = clock();

	if (!success) {
		printf("\nОшибка заполнения таблицы кодов");

		flag = 0;
		goto cleanup;
	}

	// записываем расширение исходного файла для оверхеда
	char extension[MAX_EXT_LENGTH] = { 0 };
	char* ptr = strrchr(source_path, '.');
	assert(ptr != NULL);
	int written = snprintf(extension, strlen(ptr) + 1, "%s", ptr);

	if (written < 0 || written > strlen(ptr) + 1) {
		
		flag = 0;
		goto cleanup;
	}


	clocks.start_compress = clock();
	success = compress_file_v1(source, compressed_file, freq_count, &table, extension, &file_size);
	clocks.stop_total = clock();

	if (!success) {
		printf("\nОшибка сжатия файла");

		flag = 0;
		goto cleanup;
	}

	if (fflush(compressed_file) == EOF) {
		perror("Ошибка сброса данных на диск");

		flag = 0;
		goto cleanup;
	}

	if (!result_analysis(source, compressed_file, &clocks, RUN_COMPRESS, stats_compress)) {
		printf("\nОшибка вывода аналитики");
		
		flag = 0;
	}


cleanup:

	if (source)				fclose(source);
	if (compressed_file)	fclose(compressed_file);

	return flag;
}

/*
	@brief Распаковка тестового файла с записью аналитики в промежуточную структуру
	@param compress_path - путь к сжатому файлу
	@param decompress_path - путь к распакованному файлу
	@param stats_decompress - структура с данными распаковки
	@return успех - 1, иначе - 0
*/
static int decompress_test_file(
	const char compress_path[MAX_PATH_LEN], 
	const char decompress_path[MAX_PATH_LEN], 
	AlgorithmEfficiency_s* stats_decompress
) {
	FILE* compressed_file = fopen(compress_path, "rb");
	FILE* decompressed_file = fopen(decompress_path, "wb");

	int flag = 1;

	Clocks_s clocks = { 0 };

	if (compressed_file == NULL) {
		printf("\n\nОшибка открытия сжатого файла (для чтения)");

		flag = 0;
		goto cleanup;
	}
	if (decompressed_file == NULL) {
		printf("\n\nОшибка открытия распакованного файла (для записи).");

		flag = 0;
		goto cleanup;
	}

	clocks.start_total = clock();
	int success = decompress_file_v1(compressed_file, decompressed_file);
	clocks.stop_total = clock();

	if (!success) {
		printf("\nОшибка распаковки файла");

		flag = 0;
		goto cleanup;
	}

	if (fflush(decompressed_file) == EOF) {
		perror("Ошибка сброса данных на диск");

		flag = 0;
		goto cleanup;
	}

	if (!result_analysis(compressed_file, decompressed_file, &clocks, RUN_DECOMPRESS, stats_decompress)) {
		printf("\nОшибка вывода аналитики");
		
		flag = 0;
	}

cleanup:

	if (compressed_file)	fclose(compressed_file);
	if (decompressed_file)	fclose(decompressed_file);

	return flag;
}

static int compare_test_files(const char source_path[MAX_PATH_LEN], const char decompress_path[MAX_PATH_LEN]) {
	FILE* source = fopen(source_path, "rb");
	FILE* decompress_file = fopen(decompress_path, "rb");

	int flag = 1;

	if (!source) {
		printf("\nОшибка открытия исходного файла");

		flag = 0;
		goto cleanup;
	}
	if (!decompress_file) {
		printf("\nОшибка открытия распакованного файла");

		flag = 0;
		goto cleanup;
	}

	char buf_source[READ_BUFFER_SIZE] = { 0 };
	char buf_decompress[READ_BUFFER_SIZE] = { 0 };

	size_t count_source = 0;
	size_t count_decompress = 0;

	while ((count_source = fread(buf_source, sizeof(*buf_source), READ_BUFFER_SIZE, source)) > 0 &&
		(count_decompress = fread(buf_decompress, sizeof(*buf_decompress), READ_BUFFER_SIZE, decompress_file)) > 0) 
	{
		if (count_source != count_decompress) {
			printf("\nОшибка: размер прочитанных данных не совпадает");
			
			flag = 0;
			goto cleanup;
		}

		if (memcmp(buf_source, buf_decompress, count_source)) {
			printf("\nОшибка: данные в исходном и распакованном файлах различны");

			flag = 0;
			goto cleanup;
		}
	}


cleanup:
	if (source)				fclose(source);
	if (decompress_file)	fclose(decompress_file);

	return flag;
}

/*
	@brief Запуск автотеста. 
	@brief ПРИМЕЧАНИЕ: расширения гарантируются корректными (до 32 символов, всегда существует точка-разделитель)
	@return успех - 1, иначе - 0
*/
int run_autotest_files(void) {
	char data_path[MAX_PATH_LEN] = { 0 };
	int written = snprintf(data_path, MAX_PATH_LEN, "%s%s", AUTOTEST_DIRECTORY, AUTOTEST_DATA_FILENAME);

	if (written < 0 || written > MAX_PATH_LEN) {
		printf("\nОшибка создания файла для записи данных автотеста");
		return 0;
	}

	FILE* data_autotest = fopen(data_path, "w");
	if (!data_autotest) {
		printf("\nОшибка открытия файла для записи данных автотесты");
		return 0;
	}

	int flag = 1;
	
	fprintf(data_autotest, "sep=,\n");
	fprintf(data_autotest, "id,extension,content_type,source_size,compressed_size,freq_time,bincode_time,compress_time,decompress_time,is_identical,unique_symbols\n");

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

			// создаем пути для файлов (исходник, сжатый, распакованный)
			char source_path[MAX_PATH_LEN] = { 0 };
			char compress_path[MAX_PATH_LEN] = { 0 };
			char decompress_path[MAX_PATH_LEN] = { 0 };

			char source_extension[MAX_EXT_LENGTH] = { 0 };

			// создаём файл
			if (!create_test_random_file(idx, file_len, type, source_path, source_extension)) {
				printf("\nОшибка создания тестового файла");

				flag = 0;
				goto cleanup;
			}

			// создаём путь к сжатому файлу
			if (!create_test_compress_path(source_path, compress_path)) {
				printf("\nОшибка создания пути для сжатого файла");

				flag = 0;
				goto cleanup;
			}

			AlgorithmEfficiency_s stats_compress = { 0 };

			// printf("\n[DEBUG] source_path: '%s'", source_path);
			// printf("\n[DEBUG] compress_path: '%s'", compress_path);

			size_t used_syms = 0;

			// сжимаем файл
			if (!compress_test_file(source_path, compress_path, &stats_compress, &used_syms)) {
				printf("\nОшибка сжатия тестового файла");
				
				flag = 0;
				goto cleanup;
			}

			// создаём путь для распакованного файла
			create_test_decompress_path(source_path, decompress_path);

			AlgorithmEfficiency_s stats_decompress = { 0 };

			// распаковываем файл
			if (!decompress_test_file(compress_path, decompress_path, &stats_decompress)) {
				printf("\nОшибка распаковки тестового файла");
				
				flag = 0;
				goto cleanup;
			}

			DataAnalytics_s data = { 0 };

			// СБОР ДАННЫХ


			/*
				size_t test_idx;                             // номер теста (idx)
				char extension[MAX_EXT_LENGTH];              // расширение файла (в формате ".<extension>")
				TypeFileContent_e content_type;              // тип содержания (enum)

				size_t source_size;                          // размер исходника (байт)
				size_t compressed_size;                      // размер архива (байт)

				double freq_time;							 // время заполнения массива частот (сек.)
				double bincode_time;						 // время заполнения таблицы бинарных кодов (сек.)
				double compress_time;                        // время сжатия (сек.)
				double decompress_time;                      // время распаковки (сек.)

				int is_identical;                            // результат проверки (0/1)
				size_t unique_symbols;                       // число уникальных символов в файле (0-256)
			*/
			data.test_idx = idx;
			snprintf(data.extension, MAX_EXT_LENGTH, "%s", source_extension);
			data.content_type = type;

			data.source_size = stats_compress.in_size;
			data.compressed_size = stats_compress.out_size;

			data.freq_time = stats_compress.freq_time;
			data.bincode_time = stats_compress.bincode_time;
			data.compress_time = stats_compress.compress_time;
			data.decompress_time = stats_decompress.total_time;

			data.unique_symbols = used_syms;

			if (!compare_test_files(source_path, decompress_path)) {
				printf("\nОшибка: исходный файл №%zu не совпадает со своей распакованной копией", idx);
				idx++;
				data.is_identical = 0;

				flag = 0;
				continue; // не удаляем файлы для исследования после стресс-теста
			}
			printf("\nФайл №%zu успешно прошел тест.", idx);

			data.is_identical = 1;

			fprintf(data_autotest, "%zu,%s,%d,%zu,%zu,%.3lf,%.3lf,%.3lf,%.3lf,%d,%zu\n", 
				data.test_idx, 
				data.extension, 
				data.content_type, 
				data.source_size, 
				data.compressed_size, 
				data.freq_time, 
				data.bincode_time, 
				data.compress_time, 
				data.decompress_time, 
				data.is_identical, 
				data.unique_symbols);

			if (remove(source_path)) {
				perror(source_path);
				
				flag = 0;
				goto cleanup;
			}
			if (remove(compress_path)) {
				perror(compress_path);
				
				flag = 0;
				goto cleanup;
			}
			if (remove(decompress_path)) {
				perror(decompress_path);
				
				flag = 0;
				goto cleanup;
			}

			idx++;
		}
	}

cleanup:
	if (data_autotest) fclose(data_autotest);
	return flag;
}