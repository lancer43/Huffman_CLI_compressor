#define _CRT_SECURE_NO_WARNINGS
#include "interface.h"

// модули которыми мы управляем
#include "compressor.h"
#include "decompressor.h"

#include <stdio.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define MAX_PATH_LEN			1024
#define MAX_FORMAT_LEN			10

#define COMPRESSED_EXTENSION	".huf"
#define LEN_COMP_EXT			4

typedef enum {
	exit,
	run_compress,
	run_decompress
} Mode_e;

typedef enum {
	PATH_VALID_NO_EXTENSION,
	PATH_VALID_WITH_EXTENSION
} PathStatus_e;

static void greeting_user(void) {
	printf("========== АРХИВАТОР ЗАПУЩЕН ==========");
	printf("\n\nДобро пожаловать в архиватор!");
}

static void show_menu(void) {
	printf("\n\n================ МЕНЮ ================");

	printf("\n\nВыберите действие:");
	printf("\n1. Сжать файл\
			\n2. Распаковать файл\
			\n0. Выход");
}
/*
	@brief Вывод анализа работы архиватора
	@param time - время сжатия/распаковки
	@param coefficient - коэффициент сжатия
	@param in_size - размер входного файла (в байтах)
	@param out_size - размер выходного файла (в байтах)
	@param mode - режим работы архиватора (сжатие/распаковка)
*/
static void show_analysis(const double time, const double coefficient, const size_t* in_size, const size_t* out_size, const Mode_e mode) {
	assert(mode == run_compress || mode == run_decompress);
	assert(in_size != NULL && out_size != NULL);
	
	switch (mode) {
	case run_compress:
		printf("\n\n========== АНАЛИЗ СЖАТИЯ ФАЙЛА ==========");

		printf("\n\nРазмер исходного файла: %zu байт", *in_size);
		printf("\nРазмер сжатого файла: %zu байт", *out_size);
		printf("\nКоэффициент сжатия файла равен %.2lf", coefficient);
		
		
		printf("\n\nВремя сжатия файла составило %.3lf сек.", time);
		break;

	case run_decompress:
		printf("\n\n========== АНАЛИЗ РАСПАКОВКИ ФАЙЛА ==========");

		printf("\n\nРазмер сжатого файла: %zu байт", *in_size);
		printf("\nРазмер распакованного файла: %zu байт", *out_size);
		printf("\nКоэффициент сжатия файла равен %.2lf", coefficient);


		printf("\n\nВремя распаковки файла составило %.3lf сек.", time);
		break;
	}
	
}

/*
	@brief Вычисление коэффициента сжатия файла и времени сжатия/распаковки
	@param istream - входной файл
	@param ostream - выходной файл
	@param start - начало замера времени
	@param stop - конец замера времени
	@param mode - режим работы (сжатие/распаковка)
*/
static void result_analysis(FILE* istream, FILE* ostream, clock_t start, clock_t stop, Mode_e mode) {
	assert(istream != NULL && ostream != NULL);
	assert(mode == run_compress || mode == run_decompress);

	double time = (double)(stop - start) / CLOCKS_PER_SEC;

	fseek(istream, 0, SEEK_END);
	fseek(ostream, 0, SEEK_END);

	size_t in_size = ftell(istream);
	size_t out_size = ftell(ostream);

	fseek(istream, 0, SEEK_SET);
	fseek(ostream, 0, SEEK_SET);

	double coefficient = 0.0;

	if (mode == run_compress) {
		coefficient = (double)in_size / out_size;
	}
	if (mode == run_decompress) {
		coefficient = (double)out_size / in_size;
	}

	show_analysis(time, coefficient, &in_size, &out_size, mode);
}

