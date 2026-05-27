#define _CRT_SECURE_NO_WARNINGS
#include "interface.h"

// модули которыми мы управляем
#include "compressor.h"
#include "decompressor.h"

#include <stdio.h>
#include <errno.h>
#include <locale.h>
#include <string.h>

#define MAX_PATH_LEN 1024
#define MAX_FORMAT_LEN 10

enum {
	exit,
	run_compress,
	run_decompress
};

enum {
	huf_format_len = 3
};

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
*/
static void enter_path(char* input_path) {
	printf("\nПеретащите файл в консоль (или введите путь к файлу вручную): ");

	while (fgets(input_path, MAX_PATH_LEN, stdin) == NULL) {
		printf("\nОшибка записи пути к файлу. Повторите попытку.");
		printf("\nПеретащите файл в консоль (или введите путь к файлу вручную): ");
	};


	// заменяем последний символ на конец строки
	input_path[strcspn(input_path, "\n")] = '\0';

	// очищаем файл от кавычек по бокам
	char* quote_ptr = strchr(input_path, '\"');
	while (quote_ptr != NULL) {
		memmove(quote_ptr, &quote_ptr[1], strlen(quote_ptr));

		quote_ptr = strchr(input_path, '\"');
	}

	char* slash = strchr(input_path, '\\');
	while (slash != NULL) {
		*slash = '/';
		slash = strchr(input_path, '\\');
	}
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
static void compress_interface(const char* input_path) {

	// резервируем последние 3 символа под формат .huf
	if (strlen(input_path) > MAX_PATH_LEN - huf_format_len) {
		printf("\nОшибка: путь к файлу слишком длинный.");
		return;
	}

	// создаём путь для записи сжатого файла
	char output_path[MAX_PATH_LEN];

	strncpy(output_path, input_path, MAX_PATH_LEN);
	
	char* formatter = strrchr(output_path, '.');
	// [TODO]: можно переделать проверку на корректность пути более "умно"
	if (formatter == NULL) {
		printf("Ошибка: введён некорректный формат файла");
		return;
	}

	formatter[1] = '\0';
	strncat(formatter, "huf", huf_format_len);

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
			
			enter_path(path);

			

			compress_interface(path);

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