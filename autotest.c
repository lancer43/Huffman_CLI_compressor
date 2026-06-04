#include "autotest.h"

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

/*
	Тип файла по содержанию. Необходимо для наиболее объективного результата
	тестирования программы.
*/
typedef enum {
	CONTENT_EMPTY,		// пустой файл
	CONTENT_ONE_BYTE,	// файл размером 1 байт
	CONTENT_MONO_SYM,	// файл из одного символа случайной длины
	CONTENT_TWO_SYMS,	// файл случайной длины из двух случайных символов
	CONTENT_RANDOM,		// файл с равномерным распределением символов (длина случайна)
	CONTENT_UNEVEN,		// файл с неравномерным распределением символов
	CONTENT_ASCII		// файл с гарантией использования каждого символа ASCII хотя бы 1 раз (распределение равномерное)
} TypeFileContent_e;


/*
	Для тестирования предлагается использовать 7 типов файлов (см. TypeFileContent_e)
	Здесь типы распределены по количеству на 100 тестовых файлов

	Это сделано для того, чтобы более качественно оценить сложные файлы, и при этом зазря не
	тратить циклы ради проверки очевидно верных файлов (например нет смысла 30 раз запускать
	цикл сжатия-распаковки пустого или однобайтного файла, в отличие от равномерного и
	неравномерного распределений)
*/
typedef enum {
	WEIGHT_EMPTY = 5,
	WEIGHT_ONE_BYTE = 5,
	WEIGHT_MONO_SYM = 10,
	WEIGHT_TWO_SYMS = 10,
	WEIGHT_RANDOM = 30,
	WEIGHT_UNEVEN = 30,
	WEIGHT_ASCII = 10
} TypeWeight_e;

// флаг совместимости .csv файла с Excel (чтобы не настраивать разделитель вручную в Excel)
typedef enum {
	EXCEL_INCOMPATIBILITY,
	EXCEL_COMPATIBILITY
} ExcelComp_e;

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
} DataAnalytics_s;