/*
	@brief Ввод пути файла для сжатия/распаковки
	@param input_path - строка для сохранения корректного пути
	@return возвращает флаг расширения файла (есть - 1, нет - 0)
*/
static PathStatus_e enter_path(char input_path[MAX_PATH_LEN]) {
	// главной функцией run_interface_huf() гарантируется строка-путь нужной длины
	assert(input_path != NULL);
	int valid_flag = 0;
	int extension_flag = -1;

	while (!valid_flag) {
		extension_flag = -1;
		printf("\nПеретащите файл в консоль (или введите путь к файлу вручную): ");

		// fgets() читает максимум MAX_PATH_LEN - 1 символов
		if (fgets(input_path, MAX_PATH_LEN, stdin) == NULL) {
			printf("\nОшибка чтения. Повторите попытку.");

			// чистим буфер
			int c;
			while ((c = getchar()) != '\n' && c != EOF);

			continue;
		}

		// если длинный путь
		if (strchr(input_path, '\n') == NULL && strlen(input_path) == MAX_PATH_LEN - 1) {
			printf("\nОшибка: путь слишком длинный. Повторите попытку");

			// чистим буфер
			int c;
			while ((c = getchar()) != '\n' && c != EOF);

			continue;
		}

		// если просто нажали Enter
		if (input_path[0] == '\n') {
			printf("\nПустой ввод. Повторите попытку.");
		}

		// заменяем последний символ на конец строки
		input_path[strcspn(input_path, "\n")] = '\0';

		// вспомогательный указатель
		char* ptr = NULL;

		// заменяем блок очистки пробелов, кавычек и слэшей:
		char* read_ptr = input_path;
		char* write_ptr = input_path;

		// пропускаем ведущие пробелы
		while (*read_ptr == ' ') read_ptr++;

		// копируем строку, на лету выкидывая кавычки и меняя слэши
		while (*read_ptr != '\0') {
			if (*read_ptr == '\"') {
				read_ptr++; // игнорируем кавычку
			}
			else {
				if (*read_ptr == '\\') {
					*write_ptr = '/'; // исправляем слэш
				}
				else {
					*write_ptr = *read_ptr; // копируем символ
				}
				write_ptr++;
				read_ptr++;
			}
		}
		*write_ptr = '\0'; // закрываем строку

		// отрезаем концевые пробелы (безопасно, без выхода за границы)
		if (write_ptr > input_path) {
			write_ptr--; // Встаем на последний символ перед '\0'
			while (write_ptr >= input_path && *write_ptr == ' ') {
				*write_ptr-- = '\0';
			}
		}

		// если введен просто текст
		char* format_point = strrchr(input_path, '.');
		ptr = strrchr(input_path, '/');
		
		if (format_point == NULL ||
			(ptr != NULL && format_point < ptr) ||
			format_point == ptr + 1 ||
			format_point == input_path) {
			printf("\nПредупреждение: формат файла неопределён. Отправьте любой символ, если хотите повторить ввод (иначе - Enter).\n");
			int c = getchar();

			if (c != '\n') {

				while ((c = getchar()) != '\n' && c != EOF);
				continue;
			}
			extension_flag = PATH_VALID_NO_EXTENSION;
		}

		// финальная железобетонная проверка
		FILE* test_open = fopen(input_path, "rb");
		if (test_open == NULL) {
			printf("\nДанный путь не существует. Повторите попытку.");
			continue;
		}

		fclose(test_open);

		valid_flag = 1;
		if (extension_flag == -1) {
			extension_flag = PATH_VALID_WITH_EXTENSION;
		}
	}
	return extension_flag;
}

/*
	@brief Функция полного цикла сжатия файла
	@param input_path - путь сжимаемого файла
	@param output_path - путь сжатого файла
*/
static void run_compress_file(const char* input_path, const char* output_path) {
	FILE* source = fopen(input_path, "rb");
	FILE* compressed_file = fopen(output_path, "wb");

	if (source == NULL) {
		printf("\n\nОшибка открытия исходного файла (для чтения)");
		
		goto cleanup;
	}
	if (compressed_file == NULL) {
		printf("\n\nОшибка открытия сжатого файла (для записи)");
		
		goto cleanup;
	}

	clock_t start = clock();

	printf("\n\nПодсчёт частоты каждого символа...");
	size_t freq_count[ASCII_ALP_SIZE] = { 0 };
	int success = frequency_counting(source, freq_count);
	if (!success) {
		printf("\nПодсчет частоты неудачный");
		
		goto cleanup;
	}
	printf("\nЧастота посчитана успешно!");

	printf("\n\nЗаполняем таблицу бинарных кодов для сжатия...");
	CodeTable table = { 0 };
	success = coding_symbols(freq_count, &table);
	if (!success) {
		printf("\nОшибка заполнения таблицы кодов");

		goto cleanup;
	}
	printf("\nТаблица кодов успешно заполнена!");

	printf("\n\nСжимаем файл...");
	success = compress_file_v1(source, compressed_file, freq_count, &table);
	if (!success) {
		printf("\nОшибка сжатия файла");
		
		goto cleanup;
	}
	clock_t stop = clock();

	printf("\nФайл сжат успешно!");
	printf("\n\nСжатый файл лежит по пути: '%s'", output_path);

	fflush(compressed_file);
	result_analysis(source, compressed_file, start, stop, run_compress);

cleanup:

	if (source)				fclose(source);
	if (compressed_file)	fclose(compressed_file);
}

