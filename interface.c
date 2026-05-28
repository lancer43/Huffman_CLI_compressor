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

#define MAX_PATH_LEN			1024
#define MAX_FORMAT_LEN			10

#define COMPRESSED_EXTENSION	".huf"
#define LEN_COMP_EXT			4

enum {
	exit,
	run_compress,
	run_decompress
};

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
				extension_flag = PATH_VALID_NO_EXTENSION;
				continue;
			}
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
		return;
	}
	if (compressed_file == NULL) {
		printf("\n\nОшибка открытия сжатого файла (для записи)");
		return;
	}

	printf("\n\nПодсчёт частоты каждого символа...");
	size_t freq_count[ASCII_ALP_SIZE] = { 0 };
	int success = frequency_counting(source, freq_count);
	if (!success) {
		printf("\nПодсчет частоты неудачный");
		return;
	}
	printf("\nЧастота посчитана успешно!");

	// [DEBUG]: смотрим частоты
	/*
	for (size_t i = 0; i < ASCII_ALP_SIZE; i++) {
		printf("\n[DEBUG]: arr[%zu] = %zu", i, freq_count[i]);
	}
	*/

	CodeTable table = { 0 };

	printf("\n\nЗаполняем таблицу бинарных кодов для сжатия...");
	success = coding_symbols(freq_count, &table);
	if (!success) {
		printf("\nОшибка заполнения таблицы кодов");
	}
	printf("\nТаблица кодов успешно заполнена!");

	// [TODO]: это можно встроить в compress_file_v1()? по сути ее область ответственности
	fseek(source, 0, SEEK_SET);

	printf("\n\nСжимаем файл...");
	success = compress_file_v1(source, compressed_file, freq_count, &table);
	if (!success) {
		printf("\nОшибка сжатия файла");
		return;
	}

	printf("\nФайл сжат успешно!");
	printf("\n\nСжатый файл лежит по пути: %s", output_path);

	fclose(source);
	fclose(compressed_file);
}

static void run_decompress_file(const char* input_path, const char* output_path) {
	FILE* compressed_file = fopen(input_path, "rb");
	FILE* decompressed_file = fopen(output_path, "wb");

	if (compressed_file == NULL) {
		printf("\n\nОшибка открытия сжатого файла (для чтения)");
		return;
	}
	if (decompressed_file == NULL) {
		printf("[DEBUG] output_path: '%s'", output_path);
		printf("\n\nОшибка открытия распакованного файла (для записи). Код: %d\n", errno);
		perror(output_path);
		return;
	}

	printf("\n\nРаспаковываем файл...");
	int success = decompress_file_v1(compressed_file, decompressed_file);
	if (!success) {
		printf("\nОшибка распаковки файла");
		return;
	}
	printf("\nФайл распакован успешно!");
	printf("\n\nРаспакованный файл лежит по пути: %s", output_path);

	fclose(compressed_file);
	fclose(decompressed_file);
}

/*
	@brief Функция подготовки пути для сжатого файла и его сжатие
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

static void decompress_interface(const char* input_path) {
	char format[MAX_FORMAT_LEN];
	printf("\nВведите формат файла для распаковки (без точки): ");
	while (fgets(format, MAX_FORMAT_LEN, stdin) == NULL) {
		printf("\nОшибка записи формата. Повторите попытку.");
		printf("\nВведите формат файла для распаковки: ");
	}

	format[strcspn(format, "\n")] = '\0';

	char output_path[MAX_PATH_LEN];
	strncpy(output_path, input_path, MAX_PATH_LEN);

	// 1 - под '\0'
	if (strlen(output_path) - huf_format_len + strlen(format) + 1 > MAX_PATH_LEN) {
		printf("\nОшибка: путь слишком длинный");
		return;
	}

	char* formatter = strrchr(output_path, '.');
	formatter[1] = '\0';

	strncat(output_path, format, MAX_FORMAT_LEN);
	printf("[DEBUG] Путь для распакованного файла: %s", output_path);

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