/*
	@brief Создание расширения для тестового файла
	@param extention строка для формирования расширения
*/
static void create_test_extension(char extension[MAX_EXT_LENGTH]) {
	// минимальная длина: 3 символа (например, ".x\0")
	// максимальная длина: MAX_EXT_LENGTH (32 символа, индексы 0..31)
	size_t ext_len = 3 + rand() % (MAX_EXT_LENGTH - 3 - DECOMPRESS_EXT_INDICATOR);

	char acceptable_chars[] = ACCEPTABLE_EXT_CHARS;
	size_t chars_len = strlen(acceptable_chars);

	extension[0] = '.'; 

	for (size_t count = 1; count < ext_len - 1; count++) { // оставляем последний байт под '\0'
		
		char symbol = acceptable_chars[rand() % chars_len];

		extension[count] = symbol;
	}

	extension[ext_len - 1] = '\0';
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
	@brief Создание пути к распакованному файлу (просто добавляется постфикс "_d" к исходному расширению)
	@param source_path - путь к исходному файлу
	@param decompress_path - путь к распакованному файлу
	@return успех - 1, иначе - 0
*/
static int create_test_decompress_path(const char source_path[MAX_PATH_LEN], char decompress_path[MAX_PATH_LEN]) {
	int written = snprintf(decompress_path, MAX_PATH_LEN, "%s%s", source_path, "_d");

	if (written < 0 || written > MAX_PATH_LEN) return 0;

	return 1;
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
	@brief Создание пути для сохранения данных автотеста
	@return успех - 1, иначе - 0
*/
static int create_data_path(char data_path[MAX_PATH_LEN]) {
	int written = snprintf(data_path, MAX_PATH_LEN, "%s%s", AUTOTEST_DIRECTORY, AUTOTEST_DATA_FILENAME);

	if (written < 0 || written > MAX_PATH_LEN) {
		return 0;
	}

	return 1;
}

/*
	@brief Запись первой строки таблицы в .csv файл (имена столбцов)
	@param data - файл для записи
	@param flag - флаг совместимости .csv файла с Excel таблицами.
	EXCEL_COMPATIBILITY - при открытии автоматическое разделение по столбцам через разделитель ','
	@return успех - 1, иначе - 0
*/
static int write_data_header(FILE* data, ExcelComp_e flag) {
	
	if (flag == EXCEL_COMPATIBILITY) {
		if (fprintf(data, "sep=,\n") < 0) return 0;
	}

	if (fprintf(data, "id,extension,content_type,source_size,compressed_size,freq_time,bincode_time,\
compress_time,decompress_time,is_identical\n") < 0) 
		return 0;

	return 1;
}

/*
	@brief Запись данных в .csv файл
	@param stream_data - .csv файл для записи
	@param data - структура для записи данных
	@param idx - порядковый номер файла
	@param type - тип содержания
	@param success - результат проверки (успех - 1/неудача - 0)
	@param stats_compress - структура с данными сжатия
	@param stats_decompress - структура с данными распаковки
	@param source_extension - расширение исходного файла
	@return успех - 1, иначе - 0
*/
static int write_csv(
	FILE* stream_data,
	DataAnalytics_s* data, 
	size_t idx, 
	TypeFileContent_e type,
	int success,
	AlgorithmEfficiency_s* stats_compress,
	AlgorithmEfficiency_s* stats_decompress,
	const char source_extension[MAX_EXT_LENGTH]
	) {
	// формирование структуры
	data->test_idx = idx;

	int written = snprintf(data->extension, MAX_EXT_LENGTH, "%s", source_extension);
	if (written < 0 || written > MAX_EXT_LENGTH) return 0;
	
	data->content_type = type;

	data->source_size = stats_compress->in_size;
	data->compressed_size = stats_compress->out_size;

	data->freq_time = stats_compress->freq_time;
	data->bincode_time = stats_compress->bincode_time;
	data->compress_time = stats_compress->compress_time;
	data->decompress_time = stats_decompress->total_time;

	data->is_identical = success;

	// запись в файл
	written = fprintf(stream_data, "%zu,%s,%d,%zu,%zu,%.3lf,%.3lf,%.3lf,%.3lf,%d\n",
		data->test_idx,
		data->extension,
		data->content_type,
		data->source_size,
		data->compressed_size,
		data->freq_time,
		data->bincode_time,
		data->compress_time,
		data->decompress_time,
		data->is_identical);

	if (written < 0) return 0;

	return 1;
}

/*
	@brief Одиночный цикл тестирования файла.
	@brief Происходит создание путей к файлам, генерация файла по заданному содержимому, сжатие и распаковка.
	После распаковки полученный файл сравнивается с исходным и полученные данные записываются в .csv файл
	@param stream_data - .csv файл для записи данных
	@param idx - порядковый номер тестового файла
	@param file_len - длина исходного файла в байтах
	@param type - тип содержания тестового файла
	@return если тест отработал штатно - 1, иначе - 0 
	[ПРИМЕЧАНИЕ] под "отработал штатно" подразумевается то, что не было аварийных выходов из функций. Т.е. 
	даже если исходный и распакованный файл не сошлись, функция всё равно вернет 1.
*/
static int execute_single_test(FILE* stream_data, size_t idx, size_t file_len, TypeFileContent_e type) {
	// создаем пути для файлов (исходник, сжатый, распакованный)
	char source_path[MAX_PATH_LEN] = { 0 };
	char compress_path[MAX_PATH_LEN] = { 0 };
	char decompress_path[MAX_PATH_LEN] = { 0 };

	// строка для расширения исходного файла
	char source_extension[MAX_EXT_LENGTH] = { 0 };

	// временные структуры для хранения данных о сжатии и распаковке
	AlgorithmEfficiency_s stats_compress = { 0 };
	AlgorithmEfficiency_s stats_decompress = { 0 };

	// структура для передачи данных в .csv файл
	DataAnalytics_s data = { 0 };

	// создаём файл
	if (!create_test_random_file(idx, file_len, type, source_path, source_extension)) return 0;

	// создаём путь к сжатому файлу
	if (!create_compress_path(source_path, compress_path, PATH_VALID_WITH_EXTENSION)) return 0;

	// сжимаем файл
	if (!run_compress(source_path, compress_path, &stats_compress, PATH_VALID_WITH_EXTENSION)) return 0;

	// создаём путь для распакованного файла
	if (!create_test_decompress_path(source_path, decompress_path)) return 0;

	// распаковываем файл
	if (!run_decompress(compress_path, decompress_path, &stats_decompress)) return 0;

	// сверяем исходный файл и распакованный
	if (!compare_test_files(source_path, decompress_path)) {
		printf("\nОшибка: исходный файл №%zu не совпадает со своей распакованной копией", idx);
		// если не совпали - записываем неудачу и двигаемся дальше (файл не удаляем!)
		if (!write_csv(stream_data, &data, idx, type, 0, &stats_compress, &stats_decompress, source_extension))
			return 0;

		// здесь важно что технически ничего не упало, и даже если файлы не сошлись - компьютер отработал строго по заданному коду
		return 1;
	}

	if (!write_csv(stream_data, &data, idx, type, 1, &stats_compress, &stats_decompress, source_extension))
		return 0;
	printf("\nФайл №%zu успешно прошел тест.", idx);

	// чистим за собой память на диске
	if (remove(source_path)) {
		perror(source_path);
		return 0;
	}
	if (remove(compress_path)) {
		perror(compress_path);
		return 0;
	}
	if (remove(decompress_path)) {
		perror(decompress_path);
		return 0;
	}

	return 1;
}

/*
	@brief Запуск автотеста. 
	@brief 
	@brief ПРИМЕЧАНИЕ: расширения гарантируются корректными (до 32 символов, всегда существует точка-разделитель)
	@return успех - 1, иначе - 0
*/
int run_autotest(void) {
	// создаём путь к .csv файлу для сбора данных 
	char data_path[MAX_PATH_LEN] = { 0 };
	
	if (!create_data_path(data_path)) return 0;

	// открываем файл на запись
	FILE* data_autotest = fopen(data_path, "w");
	if (!data_autotest) {
		printf("\nОшибка открытия файла для записи данных автотесты");
		return 0;
	}

	// записываем первую строку (имена столбцов)
	if (!write_data_header(data_autotest, EXCEL_COMPATIBILITY)) goto cleanup;

	size_t weight = 0; // переменная для разграничения файлов по типам в конкретных пропорциях на 100 файлов
	size_t idx = 0; // номер файла

	// массив "весов" - количества каждого типа файла в каждой сотне тестовых файлов
	TypeWeight_e weights[] = {
		WEIGHT_EMPTY,
		WEIGHT_ONE_BYTE,
		WEIGHT_MONO_SYM,
		WEIGHT_TWO_SYMS,
		WEIGHT_RANDOM,
		WEIGHT_UNEVEN,
		WEIGHT_ASCII,
	};

	// цикл по всем 7 типам содержания файлов (каждый тип вызывается ровно weights[type] раз)
	for (TypeFileContent_e type = 0; type < NUMBER_OF_TYPES; type++) {
		weight += (size_t)weights[type];
		
		while (idx < weight) {
			size_t file_len = 0;

			// частные случаи и общий
			if (type == CONTENT_EMPTY) {
				file_len = 0;
			}
			else if (type == CONTENT_ONE_BYTE) {
				file_len = 1;
			}
			else {
				file_len = create_test_random_file_size();
			}

			if (!execute_single_test(data_autotest, idx, file_len, type)) goto cleanup;

			idx++;
		}
	}

	fclose(data_autotest);
	return 1;

cleanup:
	if (data_autotest) fclose(data_autotest);
	return 0;
}