/*
	@brief Функция полного цикла распаковки файла
	@param input_path - путь сжимаемого файла
	@param output_path - путь сжатого файла
*/
static void run_decompress_file(const char* input_path, const char* output_path) {
	FILE* compressed_file = fopen(input_path, "rb");
	FILE* decompressed_file = fopen(output_path, "wb");

	if (compressed_file == NULL) {
		printf("\n\nОшибка открытия сжатого файла (для чтения)");
		
		goto cleanup;
	}
	if (decompressed_file == NULL) {
		printf("\n\nОшибка открытия распакованного файла (для записи).");
		
		goto cleanup;
	}

	printf("\n\nРаспаковываем файл...");

	clock_t start = clock();
	int success = decompress_file_v1(compressed_file, decompressed_file);
	if (!success) {
		printf("\nОшибка распаковки файла");
		
		goto cleanup;
	}
	clock_t stop = clock();

	printf("\nФайл распакован успешно!");
	printf("\n\nРаспакованный файл лежит по пути: %s", output_path);

	fflush(decompressed_file);
	result_analysis(compressed_file, decompressed_file, start, stop, run_decompress);
cleanup:

	if (compressed_file)	fclose(compressed_file);
	if (decompressed_file)	fclose(decompressed_file);
}

/*
	@brief Функция создания пути для сжатого файла и его сжатие
	@param input_path - путь исходного файла
*/
static void compress_interface(const char* input_path, int extension_flag) {
	// на вход подаётся корректный путь к файлу (гарантируется enter_path())

	// создаём путь для записи сжатого файла
	char output_path[MAX_PATH_LEN];
	snprintf(output_path, sizeof(output_path), "%s", input_path);

	// если расширение у файла было, то удаляем его
	if (extension_flag == PATH_VALID_WITH_EXTENSION) {
		char* point = strrchr(output_path, '.');
		*point = '\0';
	}

	// проверяем хватит ли места на формат + символ '\0'
	if (strlen(output_path) + LEN_COMP_EXT + 1 > MAX_PATH_LEN) {
		printf("\nОшибка: путь к файлу слишком длинный.");
		return;
	}

	// добавляем формат архиватора
	strncat(output_path, COMPRESSED_EXTENSION, LEN_COMP_EXT);
	
	// тут щас будем выделять память, таблицу все дела пупупу
	run_compress_file(input_path, output_path);
}

/*
	@brief Функция создания пути для распакованного файла и его распаковка
	@param input_path - путь сжатого файла
*/
static void decompress_interface(const char* input_path) {
	// на вход подаётся корректный путь к файлу (гарантируется enter_path())
	
	// проверка на нужный формат
	char* point = strrchr(input_path, '.');
	if (point == NULL || strcmp(point, COMPRESSED_EXTENSION) != 0) {
		printf("\nОшибка: неверный формат файла.");
		return;
	}
	point = NULL;
	

	// [TODO] внести информацию о формате исходного файла в сам алгоритм сжатия (когда замерджим ветку с главной)
	// вытекает проблема в будущем: если перенесем информацию о формате исходника в decompress_file_v1(), то 
	// мы не сможем тут проверить длину заранее.
	// придется либо отдельно вызывать файловый поток чтобы читать часть оверхеда здесь вручную, либо добавлять отдельную функцию
	// что то вроде read_overhead_file(), юзать ее здесь, но тогда и править алгоритм decompress_file_v1().
	char format[MAX_FORMAT_LEN];

	printf("\nВведите формат файла для распаковки (с точкой): ");
	while (fgets(format, MAX_FORMAT_LEN, stdin) == NULL) {
		printf("\nОшибка записи формата. Повторите попытку.");
		printf("\nВведите формат файла для распаковки (с точкой): ");

		// чистим буфер
		int c;
		while ((c = getchar()) != '\n' && c != EOF);
	}

	format[strcspn(format, "\n")] = '\0';

	// все что выше удалю потом! (выполнение [TODO])
	char output_path[MAX_PATH_LEN];
	snprintf(output_path, sizeof(output_path), "%s", input_path);

	// убираем формат с точкой
	point = strrchr(output_path, '.');
	assert(point != NULL);
	*point = '\0';
	
	// проверка длины
	if (strlen(output_path) + strlen(format) + 1 > MAX_PATH_LEN) {
		printf("\nОшибка: путь слишком длинный");
		return;
	}

	strncat(output_path, format, strlen(format));

	run_decompress_file(input_path, output_path);
}


// ========== ГЛАВНАЯ ФУНКЦИЯ МОДУЛЯ ==========

void run_interface_huf(void) {
	
	greeting_user();

	while (1) {
		show_menu();
		int choice = -1;
		int flag = 0;

		printf("\n\nВвод: ");
		if (scanf("%d", &choice) != 1) {
			printf("\nНеверный ввод. Повторите попытку.");

			// очистка буфера
			while (getchar() != '\n');

			continue;
		}

		// очистка буфера
		while (getchar() != '\n');

		switch (choice) {
		
		case run_compress: {
			char path[MAX_PATH_LEN];
			
			int extension_flag = enter_path(path);

			compress_interface(path, extension_flag);

			break;
		}
		case run_decompress: {
			char path[MAX_PATH_LEN];

			enter_path(path);

			decompress_interface(path);

			break;
		}
		case exit:
			printf("\nВыход...");
			flag = 1;
			break;
		
		default:
			printf("\nНекорректный ввод. Повторите попытку.");
			break;
		}

		if (flag) break;
	}
}